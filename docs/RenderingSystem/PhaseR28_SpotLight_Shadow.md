# Phase R28：聚光灯阴影（Spot Light Shadow）

> **创建日期**：2026-05-15
> **前置依赖**：R27（Shadow Atlas 基础架构）
> **目标**：实现聚光灯的阴影投射，作为 Shadow Atlas 架构的首个验证用例
> **后续文档**：R29（点光源阴影）

---

## 一、概述

### 1.1 为什么先实现聚光灯阴影

1. **技术最简单**：透视投影 Shadow Map，与方向光原理相同但不需要 CSM
2. **单个 Tile**：每个聚光灯只需要 Atlas 中的 1 个 Tile
3. **复用现有 Shader**：可以复用 `Shadow.vert/frag`（仅需 `u_LightSpaceMatrix` + `u_ObjectToWorldMatrix`）
4. **验证 Atlas 架构**：验证 Scissor Test、UV 变换、Atlas 采样的正确性
5. **不需要修改 Framebuffer**：使用现有 `DEPTH_COMPONENT` 格式

### 1.2 聚光灯阴影原理

聚光灯有明确的锥形范围（由 `InnerCutoffAngle` 和 `OuterCutoffAngle` 定义），使用**透视投影**即可覆盖其照射区域：

```
        光源位置
           *
          /|\
         / | \
        /  |  \    ← OuterCutoffAngle
       /   |   \
      /    |    \
     /     |     \
    ───────────────  ← 远平面（= Range）
```

- **FOV** = `OuterCutoffAngle * 2`（覆盖整个外锥角）
- **近平面** = 0.1（避免近处裁剪）
- **远平面** = `Range`（超出范围的物体不投射阴影）
- **宽高比** = 1.0（Shadow Map 是正方形）

---

## 二、Light Space Matrix 计算

### 2.1 方案对比

#### 方案 A：使用 OuterCutoffAngle 作为 FOV（推荐 ?????）

```cpp
float fov = glm::acos(light.OuterCutoff) * 2.0f;
```

**优点**：
- 精确覆盖聚光灯的照射范围
- 不浪费 Shadow Map 分辨率

**缺点**：
- 当 OuterCutoffAngle 很小时（如 5°），Shadow Map 覆盖范围小，可能出现边缘裁剪

#### 方案 B：使用固定 90° FOV

```cpp
float fov = glm::radians(90.0f);
```

**优点**：
- 简单，不依赖光源参数
- 覆盖范围大，不会出现边缘裁剪

**缺点**：
- 浪费 Shadow Map 分辨率（大部分区域不在聚光灯范围内）
- 阴影质量下降

#### 方案 C：OuterCutoffAngle + 安全边距（推荐 ????）

```cpp
float fov = glm::acos(light.OuterCutoff) * 2.0f + glm::radians(5.0f);  // 额外 5° 边距
```

**优点**：
- 避免边缘裁剪
- 分辨率利用率较高

**缺点**：
- 略微浪费分辨率

### 2.2 推荐实现（方案 A）

方案 A 最精确，且当前项目中 `OuterCutoffAngle` 默认为 17.5°，不会出现极端情况。如果后续发现边缘问题，可以升级为方案 C。

```cpp
// Renderer3D.cpp 中 BeginScene() 新增

/// <summary>
/// 计算聚光灯的 Light Space Matrix（透视投影）
/// </summary>
/// <param name="light">聚光灯 GPU 数据</param>
/// <param name="position">聚光灯世界空间位置</param>
/// <returns>Light Space VP 矩阵</returns>
static glm::mat4 CalcSpotLightMatrix(const SpotLightData& light, const glm::vec3& position)
{
    // FOV = OuterCutoffAngle * 2
    // OuterCutoff 存储的是 cos(OuterCutoffAngle)，需要反算角度
    float halfFov = glm::acos(light.OuterCutoff);
    float fov = halfFov * 2.0f;

    float aspectRatio = 1.0f;       // Shadow Map 是正方形
    float nearPlane = 0.1f;
    float farPlane = light.Range;

    glm::mat4 projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);

    // 视图矩阵：从光源位置看向光照方向
    glm::vec3 direction = glm::normalize(light.Direction);
    glm::vec3 target = position + direction;

    // 处理 up 向量退化（光照方向接近垂直时）
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(direction, up)) > 0.99f)
    {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    glm::mat4 view = glm::lookAt(position, target, up);

    return projection * view;
}
```

---

## 三、ShadowPass 重构

### 3.1 方案对比

#### 方案 A：在现有 ShadowPass 中新增聚光灯渲染分支（推荐 ?????）

在 `ShadowPass::Execute()` 中，CSM 渲染完成后，新增聚光灯渲染循环。

**优点**：
- 改动最小，复用现有 Pass 框架
- 所有阴影渲染集中在一个 Pass 中，逻辑清晰
- 与现有 `RenderPipeline` 分组机制兼容

**缺点**：
- `ShadowPass::Execute()` 会变长

#### 方案 B：创建独立的 SpotShadowPass

```cpp
class SpotShadowPass : public RenderPass { ... };
```

**优点**：
- 职责单一

**缺点**：
- 需要修改 `RenderPipeline` 注册新 Pass
- 需要在 `Renderer3D::Init()` 中注册
- Atlas FBO 需要在多个 Pass 之间共享（增加复杂度）

### 3.2 推荐实现（方案 A）

#### ShadowPass.h 修改

```cpp
// Lucky/Source/Lucky/Renderer/Passes/ShadowPass.h
#pragma once

#include "Lucky/Renderer/RenderPass.h"
#include "Lucky/Renderer/Shader.h"
#include "Lucky/Renderer/Framebuffer.h"
#include "Lucky/Renderer/ShadowAtlas.h"

namespace Lucky
{
    /// <summary>
    /// 阴影 Pass：从所有投射阴影的光源视角渲染场景深度
    /// 支持：方向光 CSM（Texture2DArray）+ 聚光灯（Shadow Atlas）
    /// 属于 "Shadow" 分组，在 OpaquePass 之前执行
    /// </summary>
    class ShadowPass : public RenderPass
    {
    public:
        void Init() override;
        void Execute(const RenderContext& context) override;
        void Resize(uint32_t width, uint32_t height) override;
        const std::string& GetName() const override { static std::string name = "ShadowPass"; return name; }
        const std::string& GetGroup() const override { static std::string group = "Shadow"; return group; }

        /// <summary>
        /// 绘制调试 GUI（CSM 级联可视化开关等）
        /// </summary>
        void OnDebugGUI() override;

        /// <summary>
        /// 获取 CSM 级联可视化开关状态
        /// </summary>
        bool IsDebugCSMVisualize() const { return m_DebugCSMVisualize; }

        /// <summary>
        /// 获取 CSM Texture2DArray 深度纹理 ID（方向光，保留旧接口）
        /// </summary>
        uint32_t GetShadowMapTextureID() const;

        /// <summary>
        /// 获取 Translucent Shadow Map Array 颜色纹理 ID
        /// </summary>
        uint32_t GetTranslucentShadowMapTextureID() const;

        /// <summary>
        /// 获取 Shadow Atlas 管理器（供 Renderer3D 查询 Tile 信息）
        /// </summary>
        const ShadowAtlas& GetShadowAtlas() const { return m_ShadowAtlas; }

        /// <summary>
        /// 获取 Shadow Atlas 深度纹理 ID
        /// </summary>
        uint32_t GetShadowAtlasTextureID() const;

        /// <summary>
        /// 获取 Shadow Atlas 纹理尺寸
        /// </summary>
        uint32_t GetShadowAtlasSize() const { return m_ShadowAtlas.GetAtlasSize(); }

    private:
        /// <summary>
        /// 重新创建 CSM FBO（当分辨率或级联数量变化时调用）
        /// </summary>
        void RecreateFBOs();

        /// <summary>
        /// 渲染方向光 CSM（保持现有逻辑，使用 Texture2DArray）
        /// </summary>
        void RenderDirectionalLightCSM(const RenderContext& context);

        /// <summary>
        /// 渲染 Translucent Shadow Map（保持现有逻辑）
        /// </summary>
        void RenderTranslucentShadow(const RenderContext& context);

        /// <summary>
        /// 渲染所有聚光灯阴影到 Shadow Atlas
        /// </summary>
        void RenderSpotLightShadows(const RenderContext& context);

        // ---- CSM 资源（方向光，保留现有实现） ----
        Ref<Framebuffer> m_CSMFramebuffer;              // CSM Texture2DArray FBO
        Ref<Framebuffer> m_TranslucentShadowFBO;        // Translucent Shadow Map FBO
        Ref<Shader> m_ShadowShader;                     // Shadow Pass Shader
        uint32_t m_ShadowMapResolution = 2048;          // CSM Shadow Map 分辨率
        int m_CascadeCount = 4;                         // 当前级联数量

        // ---- Shadow Atlas（聚光灯 + 后续点光源） ----
        ShadowAtlas m_ShadowAtlas;                      // Shadow Atlas 管理器

        // ---- 调试选项 ----
        bool m_DebugCSMVisualize = false;               // CSM 级联颜色可视化开关
    };
}
```

#### ShadowPass.cpp 新增聚光灯渲染

```cpp
void ShadowPass::Init()
{
    m_ShadowShader = Renderer3D::GetShaderLibrary()->Get("Shadow");
    RecreateFBOs();

    // 初始化 Shadow Atlas
    m_ShadowAtlas.Init(4096);
}

void ShadowPass::Execute(const RenderContext& context)
{
    bool hasOpaque = context.OpaqueDrawCommands && !context.OpaqueDrawCommands->empty();
    bool hasTransparent = context.TransparentDrawCommands && !context.TransparentDrawCommands->empty();

    if (!hasOpaque && !hasTransparent)
    {
        return;
    }

    // ======== 阶段 1：方向光 CSM（保持现有逻辑） ========
    if (context.ShadowEnabled)
    {
        RenderDirectionalLightCSM(context);
        RenderTranslucentShadow(context);
    }

    // ======== 阶段 2：聚光灯阴影（Shadow Atlas） ========
    if (context.ShadowData.SpotLightShadowCount > 0 && hasOpaque)
    {
        RenderSpotLightShadows(context);
    }

    // ---- 恢复主 FBO 视口 ----
    if (context.TargetFramebuffer)
    {
        context.TargetFramebuffer->Bind();
        const auto& spec = context.TargetFramebuffer->GetSpecification();
        RenderCommand::SetViewport(0, 0, spec.Width, spec.Height);
    }
}

void ShadowPass::RenderSpotLightShadows(const RenderContext& context)
{
    // 重置 Atlas 帧状态
    m_ShadowAtlas.BeginFrame();

    // 绑定 Atlas FBO
    m_ShadowAtlas.GetFramebuffer()->Bind();

    // 清除整个 Atlas 深度为 1.0（最远）
    RenderCommand::SetViewport(0, 0, m_ShadowAtlas.GetAtlasSize(), m_ShadowAtlas.GetAtlasSize());
    RenderCommand::Clear();

    // 启用 Scissor Test（防止渲染溢出到相邻 Tile）
    RenderCommand::SetScissorTest(true);
    RenderCommand::SetCullMode(CullMode::Off);

    m_ShadowShader->Bind();
    m_ShadowShader->SetInt("u_AlphaTestEnabled", 0);
    m_ShadowShader->SetInt("u_TranslucentShadowEnabled", 0);

    // 逐聚光灯渲染
    for (int i = 0; i < context.ShadowData.SpotLightShadowCount; ++i)
    {
        const auto& spotShadow = context.ShadowData.SpotLights[i];
        int tileIdx = m_ShadowAtlas.GetSpotLightTileIndex(i);
        const ShadowAtlasTile& tile = m_ShadowAtlas.GetTile(tileIdx);

        // 激活 Tile（设置 LightSpaceMatrix）
        m_ShadowAtlas.ActivateSpotTile(i, spotShadow.LightSpaceMatrix);

        // 设置视口和裁剪区域到 Tile 范围
        RenderCommand::SetViewport(tile.X, tile.Y, tile.Width, tile.Height);
        RenderCommand::SetScissor(tile.X, tile.Y, tile.Width, tile.Height);

        // 清除当前 Tile 区域的深度
        RenderCommand::Clear();

        // 设置 Light Space Matrix
        m_ShadowShader->SetMat4("u_LightSpaceMatrix", spotShadow.LightSpaceMatrix);

        // 渲染所有不透明物体
        for (const DrawCommand& cmd : *context.OpaqueDrawCommands)
        {
            m_ShadowShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);

            RenderCommand::DrawIndexedRange(
                cmd.MeshData->GetVertexArray(),
                cmd.SubMeshPtr->IndexOffset,
                cmd.SubMeshPtr->IndexCount
            );
        }
    }

    // 关闭 Scissor Test
    RenderCommand::SetScissorTest(false);

    // 解绑 Atlas FBO
    m_ShadowAtlas.GetFramebuffer()->Unbind();
}

uint32_t ShadowPass::GetShadowAtlasTextureID() const
{
    return m_ShadowAtlas.GetAtlasTextureID();
}
```

---

## 四、Renderer3D 修改

### 4.1 BeginScene() 新增聚光灯矩阵计算

```cpp
// Renderer3D.cpp BeginScene() 中，CSM 计算之后新增：

// ======== 聚光灯阴影矩阵计算 ========
int spotShadowCount = 0;
for (int i = 0; i < lightData.SpotLightCount && spotShadowCount < ShadowAtlas::s_MaxSpotLightShadows; ++i)
{
    if (lightData.SpotLightShadows[i].Shadows != ShadowType::None)
    {
        // 计算聚光灯 Light Space Matrix
        glm::mat4 lightSpaceMatrix = CalcSpotLightMatrix(
            lightData.SpotLights[i],
            lightData.SpotLights[i].Position
        );

        // 存储到 Renderer3DData 中（后续在 EndScene 中传递给 RenderContext）
        s_Data.SpotShadowData[spotShadowCount].LightIndex = i;
        s_Data.SpotShadowData[spotShadowCount].LightSpaceMatrix = lightSpaceMatrix;
        s_Data.SpotShadowData[spotShadowCount].ShadowBias = lightData.SpotLightShadows[i].ShadowBias;
        s_Data.SpotShadowData[spotShadowCount].ShadowStrength = lightData.SpotLightShadows[i].ShadowStrength;
        s_Data.SpotShadowData[spotShadowCount].ShadowType = static_cast<int>(lightData.SpotLightShadows[i].Shadows);

        ++spotShadowCount;
    }
}
s_Data.SpotShadowCount = spotShadowCount;
```

### 4.2 EndScene() 构建 RenderContext

```cpp
// EndScene() 中，构建 RenderContext 时新增：

// ---- 聚光灯阴影数据 ----
context.ShadowData.SpotLightShadowCount = s_Data.SpotShadowCount;
for (int i = 0; i < s_Data.SpotShadowCount; ++i)
{
    context.ShadowData.SpotLights[i].LightIndex = s_Data.SpotShadowData[i].LightIndex;
    context.ShadowData.SpotLights[i].LightSpaceMatrix = s_Data.SpotShadowData[i].LightSpaceMatrix;
    context.ShadowData.SpotLights[i].ShadowBias = s_Data.SpotShadowData[i].ShadowBias;
    context.ShadowData.SpotLights[i].ShadowStrength = s_Data.SpotShadowData[i].ShadowStrength;
    context.ShadowData.SpotLights[i].ShadowType = s_Data.SpotShadowData[i].ShadowType;

    // 从 ShadowAtlas 获取 Tile 的 ViewportScaleBias
    auto shadowPass = s_Data.Pipeline.GetPass<ShadowPass>();
    if (shadowPass)
    {
        int tileIdx = shadowPass->GetShadowAtlas().GetSpotLightTileIndex(i);
        context.ShadowData.SpotLights[i].AtlasScaleBias = shadowPass->GetShadowAtlas().GetTile(tileIdx).ViewportScaleBias;
    }
}

// Shadow Atlas 纹理 ID
auto shadowPass = s_Data.Pipeline.GetPass<ShadowPass>();
if (shadowPass)
{
    context.ShadowAtlasTextureID = shadowPass->GetShadowAtlasTextureID();
    context.ShadowAtlasSize = shadowPass->GetShadowAtlasSize();
}
```

### 4.3 Renderer3DData 新增字段

```cpp
// Renderer3D.cpp 中 Renderer3DData 新增

// ======== 聚光灯阴影数据 ========
struct SpotShadowCacheData
{
    int LightIndex = -1;
    glm::mat4 LightSpaceMatrix = glm::mat4(1.0f);
    float ShadowBias = 0.001f;
    float ShadowStrength = 1.0f;
    int ShadowType = 1;
};
SpotShadowCacheData SpotShadowData[ShadowAtlas::s_MaxSpotLightShadows];
int SpotShadowCount = 0;
```

---

## 五、OpaquePass / TransparentPass 修改

### 5.1 纹理绑定

```cpp
// OpaquePass::Execute() 中，现有 Shadow Map 绑定之后新增：

// ---- 绑定 Shadow Atlas 纹理（聚光灯阴影） ----
if (context.ShadowData.SpotLightShadowCount > 0 && context.ShadowAtlasTextureID != 0)
{
    // 绑定 Shadow Atlas 到纹理单元 14（与 CSM 的单元 15 分开）
    RenderCommand::BindTextureUnit(14, context.ShadowAtlasTextureID);
}
```

> **注意**：本阶段 Shadow Atlas 使用纹理单元 **14**（而非 15），因为方向光 CSM 仍使用单元 15 的 `sampler2DArray`。待 R29 完成 CSM 迁移后，统一使用单元 15。

### 5.2 Uniform 设置

```cpp
// 在每个 DrawCommand 循环中，设置阴影 uniform 之后新增：

// ---- 聚光灯阴影 uniform ----
if (context.ShadowData.SpotLightShadowCount > 0)
{
    cmd.MaterialData->GetShader()->SetInt("u_ShadowAtlas", 14);
    cmd.MaterialData->GetShader()->SetFloat("u_ShadowAtlasSize", static_cast<float>(context.ShadowAtlasSize));
    cmd.MaterialData->GetShader()->SetInt("u_SpotShadowCount", context.ShadowData.SpotLightShadowCount);

    for (int i = 0; i < context.ShadowData.SpotLightShadowCount; ++i)
    {
        const auto& spotShadow = context.ShadowData.SpotLights[i];
        std::string idx = std::to_string(i);

        cmd.MaterialData->GetShader()->SetInt("u_SpotShadowLightIndex[" + idx + "]", spotShadow.LightIndex);
        cmd.MaterialData->GetShader()->SetMat4("u_SpotShadowLightSpaceMatrices[" + idx + "]", spotShadow.LightSpaceMatrix);
        cmd.MaterialData->GetShader()->SetFloat4("u_SpotShadowAtlasScaleBias[" + idx + "]", spotShadow.AtlasScaleBias);
        cmd.MaterialData->GetShader()->SetFloat("u_SpotShadowBias[" + idx + "]", spotShadow.ShadowBias);
        cmd.MaterialData->GetShader()->SetFloat("u_SpotShadowStrength[" + idx + "]", spotShadow.ShadowStrength);
        cmd.MaterialData->GetShader()->SetInt("u_SpotShadowType[" + idx + "]", spotShadow.ShadowType);
    }
}
else
{
    cmd.MaterialData->GetShader()->SetInt("u_SpotShadowCount", 0);
}
```

---

## 六、Shader 修改

### 6.1 Shadow.glsl 新增聚光灯阴影函数

在现有 `Shadow.glsl` 末尾（`#endif` 之前）新增：

```glsl
// ==================== Shadow Atlas 聚光灯阴影 ====================

#define MAX_SPOT_SHADOW_LIGHTS 4

// ---- Shadow Atlas 纹理 ----
uniform sampler2D u_ShadowAtlas;                                        // Shadow Atlas 深度纹理
uniform float u_ShadowAtlasSize;                                        // Atlas 纹理尺寸

// ---- 聚光灯阴影数据 ----
uniform int u_SpotShadowCount;                                          // 投射阴影的聚光灯数量
uniform int u_SpotShadowLightIndex[MAX_SPOT_SHADOW_LIGHTS];             // 对应的光源索引
uniform mat4 u_SpotShadowLightSpaceMatrices[MAX_SPOT_SHADOW_LIGHTS];    // Light Space Matrix
uniform vec4 u_SpotShadowAtlasScaleBias[MAX_SPOT_SHADOW_LIGHTS];        // Atlas UV 变换
uniform float u_SpotShadowBias[MAX_SPOT_SHADOW_LIGHTS];                 // 阴影偏移
uniform float u_SpotShadowStrength[MAX_SPOT_SHADOW_LIGHTS];             // 阴影强度
uniform int u_SpotShadowType[MAX_SPOT_SHADOW_LIGHTS];                   // 阴影类型

/// <summary>
/// 将世界空间坐标变换到 Atlas UV 空间
/// </summary>
vec3 WorldToAtlasUV(vec3 worldPos, mat4 lightSpaceMatrix, vec4 atlasScaleBias)
{
    vec4 lightSpacePos = lightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;   // [-1,1] -> [0,1]

    // 应用 Atlas 缩放和偏移
    projCoords.xy = projCoords.xy * atlasScaleBias.xy + atlasScaleBias.zw;

    return projCoords;
}

/// <summary>
/// Atlas 硬阴影采样（单次采样）
/// </summary>
float AtlasShadowSampleHard(vec3 atlasUV, float bias)
{
    float currentDepth = atlasUV.z;
    float closestDepth = texture(u_ShadowAtlas, atlasUV.xy).r;
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

/// <summary>
/// Atlas 软阴影采样（PCF 5×5，带边界保护）
/// </summary>
float AtlasShadowSampleSoft(vec3 atlasUV, float bias, vec4 atlasScaleBias)
{
    float currentDepth = atlasUV.z;
    float shadow = 0.0;
    vec2 atlasTexelSize = 1.0 / vec2(textureSize(u_ShadowAtlas, 0));

    // Tile 边界（留 1 像素边距防止采样溢出）
    vec2 tileMin = atlasScaleBias.zw + atlasTexelSize;
    vec2 tileMax = atlasScaleBias.zw + atlasScaleBias.xy - atlasTexelSize;

    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            vec2 sampleUV = clamp(
                atlasUV.xy + vec2(x, y) * atlasTexelSize,
                tileMin, tileMax
            );
            float pcfDepth = texture(u_ShadowAtlas, sampleUV).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 25.0;
}

/// <summary>
/// 计算聚光灯阴影因子
/// 返回 float：1.0 = 完全不在阴影中，0.0 = 完全在阴影中
/// </summary>
float CalcSpotLightShadow(int shadowIndex, vec3 worldPos, vec3 normal, vec3 lightDir)
{
    mat4 lightSpaceMatrix = u_SpotShadowLightSpaceMatrices[shadowIndex];
    vec4 atlasScaleBias = u_SpotShadowAtlasScaleBias[shadowIndex];

    vec3 atlasUV = WorldToAtlasUV(worldPos, lightSpaceMatrix, atlasScaleBias);

    // 超出 Shadow Map 范围（透视投影的 z 可能 > 1 或 < 0）
    if (atlasUV.z > 1.0 || atlasUV.z < 0.0)
    {
        return 1.0;
    }

    // 检查是否在 Tile 范围内（防止采样到相邻 Tile）
    vec2 tileMin = atlasScaleBias.zw;
    vec2 tileMax = atlasScaleBias.zw + atlasScaleBias.xy;
    if (atlasUV.x < tileMin.x || atlasUV.x > tileMax.x ||
        atlasUV.y < tileMin.y || atlasUV.y > tileMax.y)
    {
        return 1.0;
    }

    // 动态 Bias
    float NdotL = dot(normal, lightDir);
    float bias = u_SpotShadowBias[shadowIndex] * (1.0 + 9.0 * (1.0 - clamp(NdotL, 0.0, 1.0)));

    // 根据阴影类型选择采样方式
    float shadow = 0.0;
    if (u_SpotShadowType[shadowIndex] == 1)     // Hard
    {
        shadow = AtlasShadowSampleHard(atlasUV, bias);
    }
    else    // Soft
    {
        shadow = AtlasShadowSampleSoft(atlasUV, bias, atlasScaleBias);
    }

    shadow *= u_SpotShadowStrength[shadowIndex];
    return 1.0 - shadow;
}

/// <summary>
/// 获取聚光灯阴影因子（供 Lighting.glsl 调用）
/// lightIndex: 聚光灯在 u_Lights.SpotLights[] 中的索引
/// </summary>
float GetSpotLightShadow(int lightIndex, vec3 worldPos, vec3 normal, vec3 lightDir)
{
    // 查找该聚光灯对应的阴影索引
    for (int i = 0; i < u_SpotShadowCount; ++i)
    {
        if (u_SpotShadowLightIndex[i] == lightIndex)
        {
            return CalcSpotLightShadow(i, worldPos, normal, lightDir);
        }
    }
    return 1.0;  // 该光源不投射阴影
}
```

### 6.2 Lighting.glsl 修改

将聚光灯循环中添加阴影查询：

```glsl
// 聚光灯（新增阴影支持）
for (int i = 0; i < u_Lights.SpotLightCount; ++i)
{
    vec3 contribution = CalcSpotLight(u_Lights.SpotLights[i], N, V, worldPos, albedo, metallic, roughness, F0);

    // 应用聚光灯阴影
    if (u_SpotShadowCount > 0)
    {
        vec3 lightDir = normalize(u_Lights.SpotLights[i].Position - worldPos);
        float shadowFactor = GetSpotLightShadow(i, worldPos, N, lightDir);
        contribution *= shadowFactor;
    }

    Lo += contribution;
}
```

---

## 七、Scene.cpp 修改

### 7.1 收集聚光灯阴影参数

```cpp
// Scene.cpp 中收集光照数据时，聚光灯部分新增：

// 收集聚光灯阴影参数
if (spotIndex < s_MaxSpotLights)
{
    lightData.SpotLightShadows[spotIndex].Shadows = light.Shadows;
    lightData.SpotLightShadows[spotIndex].ShadowBias = light.ShadowBias;
    lightData.SpotLightShadows[spotIndex].ShadowStrength = light.ShadowStrength;
}
```

---

## 八、Material 内部 Uniform 黑名单更新

```cpp
// Material.cpp 中 IsInternalUniform() 新增：

// ---- Shadow Atlas 聚光灯阴影 ----
"u_ShadowAtlas",
"u_ShadowAtlasSize",
"u_SpotShadowCount",
"u_SpotShadowLightIndex",
"u_SpotShadowLightSpaceMatrices",
"u_SpotShadowAtlasScaleBias",
"u_SpotShadowBias",
"u_SpotShadowStrength",
"u_SpotShadowType",
```

---

## 九、涉及的文件清单

### 需要修改的文件

| 文件路径 | 修改内容 |
|---------|----------|
| `Lucky/Source/Lucky/Renderer/Passes/ShadowPass.h` | 新增 `m_ShadowAtlas`、`RenderSpotLightShadows()`、Atlas 查询接口 |
| `Lucky/Source/Lucky/Renderer/Passes/ShadowPass.cpp` | `Init()` 初始化 Atlas、`Execute()` 新增聚光灯渲染、实现 `RenderSpotLightShadows()` |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | `BeginScene()` 计算聚光灯矩阵、`EndScene()` 构建 Shadow Atlas RenderContext |
| `Lucky/Source/Lucky/Renderer/Passes/OpaquePass.cpp` | 绑定 Atlas 纹理单元 14、设置聚光灯阴影 uniform |
| `Lucky/Source/Lucky/Renderer/Passes/TransparentPass.cpp` | 同 OpaquePass |
| `Lucky/Source/Lucky/Renderer/Material.cpp` | `IsInternalUniform` 黑名单新增 Atlas 相关 uniform |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | 收集聚光灯阴影参数到 `SceneLightData` |
| `Luck3DApp/Assets/Shaders/Lucky/Shadow.glsl` | 新增 Atlas 采样函数、聚光灯阴影计算 |
| `Luck3DApp/Assets/Shaders/Lucky/Lighting.glsl` | 聚光灯循环中添加阴影查询 |

### 不需要修改的文件

| 文件路径 | 原因 |
|---------|------|
| `LightComponent.h` | 阴影属性已通过通用字段支持（`Shadows`、`ShadowBias`、`ShadowStrength`） |
| `Framebuffer.h/cpp` | Atlas 使用现有 `DEPTH_COMPONENT` 格式 |
| `InspectorPanel.cpp` | 聚光灯阴影 UI 已有通用字段显示 |
| `SceneSerializer.cpp` | 阴影属性已有序列化支持 |

---

## 十、实施步骤

| 步骤 | 内容 | 验证方式 |
|------|------|---------|
| 1 | `ShadowPass.h` 新增 `m_ShadowAtlas` 成员和聚光灯渲染方法声明 | 编译通过 |
| 2 | `ShadowPass.cpp` `Init()` 中初始化 Atlas | 编译通过 |
| 3 | 实现 `RenderSpotLightShadows()` | 编译通过 |
| 4 | `Renderer3D.cpp` 新增 `CalcSpotLightMatrix()` 和 `SpotShadowCacheData` | 编译通过 |
| 5 | `Renderer3D.cpp` `BeginScene()` 计算聚光灯矩阵 | 编译通过 |
| 6 | `Renderer3D.cpp` `EndScene()` 构建聚光灯 RenderContext | 编译通过 |
| 7 | `Scene.cpp` 收集聚光灯阴影参数 | 编译通过 |
| 8 | `OpaquePass.cpp` 绑定 Atlas 纹理 + 设置 uniform | 编译通过 |
| 9 | `TransparentPass.cpp` 同上 | 编译通过 |
| 10 | `Shadow.glsl` 新增 Atlas 采样函数 | Shader 编译通过 |
| 11 | `Lighting.glsl` 聚光灯循环添加阴影 | Shader 编译通过 |
| 12 | `Material.cpp` 更新黑名单 | 编译通过 |
| 13 | 运行测试：创建聚光灯并开启阴影 | 视觉验证阴影正确 |
| 14 | 测试：多个聚光灯同时投射阴影 | 视觉验证无干扰 |
| 15 | 测试：Hard/Soft 阴影切换 | 视觉验证 PCF 效果 |

---

## 十一、设计决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| FOV 计算方式 | OuterCutoffAngle * 2（方案 A） | 精确覆盖照射范围，分辨率利用率最高 |
| ShadowPass 架构 | 在现有 Pass 中新增分支（方案 A） | 改动最小，Atlas FBO 无需跨 Pass 共享 |
| Atlas 纹理单元 | 单元 14（过渡期） | 与 CSM 的单元 15 分开，避免冲突 |
| 聚光灯阴影 Shader | 复用 Shadow.vert/frag | 只需 LightSpaceMatrix + ObjectToWorldMatrix |
| 阴影查找方式 | 循环匹配 LightIndex | 聚光灯数量少（≤4），循环开销可忽略 |

---

## 十二、已知限制与后续优化

| 限制 | 影响 | 后续优化方案 |
|------|------|-------------|
| 聚光灯不支持 Translucent Shadow | 透明物体不会在聚光灯下产生彩色阴影 | 后续可为 Atlas 添加颜色通道 |
| 无视锥体裁剪 | 所有不透明物体都渲染到每个聚光灯 Shadow Map | 添加光源视锥体裁剪 |
| 固定 512×512 分辨率 | 近处聚光灯阴影可能不够精细 | 动态分辨率分配 |
| 纹理单元 14 为过渡方案 | R29 完成后需要统一到单元 15 | R29 中处理 |
