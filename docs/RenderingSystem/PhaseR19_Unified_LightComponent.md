# Phase R19：统一光源组件（Unified LightComponent）

## 目录

- [一、概述](#一概述)
  - [1.1 当前问题](#11-当前问题)
  - [1.2 设计目标](#12-设计目标)
  - [1.3 前置依赖](#13-前置依赖)
- [二、主流引擎参考](#二主流引擎参考)
  - [2.1 Unity](#21-unity)
  - [2.2 Unreal Engine](#22-unreal-engine)
  - [2.3 Godot](#23-godot)
- [三、方案总览](#三方案总览)
- [四、方案 A：统一 LightComponent（推荐）](#四方案-a统一-lightcomponent推荐)
  - [4.1 数据结构设计](#41-数据结构设计)
  - [4.2 Scene.cpp 光源收集逻辑](#42-scenecpp-光源收集逻辑)
  - [4.3 SceneSerializer 序列化](#43-sceneserializer-序列化)
  - [4.4 InspectorPanel UI](#44-inspectorpanel-ui)
  - [4.5 SceneHierarchyPanel 创建菜单](#45-scenehierarchypanel-创建菜单)
  - [4.6 GizmoRenderer 调用处](#46-gizmorenderer-调用处)
  - [4.7 Scene.cpp OnComponentAdded](#47-scenecpp-oncomponentadded)
  - [4.8 Renderer3D.h 相关修改](#48-renderer3dh-相关修改)
  - [4.9 优缺点总结](#49-优缺点总结)
- [五、方案 B：保持分离 + 共享阴影 Mixin（折中方案）](#五方案-b保持分离--共享阴影-mixin折中方案)
  - [5.1 设计思路](#51-设计思路)
  - [5.2 代码示例](#52-代码示例)
  - [5.3 优缺点总结](#53-优缺点总结)
- [六、方案对比与推荐](#六方案对比与推荐)
- [七、涉及的文件清单](#七涉及的文件清单)
- [八、分步实施策略](#八分步实施策略)
- [九、验证清单](#九验证清单)

---

## 一、概述

### 1.1 当前问题

当前项目使用 **三个独立的 struct** 表示三种光源类型：

| 组件 | 文件 | 属性 |
|------|------|------|
| `DirectionalLightComponent` | `DirectionalLightComponent.h` | Color, Intensity, Shadows, ShadowBias, ShadowStrength |
| `PointLightComponent` | `PointLightComponent.h` | Color, Intensity, Range |
| `SpotLightComponent` | `SpotLightComponent.h` | Color, Intensity, Range, InnerCutoffAngle, OuterCutoffAngle |

这导致以下问题：

1. **代码重复**：序列化、UI 绘制、光源收集逻辑各写三遍
2. **不能运行时切换类型**：切换光源类型需要删除旧组件 + 添加新组件
3. **阴影属性分散**：当前阴影属性只在 `DirectionalLightComponent` 中，后续 Point/Spot 也需要阴影时需要分别添加
4. **扩展麻烦**：新增光源类型（如 Area Light）需要新建文件、修改多处代码
5. **ECS 查询分散**：收集光源需要三次 `registry.view<>()` 调用

### 1.2 设计目标

1. 将三种光源组件整合为一个统一的 `LightComponent`
2. 使用 `LightType` 枚举区分光源类型
3. Inspector 中可以直接切换光源类型（无需删除重建）
4. 所有光源类型共享阴影属性（为后续 Point/Spot 阴影做准备）
5. 序列化支持
6. 与现有代码规范一致

### 1.3 前置依赖

| 依赖 | 状态 | 说明 |
|------|------|------|
| Phase R4 阴影系统 | 已完成 | DirectionalLightComponent 中的阴影属性 |
| Phase R16 Shader 架构重构 | 已完成 | #include 预处理器、函数库 |
| entt ECS 框架 | 已集成 | 组件注册和查询 |

---

## 二、主流引擎参考

### 2.1 Unity

Unity 使用**单一 `Light` 组件** + `LightType` 枚举：

```csharp
public enum LightType
{
    Spot = 0,
    Directional = 1,
    Point = 2,
    Area = 3,       // Baked only
    Disc = 4        // Baked only
}

public class Light : Component
{
    public LightType type;
    public Color color;
    public float intensity;
    public float range;             // Point/Spot
    public float spotAngle;         // Spot
    public float innerSpotAngle;    // Spot
    public LightShadows shadows;    // None, Hard, Soft
    public float shadowBias;
    public float shadowNormalBias;
    public float shadowStrength;
    public int shadowResolution;
}
```

**关键设计**：
- 所有光源类型共享一个组件
- Inspector 中根据 `type` 动态显示/隐藏相关属性
- 切换类型只需修改 `type` 字段，无需删除重建
- 阴影属性对所有类型统一（但运行时根据类型决定是否生效）

### 2.2 Unreal Engine

Unreal 使用**继承体系**：

```
ULightComponent (基类)
├── UDirectionalLightComponent
├── UPointLightComponent
│   └── USpotLightComponent (继承自 PointLight)
└── URectLightComponent
```

**关键设计**：
- 基类 `ULightComponent` 包含共有属性（Color、Intensity、Shadow 设置）
- 子类添加特有属性（Range、ConeAngle 等）
- 这是传统 OOP 继承，**不是 ECS 模式**

### 2.3 Godot

Godot 使用**继承体系**（类似 Unreal）：

```
Light3D (基类)
├── DirectionalLight3D
├── OmniLight3D (点光源)
└── SpotLight3D
```

**关键设计**：
- 基类 `Light3D` 包含 color、energy、shadow 等共有属性
- 子类添加特有属性
- Godot 不是纯 ECS，所以继承没有问题

> **注意**：Unreal 和 Godot 的继承方案不适用于本项目，因为 entt ECS 框架不支持组件继承的多态查询（`view<LightBase>()` 不会返回子类组件）。详见方案对比章节。

---

## 三、方案总览

| 方案 | 描述 | 适合的架构 | 推荐度 |
|------|------|-----------|--------|
| **A：统一 LightComponent** | 一个 struct + LightType 枚举 | ECS（entt） | ??? **最优** |
| **B：保持分离 + 共享 Mixin** | 三个独立组件 + 共享阴影结构体 | ECS（entt） | ?? 其次 |

---

## 四、方案 A：统一 LightComponent（推荐）

### 4.1 数据结构设计

```cpp
// Lucky/Source/Lucky/Scene/Components/LightComponent.h
#pragma once

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 光源类型
    /// </summary>
    enum class LightType : uint8_t
    {
        Directional = 0,    // 方向光（平行光）
        Point,              // 点光源
        Spot                // 聚光灯
    };

    /// <summary>
    /// 阴影类型
    /// </summary>
    enum class ShadowType : uint8_t
    {
        None = 0,       // 不投射阴影
        Hard,           // 硬阴影（无 PCF）
        Soft            // 软阴影（PCF 3×3）
    };

    /// <summary>
    /// 统一光源组件：表示场景中的一个光源
    /// 通过 Type 字段区分光源类型，不同类型使用不同的属性子集
    /// </summary>
    struct LightComponent
    {
        // ======== 通用属性（所有光源类型共享） ========
        LightType Type = LightType::Directional;            // 光源类型
        glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);    // 光照颜色
        float Intensity = 1.0f;                             // 光照强度

        // ======== Point / Spot 属性 ========
        float Range = 10.0f;                                // 光照范围（Point/Spot 使用）

        // ======== Spot 属性 ========
        float InnerCutoffAngle = 12.5f;                     // 内锥角（度）（Spot 使用）
        float OuterCutoffAngle = 17.5f;                     // 外锥角（度）（Spot 使用）

        // ======== 阴影属性（所有光源类型共享） ========
        ShadowType Shadows = ShadowType::None;              // 阴影类型
        float ShadowBias = 0.0003f;                         // 阴影偏移
        float ShadowStrength = 1.0f;                        // 阴影强度 [0, 1]

        // ======== 构造函数 ========
        LightComponent() = default;
        LightComponent(const LightComponent& other) = default;

        /// <summary>
        /// 创建指定类型的光源组件
        /// </summary>
        /// <param name="type">光源类型</param>
        LightComponent(LightType type)
            : Type(type)
        {
            // 根据类型设置合理的默认值
            switch (type)
            {
                case LightType::Directional:
                    Shadows = ShadowType::Hard;     // 方向光默认开启硬阴影
                    break;
                case LightType::Point:
                    Shadows = ShadowType::None;
                    break;
                case LightType::Spot:
                    Shadows = ShadowType::None;
                    break;
            }
        }
    };
}
```

**设计说明**：

| 属性 | Directional | Point | Spot | 说明 |
|------|:-----------:|:-----:|:----:|------|
| Type | ? | ? | ? | 必须 |
| Color | ? | ? | ? | 通用 |
| Intensity | ? | ? | ? | 通用 |
| Range | ? | ? | ? | Directional 不使用（无限远） |
| InnerCutoffAngle | ? | ? | ? | 仅 Spot |
| OuterCutoffAngle | ? | ? | ? | 仅 Spot |
| Shadows | ? | ? | ? | 通用（但当前仅 Directional 实际生效） |
| ShadowBias | ? | ? | ? | 通用 |
| ShadowStrength | ? | ? | ? | 通用 |

> **内存开销分析**：整合后每个 `LightComponent` 约占 **48 字节**。即使场景中有 16 个光源，总内存也仅 768 字节，完全可以忽略。

---

### 4.2 Scene.cpp 光源收集逻辑

```cpp
// Lucky/Source/Lucky/Scene/Scene.cpp

void Scene::OnUpdate(DeltaTime dt, EditorCamera& camera)
{
    // ---- 收集所有光源数据（统一查询，替代原来的三次 view） ----
    SceneLightData sceneLightData;

    {
        auto lightView = m_Registry.view<TransformComponent, LightComponent>();
        for (auto entity : lightView)
        {
            auto [transform, light] = lightView.get<TransformComponent, LightComponent>(entity);

            switch (light.Type)
            {
                case LightType::Directional:
                {
                    if (sceneLightData.DirectionalLightCount >= s_MaxDirectionalLights)
                    {
                        break;
                    }

                    DirectionalLightData& dirLight = sceneLightData.DirectionalLights[sceneLightData.DirectionalLightCount];
                    dirLight.Direction = transform.GetForward();
                    dirLight.Color = light.Color;
                    dirLight.Intensity = light.Intensity;

                    // 收集第一个方向光的阴影参数
                    if (sceneLightData.DirectionalLightCount == 0)
                    {
                        sceneLightData.DirLightShadowType = light.Shadows;
                        sceneLightData.DirLightShadowBias = light.ShadowBias;
                        sceneLightData.DirLightShadowStrength = light.ShadowStrength;
                    }

                    sceneLightData.DirectionalLightCount++;
                    break;
                }
                case LightType::Point:
                {
                    if (sceneLightData.PointLightCount >= s_MaxPointLights)
                    {
                        break;
                    }

                    PointLightData& pointLight = sceneLightData.PointLights[sceneLightData.PointLightCount];
                    pointLight.Position = transform.Translation;
                    pointLight.Color = light.Color;
                    pointLight.Intensity = light.Intensity;
                    pointLight.Range = light.Range;

                    sceneLightData.PointLightCount++;
                    break;
                }
                case LightType::Spot:
                {
                    if (sceneLightData.SpotLightCount >= s_MaxSpotLights)
                    {
                        break;
                    }

                    SpotLightData& spotLight = sceneLightData.SpotLights[sceneLightData.SpotLightCount];
                    spotLight.Position = transform.Translation;
                    spotLight.Direction = transform.GetForward();
                    spotLight.Color = light.Color;
                    spotLight.Intensity = light.Intensity;
                    spotLight.Range = light.Range;
                    spotLight.InnerCutoff = glm::cos(glm::radians(light.InnerCutoffAngle));
                    spotLight.OuterCutoff = glm::cos(glm::radians(light.OuterCutoffAngle));

                    sceneLightData.SpotLightCount++;
                    break;
                }
            }
        }
    }

    // ... 后续渲染逻辑不变 ...
}
```

**对比原来的三段代码**：从三次 `view` + 三段循环简化为一次 `view` + switch，结构更清晰，且只需一次 ECS 查询。

---

### 4.3 SceneSerializer 序列化

```cpp
// ======== 序列化 ========
if (entity.HasComponent<LightComponent>())
{
    const auto& light = entity.GetComponent<LightComponent>();

    out << YAML::Key << "LightComponent";
    out << YAML::BeginMap;

    out << YAML::Key << "Type" << YAML::Value << static_cast<int>(light.Type);
    out << YAML::Key << "Color" << YAML::Value << light.Color;
    out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;

    // Point / Spot 属性
    if (light.Type == LightType::Point || light.Type == LightType::Spot)
    {
        out << YAML::Key << "Range" << YAML::Value << light.Range;
    }

    // Spot 属性
    if (light.Type == LightType::Spot)
    {
        out << YAML::Key << "InnerCutoffAngle" << YAML::Value << light.InnerCutoffAngle;
        out << YAML::Key << "OuterCutoffAngle" << YAML::Value << light.OuterCutoffAngle;
    }

    // 阴影属性（所有类型）
    out << YAML::Key << "Shadows" << YAML::Value << static_cast<int>(light.Shadows);
    out << YAML::Key << "ShadowBias" << YAML::Value << light.ShadowBias;
    out << YAML::Key << "ShadowStrength" << YAML::Value << light.ShadowStrength;

    out << YAML::EndMap;
}
```

```cpp
// ======== 反序列化 ========
YAML::Node lightComponentNode = entity["LightComponent"];
if (lightComponentNode)
{
    LightType type = static_cast<LightType>(lightComponentNode["Type"].as<int>());
    auto& light = deserializedEntity.AddComponent<LightComponent>(type);

    light.Color = lightComponentNode["Color"].as<glm::vec3>();
    light.Intensity = lightComponentNode["Intensity"].as<float>();

    if (lightComponentNode["Range"])
    {
        light.Range = lightComponentNode["Range"].as<float>();
    }
    if (lightComponentNode["InnerCutoffAngle"])
    {
        light.InnerCutoffAngle = lightComponentNode["InnerCutoffAngle"].as<float>();
    }
    if (lightComponentNode["OuterCutoffAngle"])
    {
        light.OuterCutoffAngle = lightComponentNode["OuterCutoffAngle"].as<float>();
    }

    if (lightComponentNode["Shadows"])
    {
        light.Shadows = static_cast<ShadowType>(lightComponentNode["Shadows"].as<int>());
    }
    if (lightComponentNode["ShadowBias"])
    {
        light.ShadowBias = lightComponentNode["ShadowBias"].as<float>();
    }
    if (lightComponentNode["ShadowStrength"])
    {
        light.ShadowStrength = lightComponentNode["ShadowStrength"].as<float>();
    }
}
```

**YAML 输出示例**：

```yaml
# 方向光
LightComponent:
  Type: 0
  Color: [1.0, 1.0, 1.0]
  Intensity: 1.0
  Shadows: 1
  ShadowBias: 0.0003
  ShadowStrength: 1.0

# 点光源
LightComponent:
  Type: 1
  Color: [1.0, 0.8, 0.6]
  Intensity: 2.0
  Range: 15.0
  Shadows: 0
  ShadowBias: 0.0003
  ShadowStrength: 1.0

# 聚光灯
LightComponent:
  Type: 2
  Color: [1.0, 1.0, 1.0]
  Intensity: 5.0
  Range: 20.0
  InnerCutoffAngle: 12.5
  OuterCutoffAngle: 17.5
  Shadows: 0
  ShadowBias: 0.0003
  ShadowStrength: 1.0
```

---

### 4.4 InspectorPanel UI

```cpp
// InspectorPanel.cpp

DrawComponent<LightComponent>("Light", entity, [](LightComponent& light)
{
    // 光源类型下拉框
    const char* lightTypes[] = { "Directional", "Point", "Spot" };
    int currentType = static_cast<int>(light.Type);
    if (ImGui::Combo("Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
    {
        LightType newType = static_cast<LightType>(currentType);

        // 切换类型时设置合理的默认值
        if (newType != light.Type)
        {
            light.Type = newType;

            // 根据新类型设置默认阴影模式
            if (newType == LightType::Directional)
            {
                light.Shadows = ShadowType::Hard;
            }
            else
            {
                light.Shadows = ShadowType::None;
            }
        }
    }

    ImGui::Separator();

    // 通用属性
    ImGui::ColorEdit3("Color", glm::value_ptr(light.Color));
    ImGui::DragFloat("Intensity", &light.Intensity, 0.01f, 0.0f, 100.0f);

    // Point / Spot 属性
    if (light.Type == LightType::Point || light.Type == LightType::Spot)
    {
        ImGui::DragFloat("Range", &light.Range, 0.1f, 0.1f, 1000.0f);
    }

    // Spot 属性
    if (light.Type == LightType::Spot)
    {
        ImGui::DragFloat("Inner Cutoff", &light.InnerCutoffAngle, 0.5f, 0.0f, light.OuterCutoffAngle);
        ImGui::DragFloat("Outer Cutoff", &light.OuterCutoffAngle, 0.5f, light.InnerCutoffAngle, 90.0f);
    }

    ImGui::Separator();

    // 阴影属性
    const char* shadowTypes[] = { "No Shadows", "Hard Shadows", "Soft Shadows" };
    int currentShadow = static_cast<int>(light.Shadows);
    if (ImGui::Combo("Shadow Type", &currentShadow, shadowTypes, IM_ARRAYSIZE(shadowTypes)))
    {
        light.Shadows = static_cast<ShadowType>(currentShadow);
    }

    if (light.Shadows != ShadowType::None)
    {
        ImGui::DragFloat("Shadow Bias", &light.ShadowBias, 0.0001f, 0.0f, 0.05f, "%.4f");
        ImGui::DragFloat("Shadow Strength", &light.ShadowStrength, 0.01f, 0.0f, 1.0f);
    }
});
```

**Inspector 布局预览**：

```
▼ Light
  Type:             [▼ Directional   ]
  ─────────────────────────────────────
  Color:            [■■■■■■■■] (1.0, 1.0, 1.0)
  Intensity:        [====●====] 1.0
  ─────────────────────────────────────
  Shadow Type:      [▼ Hard Shadows  ]
  Shadow Bias:      [====●====] 0.0003
  Shadow Strength:  [====●====] 1.0
```

---

### 4.5 SceneHierarchyPanel 创建菜单

```cpp
// SceneHierarchyPanel.cpp

if (ImGui::BeginMenu("Light"))
{
    // 创建 Directional Light
    if (ImGui::MenuItem("Directional Light"))
    {
        std::string uniqueName = GenerateUniqueName("Directional Light", parent);
        newEntity = m_Scene->CreateEntity(uniqueName);
        newEntity.AddComponent<LightComponent>(LightType::Directional);

        // 设置初始方向斜向下
        TransformComponent& transform = newEntity.GetComponent<TransformComponent>();
        transform.SetRotationEuler(glm::vec3(glm::radians(50.0f), glm::radians(-32.0f), 0.0f));
    }

    // 创建 Point Light
    if (ImGui::MenuItem("Point Light"))
    {
        std::string uniqueName = GenerateUniqueName("Point Light", parent);
        newEntity = m_Scene->CreateEntity(uniqueName);
        newEntity.AddComponent<LightComponent>(LightType::Point);
    }

    // 创建 Spot Light
    if (ImGui::MenuItem("Spot Light"))
    {
        std::string uniqueName = GenerateUniqueName("Spot Light", parent);
        newEntity = m_Scene->CreateEntity(uniqueName);
        newEntity.AddComponent<LightComponent>(LightType::Spot);
    }

    ImGui::EndMenu();
}
```

---

### 4.6 GizmoRenderer 调用处

```cpp
// 在绘制 Gizmo 的地方（通常在 EditorLayer 或 Scene 的 Gizmo 绘制逻辑中）

auto lightView = m_Registry.view<TransformComponent, LightComponent>();
for (auto entity : lightView)
{
    auto [transform, light] = lightView.get<TransformComponent, LightComponent>(entity);

    switch (light.Type)
    {
        case LightType::Directional:
        {
            GizmoRenderer::DrawDirectionalLightGizmo(
                transform.Translation, transform.GetForward(), light.Color);
            break;
        }
        case LightType::Point:
        {
            GizmoRenderer::DrawPointLightGizmo(
                transform.Translation, light.Range, light.Color);
            break;
        }
        case LightType::Spot:
        {
            GizmoRenderer::DrawSpotLightGizmo(
                transform.Translation, transform.GetForward(),
                light.Range, light.InnerCutoffAngle, light.OuterCutoffAngle, light.Color);
            break;
        }
    }
}
```

---

### 4.7 Scene.cpp OnComponentAdded

```cpp
// 替换原来的三个特化为一个
template<>
void Scene::OnComponentAdded<LightComponent>(Entity entity, LightComponent& component)
{
    // 当前无需特殊处理
}
```

---

### 4.8 Renderer3D.h 相关修改

`Renderer3D.h` 中的 `SceneLightData`、`DirectionalLightData`、`PointLightData`、`SpotLightData` **不需要修改**。这些是 GPU 端的数据结构，与组件层解耦。`LightComponent` 只是替代了 ECS 层的三个组件，数据收集后仍然填充到相同的 GPU 数据结构中。

唯一需要修改的是 `Renderer3D.h` 中的 `#include`：

```cpp
// 原来
#include "Lucky/Scene/Components/DirectionalLightComponent.h"

// 改为
#include "Lucky/Scene/Components/LightComponent.h"
```

> **注意**：`ShadowType` 枚举从 `DirectionalLightComponent.h` 移到了 `LightComponent.h` 中。由于 `Renderer3D.h` 和 `RenderContext.h` 都使用了 `ShadowType`，需要确保它们 include 了新的头文件。

---

### 4.9 优缺点总结

**优点**：

| # | 优点 | 说明 |
|---|------|------|
| 1 | **代码简洁** | 一个组件替代三个，序列化/UI/收集逻辑各只写一次 |
| 2 | **运行时切换类型** | Inspector 中直接切换光源类型，无需删除重建 |
| 3 | **统一阴影属性** | 所有光源类型共享阴影属性，后续为 Point/Spot 添加阴影时无需改组件结构 |
| 4 | **与 Unity 一致** | 用户熟悉的工作流 |
| 5 | **扩展性好** | 新增光源类型只需加一个枚举值 + 几个属性 |
| 6 | **ECS 查询简化** | 只需一次 `view<LightComponent>` 即可获取所有光源 |

**缺点**：

| # | 缺点 | 说明 |
|---|------|------|
| 1 | **内存浪费** | Directional 不需要 Range/CutoffAngle，Point 不需要 CutoffAngle（但总共仅 ~20 字节浪费） |
| 2 | **重构工作量** | 需要修改 ~10 个文件 |
| 3 | **"无效字段"** | 某些字段对特定类型无意义（如 Directional 的 Range），需要 UI 和逻辑中判断 |

---

## 五、方案 B：保持分离 + 共享阴影 Mixin（折中方案）

### 5.1 设计思路

保持三个独立组件，但将共有的阴影属性提取为一个嵌套结构体（Mixin），避免重复定义。

### 5.2 代码示例

```cpp
// Lucky/Source/Lucky/Scene/Components/ShadowSettings.h
#pragma once

namespace Lucky
{
    enum class ShadowType : uint8_t
    {
        None = 0,
        Hard,
        Soft
    };

    /// <summary>
    /// 阴影设置：所有光源类型共享的阴影参数
    /// </summary>
    struct ShadowSettings
    {
        ShadowType Shadows = ShadowType::None;
        float ShadowBias = 0.0003f;
        float ShadowStrength = 1.0f;
    };
}
```

```cpp
// DirectionalLightComponent.h
#pragma once

#include "ShadowSettings.h"

#include <glm/glm.hpp>

namespace Lucky
{
    struct DirectionalLightComponent
    {
        glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);    // 光照颜色
        float Intensity = 1.0f;                             // 光照强度

        ShadowSettings Shadow;                              // 阴影设置

        DirectionalLightComponent() = default;
        DirectionalLightComponent(const DirectionalLightComponent& other) = default;
        DirectionalLightComponent(const glm::vec3& color, float intensity)
            : Color(color), Intensity(intensity)
        {
            Shadow.Shadows = ShadowType::Hard;              // 方向光默认开启阴影
        }
    };
}
```

```cpp
// PointLightComponent.h
#pragma once

#include "ShadowSettings.h"

#include <glm/glm.hpp>

namespace Lucky
{
    struct PointLightComponent
    {
        glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);    // 光照颜色
        float Intensity = 1.0f;                             // 光照强度
        float Range = 10.0f;                                // 光照范围

        ShadowSettings Shadow;                              // 阴影设置（当前不使用，为后续预留）

        PointLightComponent() = default;
        PointLightComponent(const PointLightComponent& other) = default;
        PointLightComponent(const glm::vec3& color, float intensity, float range)
            : Color(color), Intensity(intensity), Range(range) {}
    };
}
```

```cpp
// SpotLightComponent.h
#pragma once

#include "ShadowSettings.h"

#include <glm/glm.hpp>

namespace Lucky
{
    struct SpotLightComponent
    {
        glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);    // 光照颜色
        float Intensity = 1.0f;                             // 光照强度
        float Range = 10.0f;                                // 光照范围
        float InnerCutoffAngle = 12.5f;                     // 内锥角（度）
        float OuterCutoffAngle = 17.5f;                     // 外锥角（度）

        ShadowSettings Shadow;                              // 阴影设置（当前不使用，为后续预留）

        SpotLightComponent() = default;
        SpotLightComponent(const SpotLightComponent& other) = default;
    };
}
```

### 5.3 优缺点总结

**优点**：

| # | 优点 | 说明 |
|---|------|------|
| 1 | **改动最小** | 只需添加一个 `ShadowSettings` 结构体，现有组件结构基本不变 |
| 2 | **类型安全** | 编译期区分光源类型 |
| 3 | **阴影属性统一** | 所有光源共享相同的阴影参数结构 |
| 4 | **零内存浪费** | 每个组件只包含该类型需要的属性 |

**缺点**：

| # | 缺点 | 说明 |
|---|------|------|
| 1 | **仍然三次查询** | 收集光源数据仍需三次 `view<>()` |
| 2 | **仍然三段代码** | 序列化、UI、收集逻辑仍然各写三遍 |
| 3 | **不能运行时切换类型** | 仍需删除旧组件 + 添加新组件 |
| 4 | **Color/Intensity 仍重复** | 共有属性（Color、Intensity）仍在每个组件中重复定义 |
| 5 | **扩展仍麻烦** | 新增光源类型仍需新建文件、修改多处代码 |

---

## 六、方案对比与推荐

| 维度 | 方案 A：统一 LightComponent | 方案 B：分离 + Mixin |
|------|:-------------------------:|:-------------------:|
| **ECS 兼容性** | ? 完美 | ? 完美 |
| **代码简洁度** | ??? 最佳 | ?? 中等 |
| **运行时切换类型** | ? 支持 | ? 不支持 |
| **统一查询** | ? 一次 view | ? 三次 view |
| **阴影属性统一** | ? 天然统一 | ? Mixin 统一 |
| **内存效率** | ?? 略有浪费 | ??? 最优 |
| **重构工作量** | ?? 中等 | ??? 最小 |
| **扩展性** | ??? 最佳 | ? 差 |
| **与 Unity 一致** | ? 完全一致 | ? 无对应 |
| **推荐度** | ??? **最优** | ?? **其次** |

### 最终推荐

**??? 强烈推荐方案 A（统一 LightComponent）**

理由：
1. 这是 ECS 架构下的最佳实践
2. 与 Unity 的设计完全一致，用户体验最好
3. 代码最简洁，维护成本最低
4. 为后续扩展（Area Light、Point/Spot 阴影）提供了最好的基础
5. 运行时切换类型是一个重要的用户体验提升

**方案 B 作为备选**，如果你认为方案 A 的重构工作量过大，可以先用方案 B 作为过渡（只添加 ShadowSettings Mixin），后续再整合为方案 A。

---

## 七、涉及的文件清单

### 需要新建的文件

| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Scene/Components/LightComponent.h` | 统一光源组件定义 |

### 需要修改的文件

| 文件路径 | 修改内容 |
|---------|---------|
| `Lucky/Source/Lucky/Scene/Components/Components.h` | 替换三个 include 为一个 `LightComponent.h` |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | 合并三段光源收集代码为一段 switch |
| `Lucky/Source/Lucky/Serialization/SceneSerializer.cpp` | 合并三段序列化代码为一段 |
| `Luck3DApp/Source/Panels/InspectorPanel.cpp` | 合并三段 UI 代码为一段（根据 Type 动态显示） |
| `Luck3DApp/Source/Panels/SceneHierarchyPanel.cpp` | 创建菜单改为添加同一组件但设置不同 Type |
| `Lucky/Source/Lucky/Renderer/Renderer3D.h` | `#include` 路径更新（`DirectionalLightComponent.h` → `LightComponent.h`） |
| `Lucky/Source/Lucky/Renderer/RenderContext.h` | `#include` 路径更新（如果引用了 `ShadowType`） |
| `Lucky/Source/Lucky/Scene/Scene.h` | 如果有 `#include` 需要更新 |

### 需要删除的文件

| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Scene/Components/DirectionalLightComponent.h` | 被 `LightComponent.h` 替代 |
| `Lucky/Source/Lucky/Scene/Components/PointLightComponent.h` | 被 `LightComponent.h` 替代 |
| `Lucky/Source/Lucky/Scene/Components/SpotLightComponent.h` | 被 `LightComponent.h` 替代 |

### 不需要修改的文件

| 文件路径 | 原因 |
|---------|------|
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | `SceneLightData` 结构不变，只是数据来源从三个组件变为一个 |
| `Lucky/Source/Lucky/Renderer/Passes/ShadowPass.cpp` | 不依赖组件类型 |
| `Lucky/Source/Lucky/Renderer/Passes/OpaquePass.cpp` | 不依赖组件类型 |
| `Luck3DApp/Assets/Shaders/*` | Shader 不关心组件层 |
| `Lucky/Source/Lucky/Renderer/GizmoRenderer.cpp` | 只是被调用方，接口不变 |

### GizmoRenderer 调用处

需要找到当前绘制光源 Gizmo 的代码位置（可能在 `EditorLayer.cpp` 或 `Scene.cpp` 中），将三段分别查询 `DirectionalLightComponent`/`PointLightComponent`/`SpotLightComponent` 的代码合并为一段查询 `LightComponent` 的代码。

---

## 八、分步实施策略

| 步骤 | 内容 | 依赖 | 预估工作量 |
|------|------|------|-----------|
| **Step 1** | 创建 `LightComponent.h`（含 LightType 枚举、ShadowType 枚举、所有属性、带 LightType 参数的构造函数） | 无 | 小 |
| **Step 2** | 修改 `Components.h`（替换 include） | Step 1 | 极小 |
| **Step 3** | 修改 `Scene.cpp`（合并光源收集逻辑 + OnComponentAdded） | Step 1 | 中 |
| **Step 4** | 修改 `Renderer3D.h`（更新 include，确保 ShadowType 可用） | Step 1 | 极小 |
| **Step 5** | 修改 `SceneSerializer.cpp`（新格式序列化） | Step 1 | 中 |
| **Step 6** | 修改 `InspectorPanel.cpp`（统一 UI 绘制） | Step 1 | 中 |
| **Step 7** | 修改 `SceneHierarchyPanel.cpp`（创建菜单） | Step 1 | 小 |
| **Step 8** | 修改 Gizmo 绘制调用处（合并三段为一段） | Step 1 | 小 |
| **Step 9** | 删除旧的三个组件文件 | Step 2~8 全部完成 | 极小 |
| **Step 10** | 编译测试 + 修复编译错误 | Step 9 | 中 |

**推荐执行顺序**：Step 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10

> **关键里程碑**：
> - Step 4 完成后：代码可以编译（但旧组件仍存在，两套并存）
> - Step 9 完成后：旧组件删除，只有新的 LightComponent
> - Step 10 完成后：全部功能验证通过

---

## 九、验证清单

| # | 验证项 | 预期结果 |
|---|--------|---------|
| 1 | 编译通过 | 无编译错误 |
| 2 | 创建 Directional Light | 正确创建，Inspector 显示 Type=Directional |
| 3 | 创建 Point Light | 正确创建，Inspector 显示 Type=Point |
| 4 | 创建 Spot Light | 正确创建，Inspector 显示 Type=Spot |
| 5 | Inspector 切换类型 | Directional → Point → Spot 切换正常，属性动态显示/隐藏 |
| 6 | 光照效果正确 | 三种光源的光照效果与重构前一致 |
| 7 | 阴影效果正确 | 方向光阴影与重构前一致 |
| 8 | Gizmo 绘制正确 | 三种光源的 Gizmo 与重构前一致 |
| 9 | 保存场景 | 新格式 YAML 输出正确 |
| 10 | 加载场景 | 新格式正确反序列化 |
| 11 | 删除光源 | 正确移除 LightComponent |
| 12 | 多光源场景 | 多个不同类型光源共存正常 |
