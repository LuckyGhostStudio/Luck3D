# PhaseR26：环境设置系统（Environment Settings）

> 本文档描述对标 Unity Lighting 面板 Environment 设置的完整实现方案。
> 目标：为 IBL 环境光照提供用户可调参数，解决 IBL 过亮问题，并在 LightingPanel 中提供统一的环境设置 UI。

---

## 目录

1. [需求分析](#1-需求分析)
2. [Unity 参考分析](#2-unity-参考分析)
3. [架构概览](#3-架构概览)
4. [数据模型设计](#4-数据模型设计)
5. [Shader 层修改](#5-shader-层修改)
6. [C++ 渲染层修改](#6-c-渲染层修改)
7. [编辑器 UI 层修改](#7-编辑器-ui-层修改)
8. [序列化支持](#8-序列化支持)
9. [实现方式对比](#9-实现方式对比)
10. [涉及文件清单](#10-涉及文件清单)
11. [实现步骤](#11-实现步骤)
12. [验证方法](#12-验证方法)

---

## 1. 需求分析

### 1.1 当前问题

1. **IBL 过亮**：HDR 环境图的辐照度值直接参与光照计算，无强度控制参数，导致开启 IBL 后物体整体偏白
2. **环境光不可调**：IBL 关闭时的环境光强度硬编码为 `AMBIENT_STRENGTH = 0.1`（`Common.glsl`），无法在编辑器中调整
3. **缺少环境光来源切换**：无法在 "Skybox（IBL）" 和 "纯色" 之间切换环境光来源
4. **LightingPanel 功能不完整**：当前 LightingPanel 仅显示天空盒材质名称和材质编辑器，缺少 Environment Lighting / Environment Reflections 设置区域
5. **IBL 预计算参数硬编码**：Irradiance Map 分辨率（32）、Prefilter Map 分辨率（128）等参数硬编码在 `IBLPrecompute.cpp` 中，无法在编辑器中调整

### 1.2 目标

- 新增 `EnvironmentSettings` 数据结构，集中管理所有环境光照参数
- 在 Shader 中新增 IBL 漫反射/镜面反射强度 uniform，解决 IBL 过亮问题
- 在 LightingPanel 中提供对标 Unity 的 Environment 设置 UI
- 支持环境光来源切换（Skybox / Color）
- 支持序列化/反序列化（保存场景后恢复环境设置）
- 遵循项目现有的数据流模式（Component → Scene 收集 → Renderer3D → RenderContext → Pass → Shader）

### 1.3 约束

- 遵循项目代码规范（[Coding_Style_Guide.md](../Coding_Style_Guide.md)）
- 复用现有 UI 组件（`PropertyFloat`、`PropertyColor`、`PropertyCombo` 等）
- 不破坏现有渲染管线的执行流程
- 保持向后兼容（默认参数值应使当前渲染效果不变）

---

## 2. Unity 参考分析

### 2.1 Unity Lighting 面板 Environment 区域

Unity 的 Environment 设置分为三大块：

| 分类 | 参数 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| **基础** | Skybox Material | Material | None | 天空盒材质 |
| | Sun Source | Light | None | 太阳光源（关联 Directional Light） |
| **Environment Lighting** | Source | Enum | Skybox | 环境光来源：Skybox / Gradient / Color |
| | Ambient Color | Color | — | Source=Color 时的纯色环境光 |
| | Ambient Mode | Enum | Realtime | Realtime / Baked |
| **Environment Reflections** | Source | Enum | Skybox | 反射来源：Skybox / Custom |
| | Resolution | Enum | 128 | 反射 Cubemap 分辨率 |
| | Compression | Enum | Auto | 压缩模式 |
| | Intensity Multiplier | Float | 1.0 | 反射强度乘数 |
| | Bounces | Int | 1 | 反射弹射次数 |

### 2.2 本项目需要实现的参数

根据项目当前阶段和实际需求，精简为以下参数：

| 分类 | 参数 | 类型 | 默认值 | 对应 Unity | 优先级 |
|------|------|------|--------|------------|--------|
| **基础** | Skybox Material | Material | 当前天空盒 | Skybox Material | 已有 |
| **Environment Lighting** | Source | Enum | Skybox | Source | ?? 高 |
| | Ambient Color | Color3 | (0.1, 0.1, 0.1) | Ambient Color | ?? 高 |
| | Diffuse Intensity | Float | 1.0 | 隐含强度 | ?? 高 |
| **Environment Reflections** | Specular Intensity | Float | 1.0 | Intensity Multiplier | ?? 高 |
| | Resolution | Enum | 128 | Resolution | ?? 中 |

**不实现的参数**（当前阶段不需要）：
- Sun Source：Procedural 天空盒需要，当前不支持
- Ambient Mode（Realtime/Baked）：当前仅支持 Realtime
- Gradient 模式：三色渐变环境光，复杂度较高，优先级低
- Compression：当前不需要压缩
- Bounces：多次反射弹射，复杂度极高

---

## 3. 架构概览

### 3.1 数据流

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           数据流（对标 PostProcess 模式）                      │
│                                                                             │
│  EnvironmentSettings (数据结构)                                              │
│       │                                                                     │
│       ├── LightingPanel::OnGUI()  ←── 编辑器 UI 修改参数                     │
│       │                                                                     │
│       ├── Renderer3D::SetEnvironmentSettings()  ←── Scene 每帧传递           │
│       │       │                                                             │
│       │       ▼                                                             │
│       │  Renderer3DData::Environment  (缓存)                                │
│       │       │                                                             │
│       │       ▼                                                             │
│       │  RenderContext::Environment*  (指针传递)                              │
│       │       │                                                             │
│       │       ├── OpaquePass::Execute()  ──→  设置 IBL uniform              │
│       │       └── TransparentPass::Execute()  ──→  设置 IBL uniform         │
│       │                                                                     │
│       └── Shader uniform                                                    │
│            ├── u_IBLDiffuseIntensity   (漫反射 IBL 强度)                     │
│            ├── u_IBLSpecularIntensity  (镜面反射 IBL 强度)                    │
│            ├── u_AmbientColor          (纯色环境光颜色)                       │
│            └── u_AmbientSource         (环境光来源)                           │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 与现有系统的关系

```
┌──────────────────────────────────────────────────────────────────┐
│                        LightingPanel                             │
│  ┌────────────────┐  ┌──────────────────┐  ┌────────────────┐   │
│  │ Environment    │  │ Environment      │  │ Environment    │   │
│  │ (基础)         │  │ Lighting         │  │ Reflections    │   │
│  │                │  │                  │  │                │   │
│  │ Skybox Material│  │ Source           │  │ Specular       │   │
│  │                │  │ Ambient Color    │  │ Intensity      │   │
│  │                │  │ Diffuse Intensity│  │ Resolution     │   │
│  └────────────────┘  └──────────────────┘  └────────────────┘   │
│                              │                                   │
│                              ▼                                   │
│                    EnvironmentSettings                            │
└──────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────────┐
│                         Scene::OnUpdate()                        │
│                                                                  │
│  收集 EnvironmentSettings → Renderer3D::SetEnvironmentSettings() │
└──────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────────┐
│                      Renderer3D::EndScene()                      │
│                                                                  │
│  构建 RenderContext，填充 Environment 参数                         │
└──────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────────┐
│              OpaquePass / TransparentPass                         │
│                                                                  │
│  设置 IBL uniform → Shader 使用参数计算环境光                      │
└──────────────────────────────────────────────────────────────────┘
```

---

## 4. 数据模型设计

### 4.1 环境光来源枚举

**文件位置**：`Lucky/Source/Lucky/Renderer/RenderContext.h`（在 `PostProcessSettings` 之前）

```cpp
/// <summary>
/// 环境光来源
/// </summary>
enum class AmbientSource
{
    Skybox = 0,     // 从天空盒 IBL 获取环境光（默认）
    Color = 1       // 使用纯色环境光
};
```

### 4.2 EnvironmentSettings 结构体

**文件位置**：`Lucky/Source/Lucky/Renderer/RenderContext.h`（在 `AmbientSource` 之后、`PostProcessSettings` 之前）

```cpp
/// <summary>
/// 环境设置参数（对标 Unity Lighting 面板 Environment 区域）
/// 由 LightingPanel 编辑，Scene 每帧传递给 Renderer3D
/// </summary>
struct EnvironmentSettings
{
    // ---- Environment Lighting ----
    AmbientSource Source = AmbientSource::Skybox;           // 环境光来源（默认 Skybox）
    glm::vec3 AmbientColor = glm::vec3(0.1f, 0.1f, 0.1f); // 纯色环境光颜色（Source=Color 时使用）
    float DiffuseIntensity = 1.0f;                          // IBL 漫反射强度（Source=Skybox 时使用）

    // ---- Environment Reflections ----
    float SpecularIntensity = 1.0f;                         // IBL 镜面反射强度（对应 Unity 的 Intensity Multiplier）
    int ReflectionResolution = 128;                         // 反射 Cubemap 分辨率（Prefilter Map）
};
```

### 4.3 数据存储位置

**核心问题**：`EnvironmentSettings` 应该存储在哪里？

#### 方案 A：存储在 Scene 中（类似 PostProcessVolumeComponent 的收集模式）【? 推荐 - 最优】

在 `Scene` 类中添加 `EnvironmentSettings` 成员，由 `LightingPanel` 直接修改，`Scene::OnUpdate()` 中传递给 `Renderer3D`。

**数据流**：
```
LightingPanel → Scene::m_EnvironmentSettings → Scene::OnUpdate() 
→ Renderer3D::SetEnvironmentSettings() → RenderContext → Pass → Shader
```

- **优点**：
  - 与 PostProcess 参数的数据流模式一致（Scene 持有数据，每帧传递给 Renderer3D）
  - 序列化时随 Scene 一起保存/加载
  - LightingPanel 通过 `Scene*` 引用直接修改，无需额外的中间层
  - 生命周期与 Scene 一致，场景切换时自动重置
- **缺点**：
  - Scene 类增加了一个成员（但 Scene 本身就是数据容器，合理）

**实现代码**（在 `Scene.h` 中添加）：

```cpp
private:
    EnvironmentSettings m_EnvironmentSettings;  // 环境设置

public:
    /// <summary>
    /// 获取环境设置（可修改，供 LightingPanel 使用）
    /// </summary>
    EnvironmentSettings& GetEnvironmentSettings() { return m_EnvironmentSettings; }
    
    /// <summary>
    /// 获取环境设置（只读）
    /// </summary>
    const EnvironmentSettings& GetEnvironmentSettings() const { return m_EnvironmentSettings; }
```

#### 方案 B：创建 EnvironmentComponent 组件【? 不推荐】

创建一个 `EnvironmentComponent`，挂载到场景中的某个实体上。

- **优点**：符合 ECS 模式
- **缺点**：
  - 环境设置是全局的，不应该依附于某个实体
  - 需要额外的"查找持有该组件的实体"逻辑
  - 如果实体被删除，环境设置丢失
  - 与 Unity 的设计不一致（Unity 的 Environment 设置也不是组件）

#### 方案 C：存储在 Renderer3D 的静态数据中【? 其次】

在 `Renderer3DData` 中添加 `EnvironmentSettings` 成员，通过 `Renderer3D::SetEnvironmentSettings()` 设置。

- **优点**：不修改 Scene 类
- **缺点**：
  - 序列化时需要额外处理（Renderer3D 是全局单例，不随场景保存）
  - 场景切换时不会自动重置
  - 与 PostProcess 的数据流不一致（PostProcess 存储在 Scene 的 Component 中）

### 4.4 RenderContext 扩展

**文件位置**：`Lucky/Source/Lucky/Renderer/RenderContext.h`

在 `RenderContext` 结构体的 IBL 数据区域扩展：

```cpp
// ---- IBL 数据 ----
bool IBLEnabled = false;                    // 是否启用 IBL
uint32_t IrradianceMapID = 0;               // Irradiance Cubemap 纹理 ID
uint32_t PrefilterMapID = 0;                // Prefiltered Environment Cubemap 纹理 ID
uint32_t BRDFLUTID = 0;                     // BRDF LUT Texture2D 纹理 ID
float PrefilterMaxMipLevel = 4.0f;          // Prefiltered Map 最大 Mip Level

// ---- 环境设置（新增） ----
AmbientSource EnvironmentSource = AmbientSource::Skybox;    // 环境光来源
glm::vec3 AmbientColor = glm::vec3(0.1f, 0.1f, 0.1f);     // 纯色环境光颜色
float IBLDiffuseIntensity = 1.0f;                           // IBL 漫反射强度
float IBLSpecularIntensity = 1.0f;                          // IBL 镜面反射强度
```

---

## 5. Shader 层修改

### 5.1 Lighting.glsl 修改

**文件位置**：`Luck3DApp/Assets/Shaders/Lucky/Lighting.glsl`

#### 5.1.1 新增 uniform 声明

在现有 IBL uniform 声明区域之后添加：

```glsl
// ---- IBL 纹理（由 OpaquePass / TransparentPass 绑定） ----
uniform samplerCube u_IrradianceMap;    // Irradiance Cubemap（漫反射 IBL）
uniform samplerCube u_PrefilterMap;     // Prefiltered Environment Cubemap（镜面反射 IBL）
uniform sampler2D u_BRDFLUT;            // BRDF LUT（Fresnel 积分查找表）
uniform int u_IBLEnabled;               // IBL 开关（0 = 关闭, 1 = 开启）
uniform float u_PrefilterMaxMipLevel;   // Prefiltered Map 最大 Mip Level

// ---- 环境设置 uniform（新增） ----
uniform int u_AmbientSource;            // 环境光来源（0 = Skybox, 1 = Color）
uniform vec3 u_AmbientColor;            // 纯色环境光颜色（Source=Color 时使用）
uniform float u_IBLDiffuseIntensity;    // IBL 漫反射强度乘数（默认 1.0）
uniform float u_IBLSpecularIntensity;   // IBL 镜面反射强度乘数（默认 1.0）
```

#### 5.1.2 修改 CalcIBLAmbient 函数

将现有的 `CalcIBLAmbient` 函数修改为支持强度参数：

```glsl
/// <summary>
/// 计算 IBL 环境光（漫反射 + 镜面反射）
/// </summary>
vec3 CalcIBLAmbient(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0, float ao)
{
    float NdotV = max(dot(N, V), 0.0);
    
    // Fresnel（使用粗糙度修正版）
    vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    
    // 漫反射/镜面反射比例
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);
    
    // ---- 漫反射 IBL ----
    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    vec3 diffuseIBL = kD * irradiance * albedo * u_IBLDiffuseIntensity;
    
    // ---- 镜面反射 IBL ----
    vec3 R = reflect(-V, N);
    float lod = roughness * u_PrefilterMaxMipLevel;
    vec3 prefilteredColor = textureLod(u_PrefilterMap, R, lod).rgb;
    vec2 brdf = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y) * u_IBLSpecularIntensity;
    
    // ---- 合成 ----
    return (diffuseIBL + specularIBL) * ao;
}
```

**关键变化**：
- `diffuseIBL` 乘以 `u_IBLDiffuseIntensity`
- `specularIBL` 乘以 `u_IBLSpecularIntensity`

### 5.2 Standard.frag 修改

**文件位置**：`Luck3DApp/Assets/Shaders/Standard.frag`

修改环境光计算部分：

```glsl
// ---- 环境光（根据 Source 选择 IBL 或纯色） ----
vec3 ambient;
if (u_AmbientSource == 0 && u_IBLEnabled != 0)
{
    // Source = Skybox：使用 IBL 环境光
    ambient = CalcIBLAmbient(N, V, albedo, metallic, roughness, F0, ao);
}
else if (u_AmbientSource == 1)
{
    // Source = Color：使用纯色环境光
    ambient = u_AmbientColor * albedo * ao;
}
else
{
    // Fallback：IBL 未就绪时使用纯色环境光
    ambient = u_AmbientColor * albedo * ao;
}
```

### 5.3 Common.glsl 修改

**文件位置**：`Luck3DApp/Assets/Shaders/Lucky/Common.glsl`

移除硬编码的 `AMBIENT_STRENGTH` 常量：

```glsl
// 移除以下行：
// const float AMBIENT_STRENGTH = 0.1;   // 常量环境光强度（无 IBL 时的替代方案）
```

> **说明**：`AMBIENT_STRENGTH` 被 `u_AmbientColor` uniform 替代。如果其他 Shader 也使用了 `AMBIENT_STRENGTH`，需要同步修改。

#### 方案 A：直接移除 AMBIENT_STRENGTH【? 推荐 - 最优】

完全移除 `AMBIENT_STRENGTH`，所有环境光强度由 `u_AmbientColor` 控制。

- **优点**：消除硬编码；所有环境光参数统一由 uniform 控制
- **缺点**：需要确保所有使用 `AMBIENT_STRENGTH` 的 Shader 都已迁移

#### 方案 B：保留 AMBIENT_STRENGTH 作为 fallback【? 其次】

保留 `AMBIENT_STRENGTH`，仅在 `u_AmbientColor` 未设置时使用。

- **优点**：向后兼容
- **缺点**：两套环境光参数共存，容易混淆

---

## 6. C++ 渲染层修改

### 6.1 Renderer3D 新增接口

**文件位置**：`Lucky/Source/Lucky/Renderer/Renderer3D.h`

```cpp
/// <summary>
/// 设置环境设置参数（由 Scene 每帧调用）
/// </summary>
static void SetEnvironmentSettings(const EnvironmentSettings& settings);
```

**文件位置**：`Lucky/Source/Lucky/Renderer/Renderer3D.cpp`

在 `Renderer3DData` 中添加：

```cpp
// ======== 环境设置 ========
EnvironmentSettings Environment;    // 环境设置参数（由 Scene 传入）
```

实现 `SetEnvironmentSettings()`：

```cpp
void Renderer3D::SetEnvironmentSettings(const EnvironmentSettings& settings)
{
    s_Data.Environment = settings;
}
```

### 6.2 RenderContext 构建

**文件位置**：`Lucky/Source/Lucky/Renderer/Renderer3D.cpp` 的 `EndScene()` 方法

在构建 RenderContext 的 IBL 数据区域之后添加：

```cpp
// 环境设置
context.EnvironmentSource = s_Data.Environment.Source;
context.AmbientColor = s_Data.Environment.AmbientColor;
context.IBLDiffuseIntensity = s_Data.Environment.DiffuseIntensity;
context.IBLSpecularIntensity = s_Data.Environment.SpecularIntensity;
```

### 6.3 OpaquePass 修改

**文件位置**：`Lucky/Source/Lucky/Renderer/Passes/OpaquePass.cpp`

在设置 IBL uniform 的区域之后添加环境设置 uniform：

```cpp
// 设置 IBL 相关 uniform
cmd.MaterialData->GetShader()->SetInt("u_IBLEnabled", context.IBLEnabled ? 1 : 0);
cmd.MaterialData->GetShader()->SetInt("u_IrradianceMap", 10);
cmd.MaterialData->GetShader()->SetInt("u_PrefilterMap", 11);
cmd.MaterialData->GetShader()->SetInt("u_BRDFLUT", 12);
cmd.MaterialData->GetShader()->SetFloat("u_PrefilterMaxMipLevel", context.PrefilterMaxMipLevel);

// 设置环境设置 uniform（新增）
cmd.MaterialData->GetShader()->SetInt("u_AmbientSource", static_cast<int>(context.EnvironmentSource));
cmd.MaterialData->GetShader()->SetFloat3("u_AmbientColor", context.AmbientColor);
cmd.MaterialData->GetShader()->SetFloat("u_IBLDiffuseIntensity", context.IBLDiffuseIntensity);
cmd.MaterialData->GetShader()->SetFloat("u_IBLSpecularIntensity", context.IBLSpecularIntensity);
```

### 6.4 TransparentPass 修改

**文件位置**：`Lucky/Source/Lucky/Renderer/Passes/TransparentPass.cpp`

与 OpaquePass 相同，在设置 IBL uniform 之后添加相同的环境设置 uniform 代码。

### 6.5 Material 黑名单更新

**文件位置**：`Lucky/Source/Lucky/Renderer/Material.cpp`

在 IBL uniform 黑名单中添加新的环境设置 uniform：

```cpp
// 环境设置 uniform（引擎内部设置，不在材质面板中显示）
"u_AmbientSource",
"u_AmbientColor",
"u_IBLDiffuseIntensity",
"u_IBLSpecularIntensity",
```

### 6.6 IBL 预计算分辨率可配置（可选）

**文件位置**：`Lucky/Source/Lucky/Renderer/IBLPrecompute.h` 和 `IBLPrecompute.cpp`

#### 方案 A：通过 GenerateFromCubemap 参数传递分辨率【? 推荐 - 最优】

修改 `GenerateFromCubemap` 的签名：

```cpp
/// <summary>
/// 从环境 Cubemap 生成 Irradiance Map 和 Prefiltered Map
/// </summary>
/// <param name="envCubemapID">原始环境 Cubemap 的 OpenGL 纹理 ID</param>
/// <param name="prefilterResolution">Prefilter Map 分辨率（默认 128）</param>
static void GenerateFromCubemap(uint32_t envCubemapID, int prefilterResolution = 128);
```

在 `GeneratePrefilterMap` 中使用传入的分辨率替代硬编码的 `128`。

- **优点**：接口简洁；调用方可以控制分辨率
- **缺点**：每次变更分辨率需要重新生成 IBL 数据

#### 方案 B：在 IBLPrecompute 中存储配置【? 其次】

在 `IBLPrecomputeData` 中添加配置字段，通过 `SetConfig()` 方法设置。

- **优点**：配置与生成分离
- **缺点**：增加了状态管理复杂度

### 6.7 Scene 中传递环境设置

**文件位置**：`Lucky/Source/Lucky/Scene/Scene.cpp`

在 `Scene::OnUpdate()` 中，`Renderer3D::BeginScene()` 之后、`EndScene()` 之前添加：

```cpp
// ---- 传递环境设置 ----
Renderer3D::SetEnvironmentSettings(m_EnvironmentSettings);
```

位置应在 `Renderer3D::SetPostProcessSettings()` 附近，保持一致的数据传递模式。

---

## 7. 编辑器 UI 层修改

### 7.1 LightingPanel 重构

**文件位置**：`Luck3DApp/Source/Panels/LightingPanel.h`

#### 7.1.1 头文件修改

```cpp
#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Renderer/Material.h"

namespace Lucky
{
    class Scene;    // 前向声明
    
    class LightingPanel : public EditorPanel
    {
    public:
        LightingPanel() = default;
        ~LightingPanel() override = default;
        
        void OnUpdate(DeltaTime dt) override;
        void OnGUI() override;
        void OnEvent(Event& event) override;
        
        /// <summary>
        /// 设置当前场景引用（场景切换时调用）
        /// </summary>
        void SetScene(Scene* scene) { m_Scene = scene; }
        
    private:
        Scene* m_Scene = nullptr;   // 当前场景引用（非拥有）
    };
}
```

**关键变化**：
- 移除 `Ref<Material> m_SkyboxMaterial` 成员（改为从 Scene 或 Renderer3D 获取）
- 新增 `Scene* m_Scene` 成员（用于访问 EnvironmentSettings）
- 构造函数改为默认构造（不再需要传入天空盒材质）

#### 方案 A：LightingPanel 持有 Scene* 引用【? 推荐 - 最优】

LightingPanel 通过 `Scene*` 访问 `EnvironmentSettings`，与 InspectorPanel 访问组件的模式一致。

- **优点**：
  - 直接修改 Scene 中的数据，无需中间层
  - 场景切换时只需更新 `Scene*` 指针
  - 序列化时数据自然随 Scene 保存
- **缺点**：
  - LightingPanel 依赖 Scene 类（但这是合理的依赖）

#### 方案 B：LightingPanel 持有 EnvironmentSettings 副本 + 回调【? 其次】

LightingPanel 持有 `EnvironmentSettings` 的副本，修改后通过回调通知外部。

- **优点**：LightingPanel 不依赖 Scene
- **缺点**：
  - 需要额外的同步机制
  - 回调管理复杂
  - 与项目现有模式不一致

#### 方案 C：通过 Renderer3D 静态接口读写【? 不推荐】

LightingPanel 通过 `Renderer3D::GetEnvironmentSettings()` / `SetEnvironmentSettings()` 读写。

- **优点**：不依赖 Scene
- **缺点**：
  - Renderer3D 是渲染层，不应该存储编辑器状态
  - 序列化困难（Renderer3D 不随场景保存）

### 7.2 LightingPanel::OnGUI() 实现

**文件位置**：`Luck3DApp/Source/Panels/LightingPanel.cpp`

```cpp
#include "LightingPanel.h"

#include "Lucky/UI/Widgets.h"
#include "Lucky/UI/PropertyGrid.h"
#include "Lucky/UI/DrawUtils.h"

#include "Lucky/Editor/MaterialEditor.h"
#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Scene/Scene.h"

namespace Lucky
{
    void LightingPanel::OnUpdate(DeltaTime dt)
    {
    }

    void LightingPanel::OnGUI()
    {
        if (!m_Scene)
        {
            return;
        }
        
        EnvironmentSettings& env = m_Scene->GetEnvironmentSettings();
        
        ImGui::Spacing();
        
        // ======== Environment 区域 ========
        if (UI::BeginPrimaryCollapsing("Environment"))
        {
            // ---- Skybox Material ----
            Ref<Material> skyboxMaterial = Renderer3D::GetSkyboxMaterial();
            const std::string& skyboxMaterialName = skyboxMaterial ? skyboxMaterial->GetName() : "None (Material)";
            UI::PropertyObject("Skybox Material", skyboxMaterialName.c_str());
            
            UI::EndPrimaryCollapsing();
        }
        
        // ======== Environment Lighting 区域 ========
        if (UI::BeginPrimaryCollapsing("Environment Lighting"))
        {
            // ---- Source 下拉框 ----
            const char* sources[] = { "Skybox", "Color" };
            int currentSource = static_cast<int>(env.Source);
            if (UI::PropertyCombo("Source", currentSource, sources, 2))
            {
                env.Source = static_cast<AmbientSource>(currentSource);
            }
            
            if (env.Source == AmbientSource::Skybox)
            {
                // ---- Diffuse Intensity（仅 Skybox 模式显示） ----
                UI::PropertyFloat("Diffuse Intensity", env.DiffuseIntensity, 0.01f, 0.0f, 5.0f);
            }
            else
            {
                // ---- Ambient Color（仅 Color 模式显示） ----
                UI::PropertyColor("Ambient Color", env.AmbientColor);
            }
            
            UI::EndPrimaryCollapsing();
        }
        
        // ======== Environment Reflections 区域 ========
        if (UI::BeginPrimaryCollapsing("Environment Reflections"))
        {
            // ---- Specular Intensity ----
            UI::PropertyFloat("Intensity Multiplier", env.SpecularIntensity, 0.01f, 0.0f, 5.0f);
            
            // ---- Resolution 下拉框 ----
            const char* resolutions[] = { "64", "128", "256", "512" };
            int resIndex = 1;  // 默认 128
            switch (env.ReflectionResolution)
            {
                case 64:  resIndex = 0; break;
                case 128: resIndex = 1; break;
                case 256: resIndex = 2; break;
                case 512: resIndex = 3; break;
            }
            if (UI::PropertyCombo("Resolution", resIndex, resolutions, 4))
            {
                int resValues[] = { 64, 128, 256, 512 };
                env.ReflectionResolution = resValues[resIndex];
                // TODO 分辨率变更时重新生成 IBL 数据
            }
            
            UI::EndPrimaryCollapsing();
        }
        
        // ======== 天空盒材质编辑器 ========
        Ref<Material> skyboxMaterial = Renderer3D::GetSkyboxMaterial();
        if (skyboxMaterial)
        {
            MaterialEditor::OnGUI(skyboxMaterial);
        }
        
        UI::Draw::HorizontalLine();
    }

    void LightingPanel::OnEvent(Event& event)
    {
    }
}
```

### 7.3 LightingPanel 初始化修改

**文件位置**：需要修改 LightingPanel 的创建位置（通常在 `EditorLayer.cpp` 中）

将原来的：
```cpp
auto lightingPanel = CreateRef<LightingPanel>(Renderer3D::GetSkyboxMaterial());
```

改为：
```cpp
auto lightingPanel = CreateRef<LightingPanel>();
lightingPanel->SetScene(m_ActiveScene.get());
```

并在场景切换时更新：
```cpp
lightingPanel->SetScene(newScene.get());
```

#### 方案 A：在 EditorLayer 中手动管理 Scene 引用【? 推荐 - 最优】

在 `EditorLayer` 的场景切换逻辑中，手动调用 `lightingPanel->SetScene()`。

- **优点**：显式控制，逻辑清晰
- **缺点**：需要在每个场景切换点都调用

#### 方案 B：LightingPanel 通过 PanelManager 获取当前 Scene【? 其次】

LightingPanel 在 `OnGUI()` 中通过某种全局方式获取当前活跃 Scene。

- **优点**：不需要手动同步
- **缺点**：引入全局依赖；与项目现有模式不一致

---

## 8. 序列化支持

### 8.1 EnvironmentSettings 序列化

**文件位置**：`Lucky/Source/Lucky/Serialization/SceneSerializer.cpp`

#### 8.1.1 序列化

在场景序列化中，`EnvironmentSettings` 作为顶层节点（与 `Entities` 平级）：

```yaml
Scene: Untitled
Environment:
  AmbientSource: 0
  AmbientColor: [0.1, 0.1, 0.1]
  DiffuseIntensity: 1.0
  SpecularIntensity: 1.0
  ReflectionResolution: 128
Entities:
  - Entity: 12345
    ...
```

**序列化代码**：

```cpp
// 在 SerializeScene() 中，Entities 之前添加：
{
    const EnvironmentSettings& env = scene->GetEnvironmentSettings();
    out << YAML::Key << "Environment" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "AmbientSource" << YAML::Value << static_cast<int>(env.Source);
    out << YAML::Key << "AmbientColor" << YAML::Value << env.AmbientColor;
    out << YAML::Key << "DiffuseIntensity" << YAML::Value << env.DiffuseIntensity;
    out << YAML::Key << "SpecularIntensity" << YAML::Value << env.SpecularIntensity;
    out << YAML::Key << "ReflectionResolution" << YAML::Value << env.ReflectionResolution;
    out << YAML::EndMap;
}
```

#### 8.1.2 反序列化

```cpp
// 在 DeserializeScene() 中，Entities 之前添加：
auto envNode = data["Environment"];
if (envNode)
{
    EnvironmentSettings& env = scene->GetEnvironmentSettings();
    
    if (envNode["AmbientSource"])
    {
        env.Source = static_cast<AmbientSource>(envNode["AmbientSource"].as<int>());
    }
    if (envNode["AmbientColor"])
    {
        env.AmbientColor = envNode["AmbientColor"].as<glm::vec3>();
    }
    if (envNode["DiffuseIntensity"])
    {
        env.DiffuseIntensity = envNode["DiffuseIntensity"].as<float>();
    }
    if (envNode["SpecularIntensity"])
    {
        env.SpecularIntensity = envNode["SpecularIntensity"].as<float>();
    }
    if (envNode["ReflectionResolution"])
    {
        env.ReflectionResolution = envNode["ReflectionResolution"].as<int>();
    }
}
```

#### 方案 A：作为 Scene 顶层节点序列化【? 推荐 - 最优】

如上所示，`Environment` 作为与 `Entities` 平级的顶层节点。

- **优点**：
  - 结构清晰，环境设置是场景级别的全局数据
  - 反序列化时直接读取，无需遍历实体
  - 与 Unity 的场景文件结构类似
- **缺点**：
  - 需要在 SceneSerializer 中添加新的序列化/反序列化逻辑

#### 方案 B：作为特殊 Entity 的 Component 序列化【? 不推荐】

创建一个 "Environment" 实体，将 EnvironmentSettings 作为组件挂载。

- **优点**：复用现有的 Component 序列化机制
- **缺点**：
  - 环境设置不是实体概念，强行用 ECS 表示不自然
  - 用户可能误删该实体
  - 增加不必要的复杂度

---

## 9. 实现方式对比

### 9.1 数据存储位置

| 方案 | 位置 | 优点 | 缺点 | 推荐度 |
|------|------|------|------|--------|
| **A** | Scene 成员 | 生命周期一致；序列化方便；与 PostProcess 模式一致 | Scene 增加成员 | ? 最优 |
| **B** | EnvironmentComponent | 符合 ECS 模式 | 全局数据不应依附实体；可能被误删 | ? 不推荐 |
| **C** | Renderer3D 静态数据 | 不修改 Scene | 序列化困难；场景切换不重置 | ? 其次 |

### 9.2 LightingPanel 数据访问方式

| 方案 | 方式 | 优点 | 缺点 | 推荐度 |
|------|------|------|------|--------|
| **A** | 持有 Scene* 引用 | 直接修改；与 Inspector 模式一致 | 依赖 Scene 类 | ? 最优 |
| **B** | 持有副本 + 回调 | 不依赖 Scene | 同步复杂；回调管理 | ? 其次 |
| **C** | 通过 Renderer3D 静态接口 | 不依赖 Scene | 渲染层不应存储编辑器状态 | ? 不推荐 |

### 9.3 AMBIENT_STRENGTH 处理方式

| 方案 | 方式 | 优点 | 缺点 | 推荐度 |
|------|------|------|------|--------|
| **A** | 直接移除，用 u_AmbientColor 替代 | 消除硬编码；统一控制 | 需确保所有 Shader 迁移 | ? 最优 |
| **B** | 保留作为 fallback | 向后兼容 | 两套参数共存，混淆 | ? 其次 |

### 9.4 序列化方式

| 方案 | 方式 | 优点 | 缺点 | 推荐度 |
|------|------|------|------|--------|
| **A** | Scene 顶层节点 | 结构清晰；读写简单 | 新增序列化逻辑 | ? 最优 |
| **B** | 特殊 Entity Component | 复用现有机制 | 不自然；可能被误删 | ? 不推荐 |

### 9.5 IBL 预计算分辨率可配置方式

| 方案 | 方式 | 优点 | 缺点 | 推荐度 |
|------|------|------|------|--------|
| **A** | GenerateFromCubemap 参数传递 | 接口简洁；调用方控制 | 变更需重新生成 | ? 最优 |
| **B** | IBLPrecompute 内部配置 | 配置与生成分离 | 增加状态管理 | ? 其次 |

### 9.6 环境光 Shader 分支方式

**核心问题**：在 Shader 中如何根据 `u_AmbientSource` 选择不同的环境光计算方式？

#### 方案 A：if-else 分支【? 推荐 - 最优】

```glsl
if (u_AmbientSource == 0 && u_IBLEnabled != 0)
{
    ambient = CalcIBLAmbient(...);
}
else
{
    ambient = u_AmbientColor * albedo * ao;
}
```

- **优点**：简单直观；GPU 分支预测对 uniform 条件效率高（所有片元走同一分支）
- **缺点**：理论上有分支开销（但 uniform 分支几乎无开销）

#### 方案 B：统一计算，用乘法系数控制【? 其次】

```glsl
float iblWeight = float(u_AmbientSource == 0 && u_IBLEnabled != 0);
float colorWeight = 1.0 - iblWeight;
ambient = CalcIBLAmbient(...) * iblWeight + u_AmbientColor * albedo * ao * colorWeight;
```

- **优点**：无分支
- **缺点**：
  - IBL 关闭时仍然执行 `CalcIBLAmbient()`（纹理采样开销）
  - 代码可读性差

---

## 10. 涉及文件清单

### 10.1 需要修改的文件

| 文件 | 修改内容 |
|------|---------|
| `Lucky/Source/Lucky/Renderer/RenderContext.h` | 新增 `AmbientSource` 枚举、`EnvironmentSettings` 结构体、RenderContext 扩展字段 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.h` | 新增 `SetEnvironmentSettings()` 接口声明 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | 新增 `EnvironmentSettings` 缓存、`SetEnvironmentSettings()` 实现、RenderContext 构建扩展 |
| `Lucky/Source/Lucky/Renderer/Passes/OpaquePass.cpp` | 新增环境设置 uniform 设置 |
| `Lucky/Source/Lucky/Renderer/Passes/TransparentPass.cpp` | 新增环境设置 uniform 设置（与 OpaquePass 相同） |
| `Lucky/Source/Lucky/Renderer/Material.cpp` | 新增环境设置 uniform 到黑名单 |
| `Lucky/Source/Lucky/Scene/Scene.h` | 新增 `m_EnvironmentSettings` 成员和访问方法 |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | 新增 `Renderer3D::SetEnvironmentSettings()` 调用 |
| `Luck3DApp/Source/Panels/LightingPanel.h` | 重构：移除 `m_SkyboxMaterial`，新增 `Scene*` 成员 |
| `Luck3DApp/Source/Panels/LightingPanel.cpp` | 重构：实现 Environment Lighting / Reflections UI |
| `Luck3DApp/Assets/Shaders/Lucky/Common.glsl` | 移除 `AMBIENT_STRENGTH` 常量 |
| `Luck3DApp/Assets/Shaders/Lucky/Lighting.glsl` | 新增环境设置 uniform、修改 `CalcIBLAmbient()` |
| `Luck3DApp/Assets/Shaders/Standard.frag` | 修改环境光计算逻辑 |

### 10.2 可能需要修改的文件

| 文件 | 修改内容 | 条件 |
|------|---------|------|
| `Lucky/Source/Lucky/Serialization/SceneSerializer.cpp` | 新增 EnvironmentSettings 序列化/反序列化 | 需要持久化 |
| `Lucky/Source/Lucky/Renderer/IBLPrecompute.h` | `GenerateFromCubemap` 签名扩展 | 需要可配置分辨率 |
| `Lucky/Source/Lucky/Renderer/IBLPrecompute.cpp` | 使用传入的分辨率参数 | 需要可配置分辨率 |
| `Luck3DApp/Source/EditorLayer.cpp` | LightingPanel 构造方式修改 | LightingPanel 重构 |

### 10.3 需要添加的 include

| 文件 | 新增 include |
|------|-------------|
| `LightingPanel.cpp` | `#include "Lucky/Scene/Scene.h"` |
| `Scene.h` | `#include "Lucky/Renderer/RenderContext.h"`（如果 EnvironmentSettings 定义在此） |

---

## 11. 实现步骤

### 11.1 推荐实现顺序

| 步骤 | 内容 | 涉及文件 | 说明 |
|------|------|---------|------|
| **Step 1** | 定义 `AmbientSource` 枚举和 `EnvironmentSettings` 结构体 | `RenderContext.h` | 数据模型基础 |
| **Step 2** | 扩展 `RenderContext` 添加环境设置字段 | `RenderContext.h` | 数据传递通道 |
| **Step 3** | Shader 层修改：新增 uniform、修改 `CalcIBLAmbient()`、修改 `Standard.frag` | `Lighting.glsl`、`Standard.frag`、`Common.glsl` | 渲染效果核心 |
| **Step 4** | `Renderer3D` 新增接口和 RenderContext 构建 | `Renderer3D.h`、`Renderer3D.cpp` | C++ 数据流 |
| **Step 5** | `OpaquePass` / `TransparentPass` 设置环境 uniform | `OpaquePass.cpp`、`TransparentPass.cpp` | Pass 层集成 |
| **Step 6** | `Material` 黑名单更新 | `Material.cpp` | 防止 uniform 在材质面板显示 |
| **Step 7** | `Scene` 添加 `EnvironmentSettings` 成员和传递逻辑 | `Scene.h`、`Scene.cpp` | 数据源 |
| **Step 8** | `LightingPanel` 重构 | `LightingPanel.h`、`LightingPanel.cpp`、`EditorLayer.cpp` | 编辑器 UI |
| **Step 9** | 序列化支持 | `SceneSerializer.cpp` | 持久化 |
| **Step 10** | IBL 预计算分辨率可配置（可选） | `IBLPrecompute.h`、`IBLPrecompute.cpp` | 高级功能 |

### 11.2 最小可验证版本

如果需要快速验证 IBL 过亮修复效果，可以先实现 **Step 1 ~ Step 6**（不含编辑器 UI 和序列化），通过代码硬编码默认参数值来测试：

```cpp
// 在 Renderer3D::Init() 中临时设置
s_Data.Environment.DiffuseIntensity = 0.5f;     // 降低漫反射 IBL 强度
s_Data.Environment.SpecularIntensity = 1.0f;     // 保持镜面反射强度
```

---

## 12. 验证方法

### 12.1 IBL 强度控制验证

1. 设置 `DiffuseIntensity = 1.0`，`SpecularIntensity = 1.0`：效果应与修改前一致
2. 设置 `DiffuseIntensity = 0.0`，`SpecularIntensity = 0.0`：效果应与 IBL 关闭时一致（仅直接光照）
3. 逐步降低 `DiffuseIntensity`（如 0.5、0.3），观察物体亮度是否平滑降低
4. 逐步降低 `SpecularIntensity`（如 0.5、0.3），观察金属球反射是否平滑减弱

### 12.2 环境光来源切换验证

1. `Source = Skybox`：环境光来自 IBL，效果与当前一致
2. `Source = Color`，`AmbientColor = (0.1, 0.1, 0.1)`：效果应与 IBL 关闭时一致
3. `Source = Color`，`AmbientColor = (0.0, 0.0, 1.0)`：物体暗面应呈蓝色
4. `Source = Color`，`AmbientColor = (0.0, 0.0, 0.0)`：物体暗面应完全黑色

### 12.3 编辑器 UI 验证

1. LightingPanel 中 Environment Lighting 区域正确显示
2. Source 下拉框切换时，下方参数动态显示/隐藏
3. 拖动 Diffuse Intensity / Specular Intensity 滑条，场景实时响应
4. 修改 Ambient Color 颜色选择器，场景实时响应

### 12.4 序列化验证

1. 修改环境设置参数后保存场景
2. 重新加载场景，验证参数恢复正确
3. 新建场景，验证使用默认参数值

### 12.5 向后兼容验证

1. 加载旧场景文件（无 `Environment` 节点），验证使用默认参数值，渲染效果不变
2. 所有默认参数值应使渲染效果与修改前一致（`DiffuseIntensity = 1.0`、`SpecularIntensity = 1.0`、`Source = Skybox`）

---

## 附录 A：UI 效果示意

### LightingPanel Environment 区域

```
┌──────────────────────────────────────────────────────────────┐
│ ▼ Environment                                                │
│   Skybox Material    [Default-Skybox (Material)        ] [⊙] │
├──────────────────────────────────────────────────────────────┤
│ ▼ Environment Lighting                                       │
│   Source             [Skybox ▼]                               │
│   Diffuse Intensity  [========●==] 1.0                       │
├──────────────────────────────────────────────────────────────┤
│ ▼ Environment Reflections                                    │
│   Intensity Multiplier [========●==] 1.0                     │
│   Resolution           [128 ▼]                               │
└──────────────────────────────────────────────────────────────┘
```

### Source = Color 时

```
┌──────────────────────────────────────────────────────────────┐
│ ▼ Environment Lighting                                       │
│   Source             [Color ▼]                                │
│   Ambient Color      [■■■■■■■■■■■■■■■■■■] (0.1, 0.1, 0.1)  │
└──────────────────────────────────────────────────────────────┘
```

---

## 附录 B：完整 Shader 修改 Diff

### Lighting.glsl 新增 uniform

```glsl
// ---- 在现有 IBL uniform 之后添加 ----

// ---- 环境设置 uniform ----
uniform int u_AmbientSource;            // 环境光来源（0 = Skybox, 1 = Color）
uniform vec3 u_AmbientColor;            // 纯色环境光颜色（Source=Color 时使用）
uniform float u_IBLDiffuseIntensity;    // IBL 漫反射强度乘数（默认 1.0）
uniform float u_IBLSpecularIntensity;   // IBL 镜面反射强度乘数（默认 1.0）
```

### CalcIBLAmbient 修改（仅变化行）

```glsl
// 修改前：
vec3 diffuseIBL = kD * irradiance * albedo;
vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

// 修改后：
vec3 diffuseIBL = kD * irradiance * albedo * u_IBLDiffuseIntensity;
vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y) * u_IBLSpecularIntensity;
```

### Standard.frag 环境光计算修改

```glsl
// 修改前：
vec3 ambient;
if (u_IBLEnabled != 0)
{
    ambient = CalcIBLAmbient(N, V, albedo, metallic, roughness, F0, ao);
}
else
{
    ambient = vec3(AMBIENT_STRENGTH) * albedo * ao;
}

// 修改后：
vec3 ambient;
if (u_AmbientSource == 0 && u_IBLEnabled != 0)
{
    ambient = CalcIBLAmbient(N, V, albedo, metallic, roughness, F0, ao);
}
else if (u_AmbientSource == 1)
{
    ambient = u_AmbientColor * albedo * ao;
}
else
{
    ambient = u_AmbientColor * albedo * ao;
}
```

---

## 附录 C：OpaquePass / TransparentPass 新增 uniform 设置代码

```cpp
// 在现有 IBL uniform 设置之后添加：

// 设置环境设置 uniform
cmd.MaterialData->GetShader()->SetInt("u_AmbientSource", static_cast<int>(context.EnvironmentSource));
cmd.MaterialData->GetShader()->SetFloat3("u_AmbientColor", context.AmbientColor);
cmd.MaterialData->GetShader()->SetFloat("u_IBLDiffuseIntensity", context.IBLDiffuseIntensity);
cmd.MaterialData->GetShader()->SetFloat("u_IBLSpecularIntensity", context.IBLSpecularIntensity);
```

> **注意**：OpaquePass 和 TransparentPass 中的代码完全相同，需要在两个文件中都添加。
