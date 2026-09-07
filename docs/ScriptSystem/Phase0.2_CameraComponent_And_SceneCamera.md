# Phase 0.2：CameraComponent + SceneCamera

## 1. 概述

P0.2 目标：给 Luck3D 引入"**场景内的游戏相机**"这一概念，为 Play 模式和 Game 面板提供视角来源。

具体产出：

1. 新增 `SceneCamera` 类：只封装投影矩阵与投影参数，不带控制逻辑
2. 新增 `CameraComponent` 组件：ECS 中承载 `SceneCamera` + `Primary / FixedAspectRatio` 标记
3. 打通 `Scene::OnRenderRuntime()`：从 P0.1 的空壳变成"用主相机渲染场景"
4. 打通 `Scene::OnViewportResize()`：视口大小变化时同步刷新非固定宽高比的相机
5. `Renderer3D::BeginScene` 新增矩阵版重载，兼容 `EditorCamera` 与 `SceneCamera` 两条路径
6. Inspector / Serializer / AddComponent 菜单 / 默认场景 全部接入

### 前置依赖
- P0.1 完成（`Scene` 已有 `OnRenderEditor / OnRenderRuntime` 接口，`m_State` 状态机就位）
- **PhaseR32 完成**（`Camera` 基类已拆包含 `ProjectionType` 枚举 + 透视/正交投影参数 + 全套 Getter/Setter；`EditorCamera` 已瘦身为纯视图/输入控制器）

> 本 Phase 直接复用 `Camera` 基类的投影能力，`SceneCamera` 类本身变得极薄（空派生类），主体工作在于 ECS 接入、主相机查找、Runtime 渲染路径打通。

### 本 Phase **不做**的事
- 不做 `GameViewportPanel`（P0.6）?? P0.2 完成后 `OnRenderRuntime` **能被调用且能出画面**，但暂时没有外部调用方（可以在 Scene 面板中临时切换以便验证）
- 不做 Play/Stop 工具条（P0.5）
- 不做游戏相机的输入响应（游戏相机由脚本控制，脚本能力是 Phase 1 才有的）
- 不做 CSM 在正交投影下的适配（P0.2 只保证透视投影下与现有一致；正交投影下 CSM 自动关闭）

---

## 2. 涉及的文件

### 需要新建
| 文件 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Renderer/SceneCamera.h` | `SceneCamera` 声明：空派生类（`class SceneCamera : public Camera {};`） |
| `Lucky/Source/Lucky/Scene/Components/CameraComponent.h` | `CameraComponent` 结构体 |

### 需要修改
| 文件 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Scene/Components/ComponentType.h` | 枚举新增 `Camera` |
| `Lucky/Source/Lucky/Scene/Components/Components.h` | 汇总 include `CameraComponent.h` + `ComponentTrait<CameraComponent>` 特化 |
| `Lucky/Source/Lucky/Scene/Scene.h` | 新增 `GetPrimaryCameraEntity()`；`OnComponentAdded<CameraComponent>` 特化前置声明；`RenderSceneImpl` 签名调整（决策点 3 中讨论） |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | `OnRenderRuntime` 补齐；`OnViewportResize` 联动；`OnComponentAdded<CameraComponent>` 空实现；`RenderSceneImpl` 迁移 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.h` | `BeginScene` 新增矩阵版重载声明；`CameraRenderData` 结构体声明；include `Camera.h`（获得 `ProjectionType`） |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | `BeginScene(EditorCamera&, ...)` 改为薄转发；矩阵版内部实现（CSM 计算用新字段） |
| `Lucky/Source/Lucky/Serialization/SceneSerializer.cpp` | `CameraComponent` 序列化 / 反序列化分支 |
| `Luck3DApp/Source/Panels/InspectorPanel.cpp` | 新增 `DrawComponent<CameraComponent>` 绘制块 |
| `Luck3DApp/Source/Panels/InspectorPanel.h` / `.cpp` | AddComponent 弹出菜单加 "Camera" 项 |
| `Luck3DApp/Source/EditorLayer.cpp` | `EnsureDefaultScene` 补一个 `Main Camera` 实体 |
| `Luck3DApp/Source/Panels/SceneViewportPanel.cpp` | Framebuffer resize 时调用 `Scene::OnViewportResize` |

> `SceneCamera` 为空派生类，**无需新建 `SceneCamera.cpp`**（无非 inline 方法）。

### 无需新建但需要新增图标资源（可选，非阻塞）
| 路径 | 说明 |
|------|------|
| `Resources/Icons/Component/Camera.png` | 组件图标；不加也能跑（`GetComponentIcon` 返回 nullptr 时 UI 会走空图标兜底） |

---

## 3. 现状回顾

### 3.1 `Renderer3D::BeginScene` 从相机身上取用的字段

从 [Renderer3D.cpp](../../Lucky/Source/Lucky/Renderer/Renderer3D.cpp) `BeginScene(const EditorCamera&, ...)` 逐行梳理（PhaseR32 后已统一长命名）：

| 字段 | 用途 |
|------|------|
| `camera.GetViewProjectionMatrix()` | 写入 `CameraUBOData.ViewProjectionMatrix` |
| `camera.GetProjectionMatrix()` | 求逆得到 `InvProjectionMatrix` |
| `camera.GetPosition()` | 写入 `CameraUBOData.Position`（PBR 计算需要） |
| `camera.GetPerspectiveNearClip()` | CSM cascade near 计算起点 |
| `camera.GetPerspectiveVerticalFOV()` | CSM 子视锥体投影计算（度） |
| `camera.GetAspectRatio()` | CSM 子视锥体投影计算 |
| `camera.GetViewMatrix()` (`cameraView`) | CSM `invVP = inverse(subProj * view)` |

**观察**：一共 7 个字段，全部可以用 `CameraRenderData` 结构体承载。这意味着"矩阵版 `BeginScene`"是可行的、无信息丢失的方案。

### 3.2 `Scene::OnRenderRuntime` 当前状态

P0.1 之后 [Scene.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp) 里 `OnRenderRuntime` 是空实现。P0.2 要把它填成"查找 Primary Camera → 构造 `CameraRenderData` → 调用 `Renderer3D::BeginScene`（矩阵版）"。

### 3.3 `Scene::OnViewportResize` 当前状态

现在是空函数，且**从未被调用**（[SceneViewportPanel.cpp](../../Luck3DApp/Source/Panels/SceneViewportPanel.cpp) 在 Framebuffer 大小变化时并没有触发它）。P0.2 需要：
1. 让 `SceneViewportPanel` / 未来的 `GameViewportPanel` 在视口变化时**调用**这个方法
2. `Scene::OnViewportResize` 内部遍历所有 `CameraComponent`，对 `FixedAspectRatio=false` 的调用 `SceneCamera::SetViewportSize(w, h)`

### 3.4 `EditorLayer::EnsureDefaultScene` 里的坑

现有代码里有一行：

```cpp
// Main Camera：暂缓，等相机组件到位后再补
```

P0.2 完成后这行注释删除，替换为真实的 Main Camera 实体创建。

---

## 4. 详细设计

### 4.1 SceneCamera 类

PhaseR32 已把投影能力（Perspective / Orthographic + AspectRatio + 全套 Getter/Setter + `RecalculateProjection`）下移到 `Camera` 基类。本 Phase 的 `SceneCamera` 直接继承 `Camera`，**无新增字段与方法**：

**头文件**：`Lucky/Source/Lucky/Renderer/SceneCamera.h`

```cpp
#pragma once

#include "Camera.h"

namespace Lucky
{
    /// <summary>
    /// 场景相机：ECS 世界中作为 CameraComponent 的一部分
    /// 位置和朝向由同一实体上的 TransformComponent 提供；不响应输入（由脚本控制）
    /// 目前为空派生类，保留类型区分性以便未来添加 ECS 相机专有字段（如 ClearFlags / CullingMask / ViewportRect 等对齐 Unity 的概念）
    /// </summary>
    class SceneCamera : public Camera
    {
    };
}
```

**为什么保留空派生类而不直接用 `Camera`**：见【决策点 4】。

### 4.2 CameraComponent

**头文件**：`Lucky/Source/Lucky/Scene/Components/CameraComponent.h`

```cpp
#pragma once

#include "Lucky/Renderer/SceneCamera.h"

namespace Lucky
{
    /// <summary>
    /// 相机组件：使实体成为一个可渲染的视角
    /// 位置和朝向由同一实体上的 TransformComponent 提供
    /// </summary>
    struct CameraComponent
    {
        SceneCamera Camera;                 // 场景相机（投影参数）
        bool Primary = true;                // 是否为主相机（场景中最多一个 Primary=true）
        bool FixedAspectRatio = false;      // 是否固定宽高比（true 时 Scene::OnViewportResize 不改动此相机）

        CameraComponent() = default;
        CameraComponent(const CameraComponent& other) = default;
    };
}
```

### 4.3 ComponentType 枚举与 Trait

`Lucky/Source/Lucky/Scene/Components/ComponentType.h`：

```cpp
enum class ComponentType : uint8_t
{
    None = 0,
    Transform,
    Light,
    MeshFilter,
    MeshRenderer,
    SpriteRenderer,
    PostProcessVolume,
    Camera,             // 新增
};
```

`Lucky/Source/Lucky/Scene/Components/Components.h` 增加 include 与 Trait 特化：

```cpp
#include "CameraComponent.h"
// ...

template<> struct ComponentTrait<CameraComponent>
{
    static constexpr ComponentType Type = ComponentType::Camera;
};
```

### 4.4 CameraRenderData 结构体

**头文件**：放在 [Renderer3D.h](../../Lucky/Source/Lucky/Renderer/Renderer3D.h) 中（不必单独文件）。`Renderer3D.h` 需 include `Camera.h` 以获得 `ProjectionType`（PhaseR32 已将其放在 `Camera.h` 中）。

```cpp
/// <summary>
/// 相机渲染数据：Renderer3D::BeginScene 的相机侧输入
/// EditorCamera 与 SceneCamera 都被折算成此结构后进入统一渲染路径
/// </summary>
struct CameraRenderData
{
    glm::mat4 ViewMatrix{ 1.0f };                       // 视图矩阵
    glm::mat4 ProjectionMatrix{ 1.0f };                 // 投影矩阵
    glm::vec3 Position{ 0.0f };                         // 相机世界坐标

    // ---- CSM 计算所需（仅透视投影下有意义） ----
    ProjectionType Projection = ProjectionType::Perspective;
    float NearClip = 0.01f;                             // 近裁剪面
    float FOV = 45.0f;                                  // 垂直张角（度）
    float AspectRatio = 1.0f;                           // 宽高比
};
```

### 4.5 Renderer3D::BeginScene 重载

`Lucky/Source/Lucky/Renderer/Renderer3D.h`：

```cpp
static void BeginScene(const EditorCamera& camera, const SceneLightData& lightData);
static void BeginScene(const CameraRenderData& cam, const SceneLightData& lightData);
```

`Renderer3D.cpp`：
- 原 `BeginScene(EditorCamera&, ...)` 改成薄转发：构造 `CameraRenderData` 后调用矩阵版
- 矩阵版 `BeginScene(const CameraRenderData&, ...)` 承接原实现主体
- CSM 段落里的 `camera.GetNear()` / `GetFOV()` / `GetAspectRatio()` / `GetViewMatrix()` 全部替换为 `cam.NearClip / cam.FOV / cam.AspectRatio / cam.ViewMatrix`
- 在 CSM 段落最前面加防御：`if (cam.Projection != ProjectionType::Perspective) { s_Data.ShadowEnabled = false; return; }`（或跳过 CSM 计算，具体位置以现有代码结构为准）

**EditorCamera 转发实现**（使用 PhaseR32 后的长命名）：

```cpp
void Renderer3D::BeginScene(const EditorCamera& camera, const SceneLightData& lightData)
{
    CameraRenderData cam;
    cam.ViewMatrix = camera.GetViewMatrix();
    cam.ProjectionMatrix = camera.GetProjectionMatrix();
    cam.Position = camera.GetPosition();
    cam.Projection = ProjectionType::Perspective;
    cam.NearClip = camera.GetPerspectiveNearClip();
    cam.FOV = camera.GetPerspectiveVerticalFOV();
    cam.AspectRatio = camera.GetAspectRatio();

    BeginScene(cam, lightData);
}
```

### 4.6 Scene 变更

#### 4.6.1 Scene.h 新增

```cpp
/// <summary>
/// 查找场景中的主相机实体
/// 遍历所有拥有 CameraComponent 的实体，返回第一个 Primary=true 的
/// 若不存在，返回无效 Entity
/// </summary>
Entity GetPrimaryCameraEntity();
```

`RenderSceneImpl` 签名调整（详见决策点 3）：

```cpp
// 原
void RenderSceneImpl(EditorCamera& camera);

// 改为
void RenderSceneImpl(const CameraRenderData& cam);
```

#### 4.6.2 Scene.cpp 变更

**`OnRenderEditor`**：由"直接转发相机"改为"构造 CameraRenderData 再转发"（使用 PhaseR32 后的长命名）：

```cpp
void Scene::OnRenderEditor(EditorCamera& camera)
{
    CameraRenderData cam;
    cam.ViewMatrix = camera.GetViewMatrix();
    cam.ProjectionMatrix = camera.GetProjectionMatrix();
    cam.Position = camera.GetPosition();
    cam.Projection = ProjectionType::Perspective;
    cam.NearClip = camera.GetPerspectiveNearClip();
    cam.FOV = camera.GetPerspectiveVerticalFOV();
    cam.AspectRatio = camera.GetAspectRatio();

    RenderSceneImpl(cam);
}
```

**`OnRenderRuntime`**：从空壳变为真实实现：

```cpp
void Scene::OnRenderRuntime()
{
    Entity primary = GetPrimaryCameraEntity();
    if (!primary)
    {
        return;
    }

    const auto& transform = primary.GetComponent<TransformComponent>();
    const auto& cameraComp = primary.GetComponent<CameraComponent>();

    CameraRenderData cam;
    cam.ViewMatrix = glm::inverse(transform.GetWorldTransform());
    cam.ProjectionMatrix = cameraComp.Camera.GetProjectionMatrix();
    cam.Position = transform.GetWorldPosition();
    cam.Projection = cameraComp.Camera.GetProjectionType();
    cam.NearClip = cameraComp.Camera.GetPerspectiveNearClip();
    cam.FOV = cameraComp.Camera.GetPerspectiveVerticalFOV();
    cam.AspectRatio = cameraComp.Camera.GetAspectRatio();

    RenderSceneImpl(cam);
}
```

**`GetPrimaryCameraEntity`**：

```cpp
Entity Scene::GetPrimaryCameraEntity()
{
    auto view = m_Registry.view<TransformComponent, CameraComponent>();
    for (auto entity : view)
    {
        const auto& cam = view.get<CameraComponent>(entity);
        if (cam.Primary)
        {
            return Entity{ entity, this };
        }
    }
    return Entity{};
}
```

**`OnViewportResize`**：从空函数变为遍历更新：

```cpp
void Scene::OnViewportResize(uint32_t width, uint32_t height)
{
    m_ViewportWidth = width;
    m_ViewportHeight = height;

    auto view = m_Registry.view<CameraComponent>();
    for (auto entity : view)
    {
        auto& cam = view.get<CameraComponent>(entity);
        if (!cam.FixedAspectRatio)
        {
            cam.Camera.SetViewportSize(width, height);
        }
    }
}
```

**`OnComponentAdded<CameraComponent>`**（模板显式特化，紧跟其它组件的特化）：

```cpp
template<>
void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
{
    if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
    {
        component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
    }
}
```

> 注意：Scene 的 `OnComponentAdded<T>` 有个 `static_assert(sizeof(TComponent) == 0)` 的兜底，未特化的组件类型会**编译报错**。所以 `CameraComponent` 的特化**必须写**，即便体内不做任何事，也要提供一个空的特化避免触发 static_assert。

### 4.7 SceneSerializer 变更

`SerializeEntity` 内新增一段（对齐 `LightComponent` 的写法）：

```cpp
if (entity.HasComponent<CameraComponent>())
{
    const auto& cc = entity.GetComponent<CameraComponent>();
    const auto& sc = cc.Camera;

    out << YAML::Key << "CameraComponent";
    out << YAML::BeginMap;

    out << YAML::Key << "Projection" << YAML::Value << static_cast<int>(sc.GetProjectionType());
    out << YAML::Key << "PerspectiveFOV" << YAML::Value << sc.GetPerspectiveVerticalFOV();
    out << YAML::Key << "PerspectiveNear" << YAML::Value << sc.GetPerspectiveNearClip();
    out << YAML::Key << "PerspectiveFar" << YAML::Value << sc.GetPerspectiveFarClip();
    out << YAML::Key << "OrthographicSize" << YAML::Value << sc.GetOrthographicSize();
    out << YAML::Key << "OrthographicNear" << YAML::Value << sc.GetOrthographicNearClip();
    out << YAML::Key << "OrthographicFar" << YAML::Value << sc.GetOrthographicFarClip();
    out << YAML::Key << "Primary" << YAML::Value << cc.Primary;
    out << YAML::Key << "FixedAspectRatio" << YAML::Value << cc.FixedAspectRatio;

    out << YAML::EndMap;
}
```

`Deserialize` 内新增一段：

```cpp
YAML::Node cameraNode = entity["CameraComponent"];
if (cameraNode)
{
    auto& cc = deserializedEntity.AddComponent<CameraComponent>();
    auto& sc = cc.Camera;

    sc.SetProjectionType(static_cast<ProjectionType>(cameraNode["Projection"].as<int>()));
    sc.SetPerspectiveVerticalFOV(cameraNode["PerspectiveFOV"].as<float>());
    sc.SetPerspectiveNearClip(cameraNode["PerspectiveNear"].as<float>());
    sc.SetPerspectiveFarClip(cameraNode["PerspectiveFar"].as<float>());
    sc.SetOrthographicSize(cameraNode["OrthographicSize"].as<float>());
    sc.SetOrthographicNearClip(cameraNode["OrthographicNear"].as<float>());
    sc.SetOrthographicFarClip(cameraNode["OrthographicFar"].as<float>());

    cc.Primary = cameraNode["Primary"].as<bool>();
    cc.FixedAspectRatio = cameraNode["FixedAspectRatio"].as<bool>();
}
```

### 4.8 InspectorPanel 变更

**AddComponent 弹窗**：在 `LightComponent` 的三种子类型之后、`PostProcessVolumeComponent` 之前加：

```cpp
DrawAddComponentMenuItem<CameraComponent>(entity, "Camera");
```

**组件绘制**（新增 `DrawComponent<CameraComponent>` 块，位置紧跟其它组件之后）：

```cpp
DrawComponent<CameraComponent>("Camera", entity, [](CameraComponent& cc)
{
    auto& sc = cc.Camera;

    // Projection 类型
    const char* projectionTypes[] = { "Perspective", "Orthographic" };
    int currentProj = static_cast<int>(sc.GetProjectionType());
    if (UI::PropertyCombo("Projection", currentProj, projectionTypes, IM_ARRAYSIZE(projectionTypes)))
    {
        sc.SetProjectionType(static_cast<ProjectionType>(currentProj));
    }

    // 按类型显示不同参数
    if (sc.GetProjectionType() == ProjectionType::Perspective)
    {
        float fov = sc.GetPerspectiveVerticalFOV();
        if (UI::PropertyFloat("Field of View", fov, 0.1f, 1.0f, 179.0f))
        {
            sc.SetPerspectiveVerticalFOV(fov);
        }

        float nearClip = sc.GetPerspectiveNearClip();
        if (UI::PropertyFloat("Near Clip", nearClip, 0.001f, 0.001f, 1000.0f))
        {
            sc.SetPerspectiveNearClip(nearClip);
        }

        float farClip = sc.GetPerspectiveFarClip();
        if (UI::PropertyFloat("Far Clip", farClip, 1.0f, 0.1f, 100000.0f))
        {
            sc.SetPerspectiveFarClip(farClip);
        }
    }
    else
    {
        float size = sc.GetOrthographicSize();
        if (UI::PropertyFloat("Size", size, 0.1f, 0.1f, 1000.0f))
        {
            sc.SetOrthographicSize(size);
        }

        float nearClip = sc.GetOrthographicNearClip();
        if (UI::PropertyFloat("Near Clip", nearClip, 0.1f, -1000.0f, 1000.0f))
        {
            sc.SetOrthographicNearClip(nearClip);
        }

        float farClip = sc.GetOrthographicFarClip();
        if (UI::PropertyFloat("Far Clip", farClip, 1.0f, 0.1f, 100000.0f))
        {
            sc.SetOrthographicFarClip(farClip);
        }
    }

    UI::PropertyBool("Primary", cc.Primary);
    UI::PropertyBool("Fixed Aspect Ratio", cc.FixedAspectRatio);
});
```

> 具体控件函数名（`PropertyFloat / PropertyBool / PropertyCombo`）对齐 [InspectorPanel.cpp](../../Luck3DApp/Source/Panels/InspectorPanel.cpp) 现有 `LightComponent` 的写法。

### 4.9 SceneViewportPanel 联动

现在 `SceneViewportPanel::OnUpdate` 里 Framebuffer resize 只更新了 `m_EditorCamera.SetViewportSize` 和 `Renderer3D::ResizePipeline`。P0.2 补一行：

```cpp
if (m_Scene)
{
    m_Scene->OnViewportResize(
        static_cast<uint32_t>(m_ViewportSize.x),
        static_cast<uint32_t>(m_ViewportSize.y));
}
```

> Game 面板（P0.6）之后也会调用一次 `OnViewportResize`。两个面板都调时，`Scene` 内部的 `m_ViewportWidth/Height` 会被后调者覆盖，这在 P0.2 无副作用（这两个字段目前只被 `OnViewportResize` 内部读，未来 P0.6 若真需要区分再引入"每相机独立视口"机制）。

### 4.10 默认场景补 Main Camera

修改 `EditorLayer::EnsureDefaultScene`（把那一行"暂缓"注释删掉，替换为真实创建）：

```cpp
Entity cameraEntity = scene->CreateEntity("Main Camera");
cameraEntity.AddComponent<CameraComponent>();   // Primary = true 是默认

auto& cameraTransform = cameraEntity.GetComponent<TransformComponent>();
cameraTransform.Translation = { 0.0f, 1.0f, 5.0f };
cameraTransform.SetRotationEuler(glm::vec3(glm::radians(-10.0f), 0.0f, 0.0f));   // 稍俯视，让 Cube 落在画面中央
```

---

## 5. 关键决策点与方案对比

### 5.1 【决策点 1】投影参数的角度单位：度 还是 弧度

#### 方案 A：内部存"度"（**推荐 ★★★**）

- **优点**
  - 完全对齐现有 `EditorCamera::m_FOV`（存度），项目内单位一致
  - Inspector 直接显示度，用户友好
  - Serializer 存储的是度，YAML 文件可读性好（`PerspectiveFOV: 45.0`）
  - 内部 `RecalculateProjection` 现场调用 `glm::radians()` 转弧度，与 `EditorCamera::UpdateProjection` 完全一致
- **缺点**
  - 每次投影重算多一次 `glm::radians` 调用（可忽略，投影重算频率极低）

#### 方案 B：内部存"弧度"

- **优点**：无需现场转换
- **缺点**：与 `EditorCamera` 不一致；Inspector 需要在读写时反复转换；Serializer 存弧度不直观

**结论**：方案 A。

### 5.2 【决策点 2】ProjectionType 的位置

**本决策已由 PhaseR32 完成**：`ProjectionType` 枚举已定义在 [Camera.h](../../Lucky/Source/Lucky/Renderer/Camera.h) 中（`namespace Lucky` 内、`class Camera` 外），与项目现有 `LightType / AssetType / ComponentType` 风格一致。

本 Phase 无需重新声明，只需在使用处 include `Camera.h` 即可：

- `Renderer3D.h` 已需 include `Camera.h`（`CameraRenderData::Projection` 字段需要）
- `CameraComponent.h` 已需 include `SceneCamera.h`，后者间接 include `Camera.h`
- `SceneSerializer.cpp` / `InspectorPanel.cpp` 使用时同样自然引入

### 5.3 【决策点 3】`RenderSceneImpl` 的签名

P0.1 里 `RenderSceneImpl(EditorCamera& camera)`，P0.2 要引入 `SceneCamera` / `CameraComponent`，如何设计？

#### 方案 A：签名改为 `RenderSceneImpl(const CameraRenderData& cam)`（**推荐 ★★★**）

- **优点**
  - `OnRenderEditor` 与 `OnRenderRuntime` 走同一份实现，一致性最强
  - `RenderSceneImpl` 不再感知具体的相机类型，纯输入函数
  - `Renderer3D::BeginScene` 也吃 `CameraRenderData`，两级接口对齐
- **缺点**
  - `OnRenderEditor` 需要多写几行"从 EditorCamera 构造 CameraRenderData"（可接受）
  - 与 Renderer3D 内部 CameraRenderData 有耦合（本来就要引入的类型）

#### 方案 B：保留 `RenderSceneImpl(EditorCamera&)`，`OnRenderRuntime` 内构造一个"假的 EditorCamera"喂进去

- **优点**：`RenderSceneImpl` 签名不动
- **缺点**：
  - `EditorCamera` 类构造代价大（包含焦点、pitch/yaw 等一堆无用状态），且要反向"从 view/proj 反推参数"，非常别扭
  - 与"SceneCamera 不响应输入"的设计定位矛盾

#### 方案 C：`RenderSceneImpl` 提供两个重载

- **优点**：两条路径独立
- **缺点**：内部有重复代码，违反 P0.1 里"渲染逻辑单一真源"的初衷

**结论**：方案 A。

### 5.4 【决策点 4】保留空派生 `SceneCamera` 而非直接用 `Camera`

PhaseR32 后 `Camera` 基类已具备全部投影能力，本 Phase 的 SceneCamera 可以有两种不同的处理：

#### 方案 A：保留空派生类 `class SceneCamera : public Camera {};`（**推荐 ★★★**）

- **优点**
  - 保持类型区分性：函数签名时 `SceneCamera&` 比 `Camera&` 语义更明确（"ECS 中的相机"与"一般相机"区分）
  - 未来加 ECS 相机专有字段（对齐 Unity `clearFlags / cullingMask / depth / viewportRect`）位置清晰、扩展平滑
  - 与 `EditorCamera` 平级派生，架构规整（Hazel Engine 也是这么做的）
- **缺点**：一个空派生类看起来"无必要"（但属于"预留"性质）

#### 方案 B：不建 `SceneCamera`，`CameraComponent` 直接持有 `Camera`

- **优点**：文件更少
- **缺点**
  - 未来加 ECS 相机字段时又要新建类，动到 `CameraComponent` 结构（涉及序列化兼容）
  - 与 Hazel Engine 参考实现不一致

**结论**：方案 A。

### 5.5 【决策点 5】Primary 唯一性的处理

场景里可以有多个 `CameraComponent`，但 `Primary=true` 逻辑上应该只有一个。

#### 方案 A：不做硬约束，`GetPrimaryCameraEntity` 返回第一个 Primary=true 的（**推荐 ★★★**）

- **优点**
  - 实现简单，无副作用
  - 序列化 / Undo / Prefab 等场景不会因"唯一性约束"引入怪异 bug
- **缺点**
  - 用户可能勾选多个 Primary，行为不直观（但也不出错）
  - Inspector 可选地做"勾选 Primary 时把其他 CameraComponent 的 Primary 置 false"（对齐 Unity `MainCamera` tag 语义），P0.2 阶段**不做**这一步，等 P0.6 或后续需要时再补

#### 方案 B：勾选 Primary 时自动把其它相机的 Primary 置 false

- **优点**：语义直观
- **缺点**
  - 需要在 Inspector 绘制回调里遍历 Scene 修改其它实体的组件（跨实体副作用）
  - Undo/Redo 的粒度变复杂（一次修改影响多个实体）
  - 对齐 Unity 需要的其实是"MainCamera 标签系统"，而非 bool；简单 bool + 遍历取第一是短期最优

#### 方案 C：改用"MainCamera Tag"标签系统

- **优点**：对齐 Unity
- **缺点**：需要引入 Tag/Layer 基础设施（当前项目没有），超出 P0.2 范围

**结论**：方案 A。

### 5.6 【决策点 6】正交投影下 CSM 的处理

`Renderer3D::BeginScene` 里的 CSM 计算基于"透视视锥体"的角点。正交下这套计算不适用。

#### 方案 A：正交下自动关闭 CSM（**推荐 ★★★**）

在 CSM 段落最前面加：

```cpp
if (cam.Projection != ProjectionType::Perspective)
{
    s_Data.ShadowEnabled = false;
    // 跳过 CSM 计算，其它渲染照常
}
else
{
    // 原 CSM 计算
}
```

- **优点**：实现简单，无副作用；正交相机下依然能出画面，只是无阴影
- **缺点**：正交相机不能看到方向光阴影（可接受，编辑场景很少用正交）

#### 方案 B：为正交投影实现"基于世界包围盒"的阴影视锥计算

- **优点**：功能完整
- **缺点**：工作量远超 P0.2 范围，属于阴影系统的独立议题

**结论**：方案 A。

### 5.7 【决策点 7】OnViewportResize 的调用方

#### 方案 A：由 `SceneViewportPanel` / 未来的 `GameViewportPanel` 自行调用（**推荐 ★★★**）

- **优点**
  - 与"面板拥有视口 Framebuffer"的现有架构一致
  - 每个面板 resize 时都会同步一次；虽然 Scene 内部会被"最后一个 resize 的面板"覆盖，但对 P0.2/P0.6 均无副作用
- **缺点**：无

#### 方案 B：由 EditorLayer 统一管理"当前主视口尺寸"并触发

- **优点**：Scene 只被推一次
- **缺点**：EditorLayer 要感知具体面板尺寸（当前不感知），耦合上移

**结论**：方案 A。

### 5.8 【决策点 8】CameraComponent 的图标

#### 方案 A：P0.2 阶段不加图标（**推荐 ★★★**）

- **优点**
  - `EditorIconManager::GetComponentIcon(ComponentType::Camera)` 返回 nullptr 时 UI 自动兜底（`ImGui` 会显示空位）
  - 图标缺失不阻塞任何功能
  - 图标资源可以在 P0.6 Game 面板落地时一起统一补
- **缺点**：Inspector 头图标和 AddComponent 菜单图标位置视觉上是空的

#### 方案 B：先加一个占位图标

- **优点**：视觉完整
- **缺点**：需要美术资源，且未来还要替换

**结论**：方案 A。若已有可用图标可以顺手加，不作强制要求。

---

## 6. 验收标准

本 Phase 完成后应满足：

1. **编译通过**：所有新增/修改文件项目正常编译
2. **零回归**：
   - 打开旧场景（无 CameraComponent）行为与改造前一致（`OnRenderEditor` 走矩阵版 `BeginScene`，但矩阵与原路径完全等价）
   - Scene 面板渲染画面与改造前**像素一致**（CSM 阴影效果一致）
3. **默认场景**：
   - 新建的默认场景包含 3 个实体：`Cube` / `Directional Light` / `Main Camera`
   - `Main Camera` 有 CameraComponent，`Primary=true`
4. **Inspector**：
   - 选中 Main Camera 实体，Inspector 显示 CameraComponent 面板，可切换 Perspective/Orthographic，可编辑 FOV / Near / Far / Size / Primary / Fixed Aspect Ratio
5. **AddComponent 菜单**：
   - 弹窗中有 "Camera" 项，已挂载 CameraComponent 的实体上此项置灰
6. **序列化往返**：
   - 修改 Main Camera 的 FOV 到 60，保存 → 重新打开场景 → FOV 仍为 60
7. **主相机查找**：
   - 手动在 `SceneViewportPanel::OnUpdate` 里临时把 `m_Scene->OnRenderEditor(m_EditorCamera)` 替换为 `m_Scene->OnRenderRuntime()`，画面从"EditorCamera 视角"切换为"Main Camera 视角"（Cube 在画面中央）
   - 验证完恢复原代码
8. **视口适配**：
   - Scene 面板拖动改变尺寸，`Main Camera` 的 aspect 自动跟随（`SceneCamera::GetAspectRatio` 与视口一致）；勾选 Fixed Aspect Ratio 后不再跟随
9. **正交模式**：
   - 把 Main Camera 切换为 Orthographic，画面能正常渲染（可能无阴影，但不崩溃）

---

## 7. 后续 Phase 的接入点预告

| 位置 | 后续 Phase | 会加什么 |
|------|-----------|---------|
| `Scene::OnRenderRuntime` | P0.6 | 由 `GameViewportPanel::OnUpdate` 调用；无主相机时 Game 面板显示 "No Camera Rendering" 占位 |
| `CameraComponent` | Phase 2（脚本字段） | 脚本层暴露 `CameraComponent` 类型，`GetComponent<Camera>()` 可访问 |
| `Scene::GetPrimaryCameraEntity` | Phase 1（脚本） | 脚本 API `Scene.MainCamera` 转发到此 |
| `SceneCamera` | 阴影系统扩展 | 正交投影下的 CSM 或替代阴影方案 |
| `Primary` bool | 未来 | 可扩展为 `MainCamera Tag` 系统 |

### 7.1 命名一致性技术债（不在本 Phase 处理）

本 Phase 引入的 `CameraRenderData` 与项目内既有 `SceneLightData` 命名风格不完全对齐：一个以 "Render" 为语义定位（"Renderer 消费的相机数据"），一个以 "Scene" 为语义定位（"从场景收集的光照数据"）。

两者本质是**同一层次的对称类型** ?? 都是"每帧从相机/场景源收集、供 Renderer3D 消费的 CPU 端聚合数据"。它们最终写入 GPU UBO 的字段基本对齐 Unreal `FSceneView` / Unity SRP `CameraData / LightData` 的双子系统结构。命名理应对称。

主流引擎的对齐方式：Unreal 用 `FSceneView / FLightSceneInfo`，Unity SRP 用 `CameraData / LightData`，Filament 用 `CameraInfo / LightManager`?? 都以"Renderer 视角"命名，而非"Scene 视角"。

**建议未来做一次独立的整洁性重构**：`SceneLightData → LightRenderData`。改名机械但影响面涉及 Scene / Renderer3D / ShadowPass 等多处，属于纯粹的整洁性工作，与相机系统无耦合，适合作为独立小 Phase 完成。

本 Phase **不承担**此改动，理由：
- P0.2 焦点是相机组件落地与 Runtime 路径打通，命名整洁不阻塞任何功能
- 与相机系统解耦，分开做每次都干净、验证明确
- `SceneLightData` 已被多处稳定引用，一起改会扩散边界

---

## 8. 变更清单速览

- **新增**
  - `SceneCamera.h`：空派生类 `class SceneCamera : public Camera {};`（无 `.cpp`）
  - `CameraComponent.h`：组件结构体
  - `Renderer3D.h`：`CameraRenderData` 结构体 + `BeginScene(CameraRenderData&, ...)` 重载；include `Camera.h`
  - `ComponentType::Camera` 枚举值 + `ComponentTrait<CameraComponent>` 特化
  - `Scene::GetPrimaryCameraEntity`
  - `Scene::OnComponentAdded<CameraComponent>` 特化
- **修改**
  - `Scene::OnRenderEditor / OnRenderRuntime`：内部改走 `CameraRenderData`
  - `Scene::OnViewportResize`：遍历更新非固定 aspect 的 `CameraComponent`
  - `Scene::RenderSceneImpl`：签名改为 `const CameraRenderData&`
  - `Renderer3D::BeginScene(EditorCamera&, ...)`：改为薄转发
  - `Renderer3D` CSM 段：`camera.Get*` 全部替换为 `cam.*`；正交下跳过
  - `SceneSerializer`：新增 CameraComponent 序列化 / 反序列化分支
  - `InspectorPanel`：AddComponent 菜单加 Camera；新增 CameraComponent 绘制块
  - `SceneViewportPanel::OnUpdate`：Framebuffer resize 时调用 `Scene::OnViewportResize`
  - `EditorLayer::EnsureDefaultScene`：新增 Main Camera 实体
- **删除**
  - 无

---

## 9. 与 PhaseR32 的关系

本 Phase 严重依赖 PhaseR32 的产出：

| 来自 PhaseR32 | 本 Phase 如何用 |
|-------------|--------------|
| `Camera` 基类（投影参数 + `RecalculateProjection`） | `SceneCamera` 直接继承；Inspector / Serializer 调用 `sc.GetPerspective* / GetOrthographic*` 长命名接口 |
| `ProjectionType` 枚举 | `CameraRenderData::Projection` 字段、Inspector 下拉框、SceneSerializer 字段均直接引用 |
| `EditorCamera::GetPerspective*` 长命名 | `Scene::OnRenderEditor` 和 `Renderer3D::BeginScene(EditorCamera&)` 转发时使用（而非短命名） |

若 PhaseR32 未完成就开始本 Phase，会遇到两种麻烦：
1. `SceneCamera` 需自行实现全套投影参数（与 `EditorCamera` 重复）
2. `ProjectionType` 定义位置需中途重新决策

所以**推荐时序：PhaseR32 → P0.2**。
