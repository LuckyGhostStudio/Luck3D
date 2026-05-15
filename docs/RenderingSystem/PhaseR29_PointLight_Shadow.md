# Phase R29：点光源阴影（Point Light Shadow）

> **创建日期**：2026-05-15
> **前置依赖**：R27（Shadow Atlas 基础架构）、R28（聚光灯阴影）
> **目标**：实现点光源的全方向阴影投射（Omnidirectional Shadow Map），并将方向光 CSM 迁移到 Shadow Atlas，完成阴影系统统一
> **完成标志**：所有光源类型均支持阴影投射，旧的 CSM Texture2DArray 代码删除

---

## 一、概述

### 1.1 点光源阴影原理

点光源向所有方向发射光线，需要 **6 个方向**的 Shadow Map 来覆盖完整的球形范围（类似 Cubemap 的 6 个面）：

```
        +Y
         |
         |
  -X ----*---- +X
        /|
       / |
      +Z -Y
```

每个面使用 **90° FOV 透视投影**，6 个面拼合后覆盖完整的 360° 球形空间。

### 1.2 技术挑战

| 挑战 | 说明 | 解决方案 |
|------|------|---------|
| 6 次渲染 | 每个点光源需要 6 次 DrawCall 循环 | 性能优化：距离裁剪、可见面裁剪 |
| 深度精度 | 透视投影的非线性深度在远处精度差 | 使用线性深度（距离/远平面） |
| 面选择 | 采样时需要确定片段属于哪个面 | 根据光源到片段的方向向量选择主轴 |
| Atlas 空间 | 每个点光源占 6 个 Tile | 固定布局已预留空间 |

### 1.3 本阶段额外目标

除了实现点光源阴影外，本阶段还将：
1. **将方向光 CSM 迁移到 Shadow Atlas**（统一所有光源的阴影存储）
2. **删除旧的 CSM Texture2DArray 相关代码**
3. **统一纹理单元**（Atlas 使用单元 15，删除过渡期的单元 14）
4. **删除 SceneLightData 和 RenderContext 中的旧字段**

---

## 二、点光源 Light Space Matrix 计算

### 2.1 6 面投影矩阵

```cpp
/// <summary>
/// 计算点光源 6 面的 Light Space Matrix
/// 使用 90° FOV 透视投影，覆盖 Cubemap 的每个面
/// </summary>
/// <param name="lightPos">点光源世界空间位置</param>
/// <param name="farPlane">远平面距离（= Range）</param>
/// <returns>6 个 Light Space VP 矩阵（+X, -X, +Y, -Y, +Z, -Z）</returns>
static std::array<glm::mat4, 6> CalcPointLightMatrices(const glm::vec3& lightPos, float farPlane)
{
    float nearPlane = 0.1f;
    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);

    std::array<glm::mat4, 6> matrices;

    // +X：看向右方
    matrices[0] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
    // -X：看向左方
    matrices[1] = projection * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
    // +Y：看向上方
    matrices[2] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    // -Y：看向下方
    matrices[3] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f,-1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    // +Z：看向前方
    matrices[4] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
    // -Z：看向后方
    matrices[5] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, 0.0f,-1.0f), glm::vec3(0.0f, -1.0f, 0.0f));

    return matrices;
}
```

### 2.2 Up 向量说明

| 面 | 目标方向 | Up 向量 | 说明 |
|----|---------|---------|------|
| +X | (1,0,0) | (0,-1,0) | 标准 Cubemap 约定 |
| -X | (-1,0,0) | (0,-1,0) | 标准 Cubemap 约定 |
| +Y | (0,1,0) | (0,0,1) | 看向上方时 up 不能是 Y 轴 |
| -Y | (0,-1,0) | (0,0,-1) | 看向下方时 up 不能是 Y 轴 |
| +Z | (0,0,1) | (0,-1,0) | 标准 Cubemap 约定 |
| -Z | (0,0,-1) | (0,-1,0) | 标准 Cubemap 约定 |

---

## 三、深度存储方案

### 3.1 方案对比

#### 方案 A：线性深度（推荐 ?????）

在 Fragment Shader 中计算片段到光源的距离，归一化到 [0,1] 后写入 `gl_FragDepth`。

```glsl
// PointShadow.frag
float lightDistance = length(v_WorldPos - u_LightPos);
gl_FragDepth = lightDistance / u_FarPlane;
```

采样时也使用线性距离比较：

```glsl
float currentDistance = length(worldPos - lightPos) / farPlane;
float closestDistance = texture(u_ShadowAtlas, atlasUV.xy).r;
float shadow = currentDistance - bias > closestDistance ? 1.0 : 0.0;
```

**优点**：
- 深度精度均匀分布（不受透视投影非线性影响）
- 远处阴影质量好
- 采样逻辑简单直观

**缺点**：
- 需要专用的 Fragment Shader（不能复用方向光/聚光灯的 Shadow Shader）
- 需要传递 `u_LightPos` 和 `u_FarPlane` 到 Shader

#### 方案 B：默认非线性深度

直接使用 OpenGL 默认的透视投影深度（`gl_FragCoord.z`），不修改 `gl_FragDepth`。

```glsl
// 不需要 Fragment Shader 修改，使用默认深度写入
```

采样时使用标准的深度比较：

```glsl
vec3 atlasUV = WorldToAtlasUV(worldPos, lightSpaceMatrix, atlasScaleBias);
float shadow = atlasUV.z - bias > texture(u_ShadowAtlas, atlasUV.xy).r ? 1.0 : 0.0;
```

**优点**：
- 可以复用现有的 Shadow Shader（`Shadow.vert/frag`）
- 实现更简单

**缺点**：
- 远处深度精度极差（透视投影的 z 值在远处高度非线性）
- 点光源 Range 较大时阴影质量严重下降
- Shadow Acne 在远处更严重

#### 方案 C：对数深度

使用对数深度缓冲，在近处和远处都有较好的精度。

**优点**：
- 精度分布比默认深度好

**缺点**：
- 实现复杂
- 需要修改所有深度比较逻辑
- 与现有系统不兼容

### 3.2 推荐方案

**推荐方案 A（线性深度）**，理由：
1. 点光源 Range 通常为 10-50 单位，线性深度在此范围内精度最优
2. 虽然需要专用 Shader，但代码量很小（仅 Fragment Shader 多 2 行）
3. 采样逻辑清晰，不容易出错
4. 业界标准做法（LearnOpenGL、Unity、Unreal 均使用线性深度）

---

## 四、点光源 Shadow Shader

### 4.1 顶点着色器

```glsl
// Luck3DApp/Assets/Shaders/Internal/Shadow/PointShadow.vert
#version 450 core

layout(location = 0) in vec3 a_Position;    // 位置
layout(location = 1) in vec4 a_Color;       // 颜色（未使用）
layout(location = 2) in vec3 a_Normal;      // 法线（未使用）
layout(location = 3) in vec2 a_TexCoord;    // 纹理坐标（未使用）
layout(location = 4) in vec4 a_Tangent;     // 切线（未使用）

layout(location = 0) out vec3 v_WorldPos;   // 世界空间位置（传递给 Fragment Shader）

uniform mat4 u_LightSpaceMatrix;            // 光源空间 VP 矩阵
uniform mat4 u_ObjectToWorldMatrix;         // 模型变换矩阵

void main()
{
    vec4 worldPos = u_ObjectToWorldMatrix * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;
    gl_Position = u_LightSpaceMatrix * worldPos;
}
```

### 4.2 片段着色器

```glsl
// Luck3DApp/Assets/Shaders/Internal/Shadow/PointShadow.frag
#version 450 core

layout(location = 0) in vec3 v_WorldPos;    // 世界空间位置

uniform vec3 u_LightPos;                    // 点光源世界空间位置
uniform float u_FarPlane;                   // 远平面距离（= Range）

void main()
{
    // 计算片段到光源的线性距离，归一化到 [0, 1]
    float lightDistance = length(v_WorldPos - u_LightPos);
    gl_FragDepth = lightDistance / u_FarPlane;
}
```

---

## 五、ShadowPass 扩展

### 5.1 新增点光源渲染方法

```cpp
// ShadowPass.h 新增

/// <summary>
/// 渲染所有点光源阴影到 Shadow Atlas（每个点光源 6 面）
/// </summary>
void RenderPointLightShadows(const RenderContext& context);
```

### 5.2 实现

```cpp
void ShadowPass::RenderPointLightShadows(const RenderContext& context)
{
    if (context.ShadowData.PointLightShadowCount <= 0)
    {
        return;
    }

    bool hasOpaque = context.OpaqueDrawCommands && !context.OpaqueDrawCommands->empty();
    if (!hasOpaque)
    {
        return;
    }

    // 绑定 Atlas FBO（如果尚未绑定）
    m_ShadowAtlas.GetFramebuffer()->Bind();

    // 启用 Scissor Test
    RenderCommand::SetScissorTest(true);
    RenderCommand::SetCullMode(CullMode::Off);

    // 使用点光源专用 Shader
    m_PointShadowShader->Bind();

    // 逐点光源渲染
    for (int i = 0; i < context.ShadowData.PointLightShadowCount; ++i)
    {
        const auto& pointShadow = context.ShadowData.PointLights[i];

        // 设置光源位置和远平面（所有 6 面共享）
        m_PointShadowShader->SetFloat3("u_LightPos", /* 从 PointLightData 获取 */);
        m_PointShadowShader->SetFloat("u_FarPlane", pointShadow.FarPlane);

        // 渲染 6 个面
        for (int face = 0; face < 6; ++face)
        {
            int tileIdx = m_ShadowAtlas.GetPointLightTileStart(i) + face;
            const ShadowAtlasTile& tile = m_ShadowAtlas.GetTile(tileIdx);

            // 激活 Tile
            m_ShadowAtlas.ActivatePointTile(i, face, pointShadow.LightSpaceMatrices[face]);

            // 设置视口和裁剪区域
            RenderCommand::SetViewport(tile.X, tile.Y, tile.Width, tile.Height);
            RenderCommand::SetScissor(tile.X, tile.Y, tile.Width, tile.Height);

            // 清除当前 Tile 深度
            RenderCommand::Clear();

            // 设置 Light Space Matrix
            m_PointShadowShader->SetMat4("u_LightSpaceMatrix", pointShadow.LightSpaceMatrices[face]);

            // 渲染所有不透明物体
            for (const DrawCommand& cmd : *context.OpaqueDrawCommands)
            {
                m_PointShadowShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);

                RenderCommand::DrawIndexedRange(
                    cmd.MeshData->GetVertexArray(),
                    cmd.SubMeshPtr->IndexOffset,
                    cmd.SubMeshPtr->IndexCount
                );
            }
        }
    }

    // 关闭 Scissor Test
    RenderCommand::SetScissorTest(false);
}
```

---

## 六、Shader 采样（点光源阴影）

### 6.1 面选择算法

```glsl
/// <summary>
/// 根据光源到片段的方向向量选择 Cubemap 面索引
/// 选择绝对值最大的轴作为主轴
/// </summary>
int SelectPointLightFace(vec3 fragToLight)
{
    vec3 absDir = abs(fragToLight);

    if (absDir.x >= absDir.y && absDir.x >= absDir.z)
    {
        return fragToLight.x > 0.0 ? 0 : 1;   // +X or -X
    }
    else if (absDir.y >= absDir.x && absDir.y >= absDir.z)
    {
        return fragToLight.y > 0.0 ? 2 : 3;   // +Y or -Y
    }
    else
    {
        return fragToLight.z > 0.0 ? 4 : 5;   // +Z or -Z
    }
}
```

### 6.2 Shadow.glsl 新增点光源阴影

```glsl
// ==================== Shadow Atlas 点光源阴影 ====================

#define MAX_POINT_SHADOW_LIGHTS 2

// ---- 点光源阴影数据 ----
uniform int u_PointShadowCount;                                                 // 投射阴影的点光源数量
uniform int u_PointShadowLightIndex[MAX_POINT_SHADOW_LIGHTS];                   // 对应的光源索引
uniform mat4 u_PointShadowLightSpaceMatrices[MAX_POINT_SHADOW_LIGHTS * 6];      // 6 面 Light Space Matrix
uniform vec4 u_PointShadowAtlasScaleBias[MAX_POINT_SHADOW_LIGHTS * 6];          // 6 面 Atlas UV 变换
uniform float u_PointShadowBias[MAX_POINT_SHADOW_LIGHTS];                       // 阴影偏移
uniform float u_PointShadowStrength[MAX_POINT_SHADOW_LIGHTS];                   // 阴影强度
uniform int u_PointShadowType[MAX_POINT_SHADOW_LIGHTS];                         // 阴影类型
uniform float u_PointShadowFarPlane[MAX_POINT_SHADOW_LIGHTS];                   // 远平面（= Range）
uniform vec3 u_PointShadowLightPos[MAX_POINT_SHADOW_LIGHTS];                    // 光源位置

/// <summary>
/// 根据光源到片段的方向向量选择 Cubemap 面索引
/// </summary>
int SelectPointLightFace(vec3 fragToLight)
{
    vec3 absDir = abs(fragToLight);

    if (absDir.x >= absDir.y && absDir.x >= absDir.z)
    {
        return fragToLight.x > 0.0 ? 0 : 1;
    }
    else if (absDir.y >= absDir.x && absDir.y >= absDir.z)
    {
        return fragToLight.y > 0.0 ? 2 : 3;
    }
    else
    {
        return fragToLight.z > 0.0 ? 4 : 5;
    }
}

/// <summary>
/// 计算点光源阴影因子
/// 使用线性深度比较（距离/远平面）
/// 返回 float：1.0 = 完全不在阴影中，0.0 = 完全在阴影中
/// </summary>
float CalcPointLightShadow(int shadowIndex, vec3 worldPos, vec3 lightPos, vec3 normal)
{
    vec3 fragToLight = worldPos - lightPos;
    int faceIndex = SelectPointLightFace(fragToLight);

    int tileIndex = shadowIndex * 6 + faceIndex;
    mat4 lightSpaceMatrix = u_PointShadowLightSpaceMatrices[tileIndex];
    vec4 atlasScaleBias = u_PointShadowAtlasScaleBias[tileIndex];

    // 变换到 Atlas UV 空间
    vec3 atlasUV = WorldToAtlasUV(worldPos, lightSpaceMatrix, atlasScaleBias);

    // 超出范围检查
    if (atlasUV.z > 1.0 || atlasUV.z < 0.0)
    {
        return 1.0;
    }

    // 计算线性深度（当前片段到光源的距离 / 远平面）
    float currentLinearDepth = length(worldPos - lightPos) / u_PointShadowFarPlane[shadowIndex];

    // 动态 Bias（点光源需要更大的 bias）
    vec3 lightDir = normalize(lightPos - worldPos);
    float NdotL = dot(normal, lightDir);
    float bias = u_PointShadowBias[shadowIndex] * (1.0 + 9.0 * (1.0 - clamp(NdotL, 0.0, 1.0)));

    // 读取 Atlas 中存储的线性深度
    float closestLinearDepth = texture(u_ShadowAtlas, atlasUV.xy).r;

    // 根据阴影类型选择采样方式
    float shadow = 0.0;
    if (u_PointShadowType[shadowIndex] == 1)    // Hard
    {
        shadow = currentLinearDepth - bias > closestLinearDepth ? 1.0 : 0.0;
    }
    else    // Soft（PCF 5×5）
    {
        vec2 atlasTexelSize = 1.0 / vec2(textureSize(u_ShadowAtlas, 0));
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
                shadow += currentLinearDepth - bias > pcfDepth ? 1.0 : 0.0;
            }
        }
        shadow /= 25.0;
    }

    shadow *= u_PointShadowStrength[shadowIndex];
    return 1.0 - shadow;
}

/// <summary>
/// 获取点光源阴影因子（供 Lighting.glsl 调用）
/// lightIndex: 点光源在 u_Lights.PointLights[] 中的索引
/// </summary>
float GetPointLightShadow(int lightIndex, vec3 worldPos, vec3 lightPos, vec3 normal)
{
    for (int i = 0; i < u_PointShadowCount; ++i)
    {
        if (u_PointShadowLightIndex[i] == lightIndex)
        {
            return CalcPointLightShadow(i, worldPos, lightPos, normal);
        }
    }
    return 1.0;
}
```

### 6.3 Lighting.glsl 修改

```glsl
// 点光源（新增阴影支持）
for (int i = 0; i < u_Lights.PointLightCount; ++i)
{
    vec3 contribution = CalcPointLight(u_Lights.PointLights[i], N, V, worldPos, albedo, metallic, roughness, F0);

    // 应用点光源阴影
    if (u_PointShadowCount > 0)
    {
        float shadowFactor = GetPointLightShadow(i, worldPos, u_Lights.PointLights[i].Position, N);
        contribution *= shadowFactor;
    }

    Lo += contribution;
}
```

---

## 七、方向光 CSM 迁移到 Atlas

### 7.1 迁移策略

将方向光 CSM 从独立的 `Texture2DArray` FBO 迁移到 Shadow Atlas 中的固定区域。

**迁移前**：
- CSM 使用 `m_CSMFramebuffer`（`DEPTH_COMPONENT_ARRAY`，Texture2DArray）
- Shader 使用 `sampler2DArray u_ShadowMapArray`

**迁移后**：
- CSM 使用 Shadow Atlas 中的方向光区域（2D 深度纹理的子区域）
- Shader 使用 `sampler2D u_ShadowAtlas` + `AtlasScaleBias` 变换

### 7.2 ShadowPass 修改

```cpp
void ShadowPass::RenderDirectionalLightCSM(const RenderContext& context)
{
    // 不再使用 m_CSMFramebuffer，改为渲染到 Atlas 的方向光区域

    m_ShadowAtlas.GetFramebuffer()->Bind();
    RenderCommand::SetScissorTest(true);
    RenderCommand::SetCullMode(CullMode::Off);

    m_ShadowShader->Bind();
    m_ShadowShader->SetInt("u_AlphaTestEnabled", 0);
    m_ShadowShader->SetInt("u_TranslucentShadowEnabled", 0);

    // 逐方向光渲染
    for (int dirIdx = 0; dirIdx < context.ShadowData.DirLightShadowCount; ++dirIdx)
    {
        const auto& dirShadow = context.ShadowData.DirLights[dirIdx];

        for (int cascade = 0; cascade < dirShadow.CascadeCount; ++cascade)
        {
            int tileIdx = m_ShadowAtlas.GetDirLightTileStart(dirIdx) + cascade;
            const ShadowAtlasTile& tile = m_ShadowAtlas.GetTile(tileIdx);

            // 激活 Tile
            m_ShadowAtlas.ActivateDirectionalTile(dirIdx, cascade, dirShadow.LightSpaceMatrices[cascade]);

            // 设置视口和裁剪区域
            RenderCommand::SetViewport(tile.X, tile.Y, tile.Width, tile.Height);
            RenderCommand::SetScissor(tile.X, tile.Y, tile.Width, tile.Height);
            RenderCommand::Clear();

            // 设置 Light Space Matrix
            m_ShadowShader->SetMat4("u_LightSpaceMatrix", dirShadow.LightSpaceMatrices[cascade]);

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
    }

    RenderCommand::SetScissorTest(false);
}
```

### 7.3 Shadow.glsl 方向光 CSM 迁移

```glsl
// 替换原有的 sampler2DArray 采样为 Atlas 采样

#define MAX_DIR_SHADOW_LIGHTS 2

uniform int u_DirShadowCount;                                                   // 投射阴影的方向光数量
uniform int u_DirShadowCascadeCount[MAX_DIR_SHADOW_LIGHTS];                     // 每个方向光的级联数量
uniform mat4 u_DirShadowLightSpaceMatrices[MAX_DIR_SHADOW_LIGHTS * MAX_CASCADE_COUNT];
uniform vec4 u_DirShadowAtlasScaleBias[MAX_DIR_SHADOW_LIGHTS * MAX_CASCADE_COUNT];
uniform float u_DirShadowCascadeFarPlanes[MAX_DIR_SHADOW_LIGHTS * MAX_CASCADE_COUNT];
uniform float u_DirShadowBias[MAX_DIR_SHADOW_LIGHTS];
uniform float u_DirShadowStrength[MAX_DIR_SHADOW_LIGHTS];
uniform int u_DirShadowType[MAX_DIR_SHADOW_LIGHTS];

/// <summary>
/// 选择方向光的级联索引（使用视图空间深度）
/// </summary>
int SelectDirCascadeIndex(vec3 worldPos, int shadowIndex)
{
    vec4 viewPos = u_CameraViewMatrix * vec4(worldPos, 1.0);
    float depth = -viewPos.z;

    int baseIndex = shadowIndex * MAX_CASCADE_COUNT;
    int cascadeCount = u_DirShadowCascadeCount[shadowIndex];

    for (int i = 0; i < cascadeCount; ++i)
    {
        if (depth < u_DirShadowCascadeFarPlanes[baseIndex + i])
        {
            return i;
        }
    }
    return cascadeCount - 1;
}

/// <summary>
/// 计算方向光阴影（Atlas 版本，支持多方向光）
/// 返回 vec3（支持 Translucent Shadow）
/// </summary>
vec3 CalcDirectionalShadow(int shadowIndex, vec3 worldPos, vec3 normal, vec3 lightDir)
{
    int cascadeIndex = SelectDirCascadeIndex(worldPos, shadowIndex);
    int tileIndex = shadowIndex * MAX_CASCADE_COUNT + cascadeIndex;

    mat4 lightSpaceMatrix = u_DirShadowLightSpaceMatrices[tileIndex];
    vec4 atlasScaleBias = u_DirShadowAtlasScaleBias[tileIndex];

    vec3 atlasUV = WorldToAtlasUV(worldPos, lightSpaceMatrix, atlasScaleBias);

    if (atlasUV.z > 1.0)
    {
        return vec3(1.0);
    }

    // 动态 Bias
    float NdotL = dot(normal, lightDir);
    float bias = u_DirShadowBias[shadowIndex] * (1.0 + 9.0 * (1.0 - clamp(NdotL, 0.0, 1.0)));

    float shadow = 0.0;
    if (u_DirShadowType[shadowIndex] == 1)  // Hard
    {
        shadow = AtlasShadowSampleHard(atlasUV, bias);
    }
    else  // Soft
    {
        shadow = AtlasShadowSampleSoft(atlasUV, bias, atlasScaleBias);
    }

    shadow *= u_DirShadowStrength[shadowIndex];
    float baseShadow = 1.0 - shadow;

    // Translucent Shadow（仅方向光 0 支持）
    if (shadowIndex == 0 && u_TranslucentShadowEnabled != 0)
    {
        // 使用原始 projCoords（非 Atlas UV）采样 Translucent Shadow Map Array
        vec4 fragPosLightSpace = lightSpaceMatrix * vec4(worldPos, 1.0);
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;

        vec3 translucentColor;
        if (u_DirShadowType[shadowIndex] == 1)
        {
            translucentColor = texture(u_TranslucentShadowMap, vec3(projCoords.xy, float(cascadeIndex))).rgb;
        }
        else
        {
            // 软采样
            vec3 colorSum = vec3(0.0);
            vec2 texelSize = 1.0 / vec2(textureSize(u_TranslucentShadowMap, 0).xy);
            for (int x = -2; x <= 2; ++x)
            {
                for (int y = -2; y <= 2; ++y)
                {
                    colorSum += texture(u_TranslucentShadowMap, vec3(projCoords.xy + vec2(x, y) * texelSize, float(cascadeIndex))).rgb;
                }
            }
            translucentColor = colorSum / 25.0;
        }
        return min(vec3(baseShadow), translucentColor);
    }

    return vec3(baseShadow);
}

/// <summary>
/// 获取方向光阴影因子（供 Lighting.glsl 调用）
/// 替代原有的 ShadowCalculation()，支持多方向光
/// </summary>
vec3 GetDirectionalLightShadow(int lightIndex, vec3 worldPos, vec3 normal, vec3 lightDir)
{
    if (u_DirShadowCount <= 0)
    {
        return vec3(1.0);
    }

    // 方向光阴影索引 == 光源索引（前 MAX_DIR_SHADOW_LIGHTS 个可投射阴影）
    if (lightIndex >= u_DirShadowCount)
    {
        return vec3(1.0);
    }

    return CalcDirectionalShadow(lightIndex, worldPos, normal, lightDir);
}
```

### 7.4 Lighting.glsl 方向光循环修改

```glsl
// 方向光（替换原有的 i == 0 硬编码）
for (int i = 0; i < u_Lights.DirectionalLightCount; ++i)
{
    vec3 contribution = CalcDirectionalLight(u_Lights.DirectionalLights[i], N, V, albedo, metallic, roughness, F0);

    // 对所有投射阴影的方向光应用阴影（替代原有的 if (i == 0)）
    vec3 lightDir = normalize(-u_Lights.DirectionalLights[i].Direction);
    vec3 shadow = GetDirectionalLightShadow(i, worldPos, N, lightDir);
    contribution *= shadow;

    Lo += contribution;
}
```

---

## 八、删除旧代码

### 8.1 需要删除的内容

| 文件 | 删除内容 |
|------|---------|
| `ShadowPass.h` | `m_CSMFramebuffer` 成员、`GetShadowMapTextureID()` 旧接口 |
| `ShadowPass.cpp` | `RecreateFBOs()` 中 CSM FBO 创建、旧的逐级联渲染循环 |
| `Renderer3D.h` | `SceneLightData` 中旧的单一阴影字段 |
| `Renderer3D.cpp` | `Renderer3DData` 中旧的 CSM 数据字段 |
| `RenderContext.h` | 旧的 `ShadowEnabled`、`ShadowBias`、`CascadeLightSpaceMatrices` 等字段 |
| `OpaquePass.cpp` | 旧的 CSM uniform 设置代码 |
| `TransparentPass.cpp` | 同上 |
| `Shadow.glsl` | 旧的 `sampler2DArray u_ShadowMapArray`、`ShadowCalculation()` 函数 |
| `Lighting.glsl` | 旧的 `if (i == 0 && u_ShadowEnabled != 0)` 代码 |
| `Framebuffer.h` | `DEPTH_COMPONENT_ARRAY` 格式（如果不再使用） |

### 8.2 纹理单元统一

迁移完成后，统一使用纹理单元 **15** 绑定 Shadow Atlas：

| 纹理单元 | 用途（最终） |
|----------|-------------|
| 0-7 | 材质纹理 |
| 8 | Translucent Shadow Map Array（保留，仅方向光 0） |
| 10 | Irradiance Cubemap |
| 11 | Prefilter Cubemap |
| 12 | BRDF LUT |
| 15 | **Shadow Atlas**（统一所有光源阴影） |

---

## 九、Renderer3D 修改

### 9.1 BeginScene() 新增点光源矩阵计算

```cpp
// ======== 点光源阴影矩阵计算 ========
int pointShadowCount = 0;
for (int i = 0; i < lightData.PointLightCount && pointShadowCount < ShadowAtlas::s_MaxPointLightShadows; ++i)
{
    if (lightData.PointLightShadows[i].Shadows != ShadowType::None)
    {
        float farPlane = lightData.PointLights[i].Range;
        glm::vec3 lightPos = lightData.PointLights[i].Position;

        // 计算 6 面 Light Space Matrix
        std::array<glm::mat4, 6> matrices = CalcPointLightMatrices(lightPos, farPlane);

        s_Data.PointShadowData[pointShadowCount].LightIndex = i;
        s_Data.PointShadowData[pointShadowCount].FarPlane = farPlane;
        s_Data.PointShadowData[pointShadowCount].LightPos = lightPos;
        s_Data.PointShadowData[pointShadowCount].ShadowBias = lightData.PointLightShadows[i].ShadowBias;
        s_Data.PointShadowData[pointShadowCount].ShadowStrength = lightData.PointLightShadows[i].ShadowStrength;
        s_Data.PointShadowData[pointShadowCount].ShadowType = static_cast<int>(lightData.PointLightShadows[i].Shadows);

        for (int face = 0; face < 6; ++face)
        {
            s_Data.PointShadowData[pointShadowCount].LightSpaceMatrices[face] = matrices[face];
        }

        ++pointShadowCount;
    }
}
s_Data.PointShadowCount = pointShadowCount;
```

### 9.2 Renderer3DData 新增字段

```cpp
// ======== 点光源阴影数据 ========
struct PointShadowCacheData
{
    int LightIndex = -1;
    glm::vec3 LightPos = glm::vec3(0.0f);
    float FarPlane = 25.0f;
    glm::mat4 LightSpaceMatrices[6];
    float ShadowBias = 0.05f;
    float ShadowStrength = 1.0f;
    int ShadowType = 1;
};
PointShadowCacheData PointShadowData[ShadowAtlas::s_MaxPointLightShadows];
int PointShadowCount = 0;
```

---

## 十、性能优化

### 10.1 距离裁剪

超出点光源 Range 的物体不需要渲染到 Shadow Map：

```cpp
// 在 RenderPointLightShadows() 中，渲染物体前检查距离
for (const DrawCommand& cmd : *context.OpaqueDrawCommands)
{
    // 简单的距离检查（使用物体中心点）
    glm::vec3 objPos = glm::vec3(cmd.Transform[3]);
    float distance = glm::length(objPos - lightPos);

    // 超出 Range + 安全边距的物体跳过
    if (distance > farPlane * 1.5f)
    {
        continue;
    }

    m_PointShadowShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);
    RenderCommand::DrawIndexedRange(...);
}
```

### 10.2 可见面裁剪（后续优化）

如果相机在点光源的某一侧，对面的 Shadow Map 面可能完全不可见，可以跳过渲染。这是一个后续优化项，当前阶段先渲染所有 6 面。

---

## 十一、涉及的文件清单

### 需要新增的文件

| 文件路径 | 内容 |
|---------|------|
| `Luck3DApp/Assets/Shaders/Internal/Shadow/PointShadow.vert` | 点光源 Shadow 顶点着色器 |
| `Luck3DApp/Assets/Shaders/Internal/Shadow/PointShadow.frag` | 点光源 Shadow 片段着色器（线性深度） |

### 需要修改的文件

| 文件路径 | 修改内容 |
|---------|----------|
| `Lucky/Source/Lucky/Renderer/Passes/ShadowPass.h` | 新增 `m_PointShadowShader`、`RenderPointLightShadows()`、删除旧 CSM FBO |
| `Lucky/Source/Lucky/Renderer/Passes/ShadowPass.cpp` | 实现点光源渲染、CSM 迁移到 Atlas、删除旧代码 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.h` | 删除 `SceneLightData` 旧字段 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | 新增点光源矩阵计算、CSM 迁移、删除旧代码 |
| `Lucky/Source/Lucky/Renderer/RenderContext.h` | 删除旧阴影字段 |
| `Lucky/Source/Lucky/Renderer/Passes/OpaquePass.cpp` | 统一使用 Atlas 纹理单元 15、新增点光源 uniform |
| `Lucky/Source/Lucky/Renderer/Passes/TransparentPass.cpp` | 同上 |
| `Lucky/Source/Lucky/Renderer/Material.cpp` | 黑名单新增点光源 + 方向光 Atlas uniform |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | 收集点光源阴影参数 |
| `Luck3DApp/Assets/Shaders/Lucky/Shadow.glsl` | 完全重写为 Atlas 版本 |
| `Luck3DApp/Assets/Shaders/Lucky/Lighting.glsl` | 所有光源循环添加阴影 |

### 需要删除的文件

无（旧 Shader 文件保留，内容被替换）

---

## 十二、实施步骤

| 步骤 | 内容 | 验证方式 |
|------|------|---------|
| 1 | 创建 `PointShadow.vert` 和 `PointShadow.frag` | Shader 编译通过 |
| 2 | `ShadowPass.h/cpp` 新增 `m_PointShadowShader` 和 `RenderPointLightShadows()` | 编译通过 |
| 3 | `Renderer3D.cpp` 新增 `CalcPointLightMatrices()` 和 `PointShadowCacheData` | 编译通过 |
| 4 | `Renderer3D.cpp` `BeginScene()` 计算点光源矩阵 | 编译通过 |
| 5 | `Renderer3D.cpp` `EndScene()` 构建点光源 RenderContext | 编译通过 |
| 6 | `Scene.cpp` 收集点光源阴影参数 | 编译通过 |
| 7 | `Shadow.glsl` 新增点光源阴影采样 | Shader 编译通过 |
| 8 | `Lighting.glsl` 点光源循环添加阴影 | Shader 编译通过 |
| 9 | `OpaquePass.cpp` / `TransparentPass.cpp` 新增点光源 uniform | 编译通过 |
| 10 | 运行测试：点光源阴影正确 | 视觉验证 |
| 11 | **CSM 迁移**：修改 `RenderDirectionalLightCSM()` 使用 Atlas | 编译通过 |
| 12 | `Shadow.glsl` 方向光改为 Atlas 采样 | Shader 编译通过 |
| 13 | `Lighting.glsl` 方向光循环改为 `GetDirectionalLightShadow()` | Shader 编译通过 |
| 14 | 删除旧 CSM Texture2DArray 相关代码 | 编译通过 |
| 15 | 删除 `SceneLightData` 和 `RenderContext` 旧字段 | 编译通过 |
| 16 | 统一纹理单元（Atlas 使用 15，删除 14） | 编译通过 |
| 17 | 全面测试：方向光 + 聚光灯 + 点光源阴影同时工作 | 视觉验证 |
| 18 | 测试：多方向光阴影 | 视觉验证 |

---

## 十三、设计决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 深度存储方案 | 线性深度（方案 A） | 精度均匀，业界标准，远处阴影质量好 |
| 点光源 Shader | 专用 PointShadow.vert/frag | 需要计算线性深度，不能复用标准 Shadow Shader |
| 面选择算法 | 主轴选择（绝对值最大） | 简单高效，无需额外数据 |
| CSM 迁移时机 | 与点光源阴影同阶段完成 | 统一系统，避免长期维护两套代码 |
| Translucent Shadow | 保持独立 FBO | 仅方向光 0 使用，不值得迁移到 Atlas |

---

## 十四、已知限制与后续优化

| 限制 | 影响 | 后续优化方案 |
|------|------|-------------|
| 每个点光源 6 次渲染循环 | 性能开销大 | 可见面裁剪（仅渲染相机可见的面） |
| 无距离 LOD | 远处点光源仍使用 512×512 | 动态分辨率（远处降低） |
| 线性深度 PCF | PCF 在线性深度空间中的模糊效果略有不同 | 可调整 PCF 核大小 |
| 点光源不支持 Translucent Shadow | 透明物体不会在点光源下产生彩色阴影 | 后续可为点光源添加颜色通道 |
| 最多 2 个点光源投射阴影 | 场景中点光源多时部分无阴影 | 增大 Atlas 或使用动态分配 |
| 无阴影缓存 | 静态场景每帧重新渲染 | 静态物体阴影缓存 |

---

## 十五、最终 Shadow.glsl 结构概览

完成本阶段后，`Shadow.glsl` 的整体结构如下：

```glsl
#ifndef LUCKY_SHADOW_GLSL
#define LUCKY_SHADOW_GLSL

// ---- 常量 ----
#define MAX_CASCADE_COUNT 4
#define MAX_DIR_SHADOW_LIGHTS 2
#define MAX_SPOT_SHADOW_LIGHTS 4
#define MAX_POINT_SHADOW_LIGHTS 2

// ---- Shadow Atlas 纹理 ----
uniform sampler2D u_ShadowAtlas;
uniform float u_ShadowAtlasSize;
uniform int u_ShadowEnabled;
uniform mat4 u_CameraViewMatrix;

// ---- 方向光阴影 uniform ----
// ...

// ---- 聚光灯阴影 uniform ----
// ...

// ---- 点光源阴影 uniform ----
// ...

// ---- Translucent Shadow Map（保留） ----
uniform sampler2DArray u_TranslucentShadowMap;
uniform int u_TranslucentShadowEnabled;

// ---- 调试 ----
uniform int u_DebugCSMVisualize;

// ==================== 工具函数 ====================
vec3 WorldToAtlasUV(...);
float AtlasShadowSampleHard(...);
float AtlasShadowSampleSoft(...);

// ==================== 方向光 CSM ====================
int SelectDirCascadeIndex(...);
vec3 CalcDirectionalShadow(...);
vec3 GetDirectionalLightShadow(...);

// ==================== 聚光灯 ====================
float CalcSpotLightShadow(...);
float GetSpotLightShadow(...);

// ==================== 点光源 ====================
int SelectPointLightFace(...);
float CalcPointLightShadow(...);
float GetPointLightShadow(...);

// ==================== 调试 ====================
vec3 GetCascadeDebugColor(...);

#endif // LUCKY_SHADOW_GLSL
```
