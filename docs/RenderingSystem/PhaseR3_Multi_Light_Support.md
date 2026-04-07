# Phase R3：多光源支持

> **文档版本**：v1.0  
> **创建日期**：2026-04-07  
> **优先级**：?? P1  
> **预计工作量**：3-4 天  
> **前置依赖**：Phase R2（PBR Shader）  
> **文档说明**：本文档详细描述如何将当前的单方向光系统扩展为支持多光源（方向光 + 点光源 + 聚光灯），包括新增光源组件、扩展 Light UBO、修改 PBR Shader 的光照循环。所有代码可直接对照实现。

---

## 目录

- [一、现状分析](#一现状分析)
- [二、改进目标](#二改进目标)
- [三、涉及的文件清单](#三涉及的文件清单)
- [四、方案选择：多光源实现方式](#四方案选择多光源实现方式)
- [五、光源类型设计](#五光源类型设计)
  - [5.1 方向光（已有）](#51-方向光已有)
  - [5.2 点光源（新增）](#52-点光源新增)
  - [5.3 聚光灯（新增）](#53-聚光灯新增)
- [六、Light UBO 重新设计](#六light-ubo-重新设计)
  - [6.1 UBO 数据结构（C++ 侧）](#61-ubo-数据结构c-侧)
  - [6.2 UBO 数据结构（GLSL 侧）](#62-ubo-数据结构glsl-侧)
  - [6.3 std140 内存布局分析](#63-std140-内存布局分析)
- [七、新增组件定义](#七新增组件定义)
  - [7.1 PointLightComponent](#71-pointlightcomponent)
  - [7.2 SpotLightComponent](#72-spotlightcomponent)
- [八、Scene.cpp 修改](#八scenecpp-修改)
  - [8.1 光源收集逻辑](#81-光源收集逻辑)
- [九、Renderer3D 修改](#九renderer3d-修改)
  - [9.1 Renderer3D.h 修改](#91-renderer3dh-修改)
  - [9.2 Renderer3D.cpp 修改](#92-renderer3dcpp-修改)
- [十、Standard.frag 多光源循环](#十standardfrag-多光源循环)
  - [10.1 光照计算函数](#101-光照计算函数)
  - [10.2 完整 main 函数修改](#102-完整-main-函数修改)
- [十一、编辑器集成](#十一编辑器集成)
  - [11.1 SceneHierarchyPanel 创建菜单](#111-scenehierarchypanel-创建菜单)
  - [11.2 InspectorPanel 组件绘制](#112-inspectorpanel-组件绘制)
- [十二、组件注册](#十二组件注册)
- [十三、序列化支持](#十三序列化支持)
- [十四、衰减模型](#十四衰减模型)
- [十五、验证方法](#十五验证方法)
- [十六、设计决策记录](#十六设计决策记录)

---

## 一、现状分析

### 当前光照系统

```cpp
// Renderer3D.h - 当前只支持单个方向光
struct DirectionalLightData
{
    glm::vec3 Direction = glm::vec3(-0.8f, -1.0f, -0.5f);
    glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);
    float Intensity = 1.0f;
};
```

```cpp
// Renderer3D.cpp - Light UBO 只有一个方向光
struct LightData
{
    float Intensity;
    char padding3[12];
    glm::vec3 Direction;
    char padding1[4];
    glm::vec3 Color;
    char padding2[4];
};
```

```cpp
// Scene.cpp - 只查询第一个方向光
auto lightView = m_Registry.view<TransformComponent, DirectionalLightComponent>();
for (auto entity : lightView)
{
    // ...
    break;  // 目前仅支持一个方向光
}
```

### 问题

| 编号 | 问题 | 影响 |
|------|------|------|
| R3-01 | 仅支持单个方向光 | 无法创建多光源场景 |
| R3-02 | 无点光源支持 | 无法模拟灯泡、火把等局部光源 |
| R3-03 | 无聚光灯支持 | 无法模拟手电筒、舞台灯等 |
| R3-04 | Light UBO 结构固定 | 无法扩展为多光源 |

---

## 二、改进目标

1. **支持多个方向光**：最多 4 个
2. **支持点光源**：最多 8 个，带距离衰减
3. **支持聚光灯**：最多 4 个，带锥形衰减
4. **新增 `PointLightComponent` 和 `SpotLightComponent`**
5. **重新设计 Light UBO**：支持多光源数组
6. **修改 PBR Shader**：循环计算每个光源的贡献

---

## 三、涉及的文件清单

| 文件路径 | 操作 | 说明 |
|---------|------|------|
| `Lucky/Source/Lucky/Scene/Components/PointLightComponent.h` | **新建** | 点光源组件 |
| `Lucky/Source/Lucky/Scene/Components/SpotLightComponent.h` | **新建** | 聚光灯组件 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.h` | 修改 | 新增多光源数据结构，修改 BeginScene 接口 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | 修改 | 重新设计 Light UBO，实现多光源数据上传 |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | 修改 | 收集所有光源数据 |
| `Luck3DApp/Assets/Shaders/Standard.frag` | 修改 | 多光源循环 |
| `Luck3DApp/Source/Panels/SceneHierarchyPanel.cpp` | 修改 | 添加创建菜单 |
| `Luck3DApp/Source/Panels/InspectorPanel.cpp` | 修改 | 添加组件 Inspector UI |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | 修改 | 注册新组件 |
| `Lucky/Source/Lucky/Scene/SceneSerializer.cpp` | 修改 | 序列化新组件 |

---

## 四、方案选择：多光源实现方式

| 方案 | 说明 | 优点 | 缺点 | 推荐 |
|------|------|------|------|------|
| **方案 A：UBO 数组** | 在 UBO 中定义固定大小的光源数组 | 简单，兼容性好（OpenGL 3.3+） | 光源数量有上限 | ? **推荐** |
| 方案 B：SSBO | 使用 Shader Storage Buffer Object | 光源数量无上限 | 需要 OpenGL 4.3+，复杂度高 | |
| 方案 C：多 Pass | 每个光源一个 Pass（Unity Forward 模式） | 与 Unity 一致 | 需要多 Pass 框架（Phase R7），DrawCall 翻倍 | |

**推荐方案 A（UBO 数组）**：

理由：
1. 当前引擎使用 OpenGL 4.5（`#version 450 core`），UBO 完全支持
2. 16 个光源的上限对大多数场景足够
3. 实现最简单，与当前架构兼容
4. UBO 大小限制：OpenGL 保证至少 16KB，我们的 Light UBO 约 1KB，远在限制内

---

## 五、光源类型设计

### 5.1 方向光（已有）

| 属性 | 类型 | 说明 |
|------|------|------|
| Direction | vec3 | 由 TransformComponent 的 forward 向量推导 |
| Color | vec3 | 光照颜色 |
| Intensity | float | 光照强度 |

### 5.2 点光源（新增）

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| Position | vec3 | 由 TransformComponent 位置推导 | 光源位置 |
| Color | vec3 | (1,1,1) | 光照颜色 |
| Intensity | float | 1.0 | 光照强度 |
| Range | float | 10.0 | 光照范围（超出此范围衰减为 0） |

### 5.3 聚光灯（新增）

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| Position | vec3 | 由 TransformComponent 位置推导 | 光源位置 |
| Direction | vec3 | 由 TransformComponent 的 forward 向量推导 | 光照方向 |
| Color | vec3 | (1,1,1) | 光照颜色 |
| Intensity | float | 1.0 | 光照强度 |
| Range | float | 10.0 | 光照范围 |
| InnerCutoff | float | 12.5° | 内锥角（全亮区域） |
| OuterCutoff | float | 17.5° | 外锥角（衰减到 0 的边界） |

---

## 六、Light UBO 重新设计

### 6.1 UBO 数据结构（C++ 侧）

```cpp
// Renderer3D.cpp 中的新 LightData 结构

#define MAX_DIRECTIONAL_LIGHTS 4
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4

struct DirectionalLightGPU
{
    glm::vec3 Direction;    // 12 bytes
    float Intensity;        // 4 bytes
    glm::vec3 Color;        // 12 bytes
    float _padding;         // 4 bytes（对齐到 32 bytes）
};
// sizeof = 32 bytes

struct PointLightGPU
{
    glm::vec3 Position;     // 12 bytes
    float Intensity;        // 4 bytes
    glm::vec3 Color;        // 12 bytes
    float Range;            // 4 bytes
};
// sizeof = 32 bytes

struct SpotLightGPU
{
    glm::vec3 Position;     // 12 bytes
    float Intensity;        // 4 bytes
    glm::vec3 Direction;    // 12 bytes
    float Range;            // 4 bytes
    glm::vec3 Color;        // 12 bytes
    float InnerCutoff;      // 4 bytes（cos 值）
    float OuterCutoff;      // 4 bytes（cos 值）
    float _padding[3];      // 12 bytes（对齐到 64 bytes）
};
// sizeof = 64 bytes

struct LightUBOData
{
    int DirectionalLightCount;  // 4 bytes
    int PointLightCount;        // 4 bytes
    int SpotLightCount;         // 4 bytes
    int _padding;               // 4 bytes
    DirectionalLightGPU DirectionalLights[MAX_DIRECTIONAL_LIGHTS];  // 4 × 32 = 128 bytes
    PointLightGPU PointLights[MAX_POINT_LIGHTS];                    // 8 × 32 = 256 bytes
    SpotLightGPU SpotLights[MAX_SPOT_LIGHTS];                       // 4 × 64 = 256 bytes
};
// 总计 = 16 + 128 + 256 + 256 = 656 bytes
```

### 6.2 UBO 数据结构（GLSL 侧）

```glsl
#define MAX_DIRECTIONAL_LIGHTS 4
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4

struct DirectionalLight
{
    vec3 Direction;
    float Intensity;
    vec3 Color;
    float _padding;
};

struct PointLight
{
    vec3 Position;
    float Intensity;
    vec3 Color;
    float Range;
};

struct SpotLight
{
    vec3 Position;
    float Intensity;
    vec3 Direction;
    float Range;
    vec3 Color;
    float InnerCutoff;  // cos(innerAngle)
    float OuterCutoff;  // cos(outerAngle)
    float _padding[3];
};

layout(std140, binding = 1) uniform Lights
{
    int DirectionalLightCount;
    int PointLightCount;
    int SpotLightCount;
    int _padding;
    DirectionalLight DirectionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLight PointLights[MAX_POINT_LIGHTS];
    SpotLight SpotLights[MAX_SPOT_LIGHTS];
} u_Lights;
```

### 6.3 std140 内存布局分析

std140 布局规则：
- `int/float`：4 bytes 对齐
- `vec3`：16 bytes 对齐（**注意：vec3 在 std140 中占 16 bytes！**）
- `struct`：最大成员对齐的倍数

> **关键注意**：在 std140 布局中，`vec3` 实际占用 16 bytes（与 `vec4` 相同）。因此上述结构体中 `vec3` 后面的 `float` 会自然填充到 `vec3` 的 padding 位置。

验证 `DirectionalLightGPU` 的 std140 布局：
```
offset 0:  vec3 Direction  (16 bytes, 但只用 12)
offset 12: float Intensity (4 bytes, 填充到 Direction 的 padding)
offset 16: vec3 Color      (16 bytes, 但只用 12)
offset 28: float _padding  (4 bytes, 填充到 Color 的 padding)
总计 = 32 bytes ?
```

---

## 七、新增组件定义

### 7.1 PointLightComponent

```cpp
// Lucky/Source/Lucky/Scene/Components/PointLightComponent.h
#pragma once

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 点光源组件：表示一个向所有方向发射光线的点光源
    /// 光源位置由实体的 TransformComponent 位置决定
    /// </summary>
    struct PointLightComponent
    {
        glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);    // 光照颜色
        float Intensity = 1.0f;                              // 光照强度
        float Range = 10.0f;                                 // 光照范围

        PointLightComponent() = default;
        PointLightComponent(const PointLightComponent& other) = default;
        PointLightComponent(const glm::vec3& color, float intensity, float range)
            : Color(color), Intensity(intensity), Range(range) {}
    };
}
```

### 7.2 SpotLightComponent

```cpp
// Lucky/Source/Lucky/Scene/Components/SpotLightComponent.h
#pragma once

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 聚光灯组件：表示一个锥形光源
    /// 光源位置由 TransformComponent 位置决定
    /// 光照方向由 TransformComponent 的 forward 向量推导
    /// </summary>
    struct SpotLightComponent
    {
        glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);    // 光照颜色
        float Intensity = 1.0f;                              // 光照强度
        float Range = 10.0f;                                 // 光照范围
        float InnerCutoffAngle = 12.5f;                      // 内锥角（度）
        float OuterCutoffAngle = 17.5f;                      // 外锥角（度）

        SpotLightComponent() = default;
        SpotLightComponent(const SpotLightComponent& other) = default;
    };
}
```

---

## 八、Scene.cpp 修改

### 8.1 光源收集逻辑

```cpp
void Scene::OnUpdate(DeltaTime dt, EditorCamera& camera)
{
    // 收集所有光源数据
    SceneLightData sceneLightData;
    
    // 收集方向光
    {
        auto lightView = m_Registry.view<TransformComponent, DirectionalLightComponent>();
        for (auto entity : lightView)
        {
            if (sceneLightData.DirectionalLightCount >= MAX_DIRECTIONAL_LIGHTS)
                break;
            
            auto [transform, light] = lightView.get<TransformComponent, DirectionalLightComponent>(entity);
            
            auto& dl = sceneLightData.DirectionalLights[sceneLightData.DirectionalLightCount];
            dl.Direction = transform.GetForward();
            dl.Color = light.Color;
            dl.Intensity = light.Intensity;
            sceneLightData.DirectionalLightCount++;
        }
    }
    
    // 收集点光源
    {
        auto lightView = m_Registry.view<TransformComponent, PointLightComponent>();
        for (auto entity : lightView)
        {
            if (sceneLightData.PointLightCount >= MAX_POINT_LIGHTS)
                break;
            
            auto [transform, light] = lightView.get<TransformComponent, PointLightComponent>(entity);
            
            auto& pl = sceneLightData.PointLights[sceneLightData.PointLightCount];
            pl.Position = transform.GetTranslation();
            pl.Color = light.Color;
            pl.Intensity = light.Intensity;
            pl.Range = light.Range;
            sceneLightData.PointLightCount++;
        }
    }
    
    // 收集聚光灯
    {
        auto lightView = m_Registry.view<TransformComponent, SpotLightComponent>();
        for (auto entity : lightView)
        {
            if (sceneLightData.SpotLightCount >= MAX_SPOT_LIGHTS)
                break;
            
            auto [transform, light] = lightView.get<TransformComponent, SpotLightComponent>(entity);
            
            auto& sl = sceneLightData.SpotLights[sceneLightData.SpotLightCount];
            sl.Position = transform.GetTranslation();
            sl.Direction = transform.GetForward();
            sl.Color = light.Color;
            sl.Intensity = light.Intensity;
            sl.Range = light.Range;
            sl.InnerCutoff = glm::cos(glm::radians(light.InnerCutoffAngle));
            sl.OuterCutoff = glm::cos(glm::radians(light.OuterCutoffAngle));
            sceneLightData.SpotLightCount++;
        }
    }
    
    Renderer3D::BeginScene(camera, sceneLightData);
    {
        // ... 渲染逻辑不变 ...
    }
    Renderer3D::EndScene();
}
```

---

## 九、Renderer3D 修改

### 9.1 Renderer3D.h 修改

```cpp
#pragma once

#include "Camera.h"
#include "EditorCamera.h"
#include "Texture.h"
#include "Mesh.h"
#include "Material.h"

namespace Lucky
{
    #define MAX_DIRECTIONAL_LIGHTS 4
    #define MAX_POINT_LIGHTS 8
    #define MAX_SPOT_LIGHTS 4
    
    /// <summary>
    /// 方向光数据（GPU 格式）
    /// </summary>
    struct DirectionalLightGPU
    {
        glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f);
        float Intensity = 1.0f;
        glm::vec3 Color = glm::vec3(1.0f);
        float _padding = 0.0f;
    };
    
    /// <summary>
    /// 点光源数据（GPU 格式）
    /// </summary>
    struct PointLightGPU
    {
        glm::vec3 Position = glm::vec3(0.0f);
        float Intensity = 1.0f;
        glm::vec3 Color = glm::vec3(1.0f);
        float Range = 10.0f;
    };
    
    /// <summary>
    /// 聚光灯数据（GPU 格式）
    /// </summary>
    struct SpotLightGPU
    {
        glm::vec3 Position = glm::vec3(0.0f);
        float Intensity = 1.0f;
        glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f);
        float Range = 10.0f;
        glm::vec3 Color = glm::vec3(1.0f);
        float InnerCutoff = 0.9763f;   // cos(12.5°)
        float OuterCutoff = 0.9537f;   // cos(17.5°)
        float _padding[3] = {0};
    };
    
    /// <summary>
    /// 场景光照数据：从 Scene 传递给 Renderer3D
    /// </summary>
    struct SceneLightData
    {
        int DirectionalLightCount = 0;
        DirectionalLightGPU DirectionalLights[MAX_DIRECTIONAL_LIGHTS];
        
        int PointLightCount = 0;
        PointLightGPU PointLights[MAX_POINT_LIGHTS];
        
        int SpotLightCount = 0;
        SpotLightGPU SpotLights[MAX_SPOT_LIGHTS];
    };
    
    class Renderer3D
    {
    public:
        static void Init();
        static void Shutdown();

        /// <summary>
        /// 开始渲染场景（多光源版本）
        /// </summary>
        static void BeginScene(const EditorCamera& camera, const SceneLightData& lightData);
        
        // ... 其余接口不变 ...
    };
}
```

### 9.2 Renderer3D.cpp 修改

```cpp
// 新的 LightUBOData 结构（与 GLSL std140 布局一致）
struct LightUBOData
{
    int DirectionalLightCount;
    int PointLightCount;
    int SpotLightCount;
    int _padding;
    DirectionalLightGPU DirectionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLightGPU PointLights[MAX_POINT_LIGHTS];
    SpotLightGPU SpotLights[MAX_SPOT_LIGHTS];
};

// Init 中更新 UBO 大小
void Renderer3D::Init()
{
    // ...
    s_Data.LightUniformBuffer = UniformBuffer::Create(sizeof(LightUBOData), 1);
}

// BeginScene 中上传多光源数据
void Renderer3D::BeginScene(const EditorCamera& camera, const SceneLightData& lightData)
{
    // Camera UBO 不变
    s_Data.CameraBuffer.ViewProjectionMatrix = camera.GetViewProjectionMatrix();
    s_Data.CameraBuffer.Position = camera.GetPosition();
    s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));
    
    // Light UBO - 多光源
    LightUBOData lightUBO = {};
    lightUBO.DirectionalLightCount = lightData.DirectionalLightCount;
    lightUBO.PointLightCount = lightData.PointLightCount;
    lightUBO.SpotLightCount = lightData.SpotLightCount;
    
    for (int i = 0; i < lightData.DirectionalLightCount; ++i)
        lightUBO.DirectionalLights[i] = lightData.DirectionalLights[i];
    
    for (int i = 0; i < lightData.PointLightCount; ++i)
        lightUBO.PointLights[i] = lightData.PointLights[i];
    
    for (int i = 0; i < lightData.SpotLightCount; ++i)
        lightUBO.SpotLights[i] = lightData.SpotLights[i];
    
    s_Data.LightUniformBuffer->SetData(&lightUBO, sizeof(LightUBOData));
}
```

---

## 十、Standard.frag 多光源循环

### 10.1 光照计算函数

```glsl
// 计算方向光的 PBR 贡献
vec3 CalcDirectionalLight(DirectionalLight light, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3 L = normalize(-light.Direction);
    vec3 H = normalize(V + L);
    
    vec3 radiance = light.Color * light.Intensity;
    
    float D = DistributionGGX(N, H, roughness);
    vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    float G = GeometrySmith(N, V, L, roughness);
    
    vec3 numerator = D * F * G;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);
    
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// 计算点光源的 PBR 贡献
vec3 CalcPointLight(PointLight light, vec3 N, vec3 V, vec3 worldPos, vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3 L = light.Position - worldPos;
    float distance = length(L);
    
    // 超出范围则不计算
    if (distance > light.Range)
        return vec3(0.0);
    
    L = normalize(L);
    vec3 H = normalize(V + L);
    
    // 距离衰减（平滑衰减到 Range 边界）
    float attenuation = clamp(1.0 - (distance / light.Range), 0.0, 1.0);
    attenuation *= attenuation;  // 二次衰减，更自然
    
    vec3 radiance = light.Color * light.Intensity * attenuation;
    
    float D = DistributionGGX(N, H, roughness);
    vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    float G = GeometrySmith(N, V, L, roughness);
    
    vec3 numerator = D * F * G;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);
    
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// 计算聚光灯的 PBR 贡献
vec3 CalcSpotLight(SpotLight light, vec3 N, vec3 V, vec3 worldPos, vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3 L = light.Position - worldPos;
    float distance = length(L);
    
    if (distance > light.Range)
        return vec3(0.0);
    
    L = normalize(L);
    vec3 H = normalize(V + L);
    
    // 锥形衰减
    float theta = dot(L, normalize(-light.Direction));
    float epsilon = light.InnerCutoff - light.OuterCutoff;
    float spotIntensity = clamp((theta - light.OuterCutoff) / epsilon, 0.0, 1.0);
    
    // 距离衰减
    float attenuation = clamp(1.0 - (distance / light.Range), 0.0, 1.0);
    attenuation *= attenuation;
    
    vec3 radiance = light.Color * light.Intensity * attenuation * spotIntensity;
    
    float D = DistributionGGX(N, H, roughness);
    vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    float G = GeometrySmith(N, V, L, roughness);
    
    vec3 numerator = D * F * G;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);
    
    return (kD * albedo / PI + specular) * radiance * NdotL;
}
```

### 10.2 完整 main 函数修改

```glsl
void main()
{
    // ---- 采样材质参数（与 Phase R2 相同） ----
    // ... albedo, metallic, roughness, ao, emission ...
    
    vec3 N = GetNormal();
    vec3 V = normalize(u_Camera.Position - v_Input.WorldPos);
    
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // ---- 多光源循环 ----
    vec3 Lo = vec3(0.0);
    
    // 方向光
    for (int i = 0; i < u_Lights.DirectionalLightCount; ++i)
    {
        Lo += CalcDirectionalLight(u_Lights.DirectionalLights[i], N, V, albedo, metallic, roughness, F0);
    }
    
    // 点光源
    for (int i = 0; i < u_Lights.PointLightCount; ++i)
    {
        Lo += CalcPointLight(u_Lights.PointLights[i], N, V, v_Input.WorldPos, albedo, metallic, roughness, F0);
    }
    
    // 聚光灯
    for (int i = 0; i < u_Lights.SpotLightCount; ++i)
    {
        Lo += CalcSpotLight(u_Lights.SpotLights[i], N, V, v_Input.WorldPos, albedo, metallic, roughness, F0);
    }
    
    // ---- 环境光 + 自发光 ----
    vec3 ambient = vec3(AMBIENT_STRENGTH) * albedo * ao;
    vec3 color = ambient + Lo + emission;
    
    // ---- Gamma 校正 ----
    color = pow(color, vec3(1.0 / 2.2));
    
    o_Color = vec4(color, alpha);
}
```

---

## 十一、编辑器集成

### 11.1 SceneHierarchyPanel 创建菜单

在 `DrawEntityCreateMenu` 中添加：

```cpp
// 创建 Point Light
if (ImGui::MenuItem("Point Light"))
{
    std::string uniqueName = GenerateUniqueName("Point Light", parent);
    newEntity = m_Scene->CreateEntity(uniqueName);
    newEntity.AddComponent<PointLightComponent>();
}

// 创建 Spot Light
if (ImGui::MenuItem("Spot Light"))
{
    std::string uniqueName = GenerateUniqueName("Spot Light", parent);
    newEntity = m_Scene->CreateEntity(uniqueName);
    newEntity.AddComponent<SpotLightComponent>();
}
```

### 11.2 InspectorPanel 组件绘制

```cpp
// PointLightComponent Inspector
DrawComponent<PointLightComponent>("Point Light", entity, [](auto& component)
{
    ImGui::ColorEdit3("Color", glm::value_ptr(component.Color));
    ImGui::DragFloat("Intensity", &component.Intensity, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Range", &component.Range, 0.1f, 0.1f, 1000.0f);
});

// SpotLightComponent Inspector
DrawComponent<SpotLightComponent>("Spot Light", entity, [](auto& component)
{
    ImGui::ColorEdit3("Color", glm::value_ptr(component.Color));
    ImGui::DragFloat("Intensity", &component.Intensity, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Range", &component.Range, 0.1f, 0.1f, 1000.0f);
    ImGui::DragFloat("Inner Cutoff", &component.InnerCutoffAngle, 0.5f, 0.0f, component.OuterCutoffAngle);
    ImGui::DragFloat("Outer Cutoff", &component.OuterCutoffAngle, 0.5f, component.InnerCutoffAngle, 90.0f);
});
```

---

## 十二、组件注册

在 `Scene.cpp` 的组件注册处添加：

```cpp
m_Registry.on_construct<PointLightComponent>().connect<&Scene::OnComponentAdded<PointLightComponent>>(*this);
m_Registry.on_construct<SpotLightComponent>().connect<&Scene::OnComponentAdded<SpotLightComponent>>(*this);
```

---

## 十三、序列化支持

在 `SceneSerializer.cpp` 中添加：

```cpp
// 序列化
if (entity.HasComponent<PointLightComponent>())
{
    auto& light = entity.GetComponent<PointLightComponent>();
    out << YAML::Key << "PointLightComponent";
    out << YAML::BeginMap;
    out << YAML::Key << "Color" << YAML::Value << light.Color;
    out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
    out << YAML::Key << "Range" << YAML::Value << light.Range;
    out << YAML::EndMap;
}

if (entity.HasComponent<SpotLightComponent>())
{
    auto& light = entity.GetComponent<SpotLightComponent>();
    out << YAML::Key << "SpotLightComponent";
    out << YAML::BeginMap;
    out << YAML::Key << "Color" << YAML::Value << light.Color;
    out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
    out << YAML::Key << "Range" << YAML::Value << light.Range;
    out << YAML::Key << "InnerCutoffAngle" << YAML::Value << light.InnerCutoffAngle;
    out << YAML::Key << "OuterCutoffAngle" << YAML::Value << light.OuterCutoffAngle;
    out << YAML::EndMap;
}

// 反序列化
auto pointLightComponent = entity["PointLightComponent"];
if (pointLightComponent)
{
    auto& light = deserializedEntity.AddComponent<PointLightComponent>();
    light.Color = pointLightComponent["Color"].as<glm::vec3>();
    light.Intensity = pointLightComponent["Intensity"].as<float>();
    light.Range = pointLightComponent["Range"].as<float>();
}

auto spotLightComponent = entity["SpotLightComponent"];
if (spotLightComponent)
{
    auto& light = deserializedEntity.AddComponent<SpotLightComponent>();
    light.Color = spotLightComponent["Color"].as<glm::vec3>();
    light.Intensity = spotLightComponent["Intensity"].as<float>();
    light.Range = spotLightComponent["Range"].as<float>();
    light.InnerCutoffAngle = spotLightComponent["InnerCutoffAngle"].as<float>();
    light.OuterCutoffAngle = spotLightComponent["OuterCutoffAngle"].as<float>();
}
```

---

## 十四、衰减模型

### 方案选择

| 方案 | 公式 | 优点 | 缺点 | 推荐 |
|------|------|------|------|------|
| **方案 A：平滑衰减（推荐）** | `att = clamp(1 - d/R, 0, 1)2` | 简单，在 Range 边界平滑衰减到 0 | 非物理精确 | ? |
| 方案 B：物理衰减 | `att = 1 / (d2 + 1)` | 物理精确 | 永远不会衰减到 0，需要截断 | |
| 方案 C：UE4 衰减 | `att = clamp(1 - (d/R)?, 0, 1)2 / (d2 + 1)` | 物理精确 + 有限范围 | 计算复杂 | |

**推荐方案 A**：简单平滑衰减。在 Range 边界自然衰减到 0，无需额外截断。

---

## 十五、验证方法

### 15.1 多方向光验证

1. 创建 2 个方向光实体，设置不同颜色（红色和蓝色）
2. 确认物体同时受两个方向光影响
3. 确认颜色混合正确

### 15.2 点光源验证

1. 创建一个点光源，放置在物体附近
2. 确认物体面向光源的一侧被照亮
3. 移动点光源，确认光照随距离衰减
4. 确认超出 Range 后完全无光照

### 15.3 聚光灯验证

1. 创建一个聚光灯，对准物体
2. 确认只有锥形区域内被照亮
3. 确认内锥角和外锥角之间有平滑过渡
4. 旋转聚光灯，确认光照方向跟随

### 15.4 性能验证

1. 创建 4 个方向光 + 8 个点光源 + 4 个聚光灯（满载）
2. 确认帧率可接受
3. 确认超出上限的光源被正确忽略

---

## 十六、设计决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 多光源方案 | UBO 数组 | 简单可靠，兼容性好，满足大多数场景 |
| 最大方向光数 | 4 | 大多数场景 1-2 个方向光足够 |
| 最大点光源数 | 8 | 室内场景常见的灯光数量 |
| 最大聚光灯数 | 4 | 聚光灯使用频率较低 |
| 衰减模型 | 平滑衰减 `(1-d/R)2` | 简单，Range 边界自然衰减到 0 |
| 聚光灯角度存储 | 组件中存角度，GPU 中存 cos 值 | 角度对用户直观，cos 值对 Shader 高效 |
| 光源位置/方向 | 由 TransformComponent 推导 | 与 Unity 一致，避免数据冗余 |
