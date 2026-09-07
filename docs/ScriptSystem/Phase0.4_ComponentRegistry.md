# Phase 0.4：ComponentRegistry 组件注册表（C 档）

## 1. 概述

P0.4 目标：把"逐组件 if"重复散落在 6 处的问题一次性归并到统一的 **`ComponentRegistry`** 中，作为 Phase 1 `ScriptComponent` 落地的公共前置，同时为后续 "Inspector 组件拖动排序" 铺路。

本 Phase 采用 **C 档范围**：一次性把 Copy / Serialize / Deserialize / Inspector Draw / Icon / AddComponent 菜单 / Hierarchy 图标条 六件事全部统一到 Registry。

### 具体产出

1. `ComponentDescriptor` 结构体：承载单个组件类型的所有元信息（类型、名称、图标、Copy / Serialize / Deserialize / Draw / AddDefault 回调）
2. `ComponentRegistry` 静态类：全局注册表 + `ForEach / GetByType / RegisterAll` 接口
3. 每种业务组件的一份 `ComponentXxxRegistration.cpp` 或聚合的 `ComponentRegistrations.cpp`：把当前散落在 Serializer / Inspector 的 lambda / free function 抽出到组件旁边
4. `SceneSerializer::Serialize / Deserialize` 内部改为 `ComponentRegistry::ForEach` 循环
5. `Scene::Copy` 内部改为 `ComponentRegistry::ForEach` 循环（P0.3 的硬编码模板列表退役）
6. `InspectorPanel::DrawComponents / DrawAddComponentButton` 改为 Registry 循环
7. `SceneHierarchyPanel::DrawEntityComponentIcons` 改为 Registry 循环（右侧组件图标条不再硬编码 6 个 if）
8. `EditorIconManager` 组件图标注册保留，但通过 Descriptor 的 `IconPath` 集中在 Components 一侧声明

### 前置依赖

- P0.1~P0.3 完成
- 现有 `ComponentType` 枚举 + `ComponentTrait<T>::Type` 特化保持不变（Registry 复用它做类型键）
- 现有 `ComponentIconResolver<T>` 编译期模板特化保持不变（Registry 的 IconFn 通过它转发，`LightComponent` 的子类型分派自动生效）

### 本 Phase **不做**的事

- **不做"Inspector 组件拖动排序"功能**（P0.4.x 独立小 Phase）?? 本 Phase 只把 Registry 铺好，让 Ordering 变成"增量特性"
- **不动 `Scene::OnComponentAdded<T>` 特化**：这些特化是编译期分派，逻辑集中在 2 个组件（`MeshRendererComponent` / `CameraComponent`），Registry 化收益极低，保持原状
- **不改 `ComponentType` 枚举 / `ComponentTrait` 特化的声明位置**：现有位置合理，Registry 与它们**共存并使用**
- **不做 Remove 保护 / 组件依赖声明**（Unity RequireComponent 语义）：属于独立议题，未来在 Descriptor 增加字段即可
- **不改现有 `.luck3d` 场景文件的 YAML 格式**：Registry 化前后序列化产物必须**逐字节等价**（除排序差异外）

---

## 2. 涉及的文件

### 需要新建

| 文件 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Scene/Components/ComponentDescriptor.h` | `ComponentDescriptor` 结构体定义（Copy/Serialize/Deserialize/Draw/AddDefault/IconFn 六大回调 + Name + Type） |
| `Lucky/Source/Lucky/Scene/Components/ComponentRegistry.h` | `ComponentRegistry` 静态类接口 |
| `Lucky/Source/Lucky/Scene/Components/ComponentRegistry.cpp` | `ComponentRegistry` 实现（内部维护有序 `std::vector<ComponentDescriptor>` + `ComponentType → index` 查表） |
| `Lucky/Source/Lucky/Scene/Components/ComponentRegistrations.cpp` | 集中注册所有 9 个组件的元信息（每个组件的 Serialize / Deserialize / Draw 三个 free function 写在此文件的匿名命名空间内） |

### 需要修改

| 文件 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Serialization/SceneSerializer.cpp` | `SerializeEntity` / `Deserialize` 内组件段用 `ComponentRegistry::ForEach` 循环替换硬编码 if |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | `Scene::Copy` 用 `ComponentRegistry::ForEach` 替换 `CopyComponents<...>` 硬编码模板列表 |
| `Luck3DApp/Source/Panels/InspectorPanel.h` | 移除 `DrawComponent<T>` / `DrawAddComponentMenuItem<T>` 模板；新增运行时版 `DrawComponentHeader(entity, desc)` 私有方法 |
| `Luck3DApp/Source/Panels/InspectorPanel.cpp` | `DrawComponents` / `DrawAddComponentButton` 改为 Registry 循环；原有各组件绘制 lambda 迁移到 `ComponentRegistrations.cpp` |
| `Luck3DApp/Source/Panels/SceneHierarchyPanel.cpp` | `DrawEntityComponentIcons` 改为 Registry 循环 |
| `Luck3DApp/Source/EditorLayer.cpp` | `OnAttach` 中新增一行 `ComponentRegistry::RegisterAll()` 显式注册调用（对齐 `AssetInspectorRegistry::Register` 模式） |

### 无需改动但需要关注

- `ComponentType.h` / `Components.h`：保持不变。`ComponentType` 枚举 + `ComponentTrait<T>::Type` 特化继续存在，作为 Registry 的类型键来源
- `EditorIconManager`：保持不变。Registry 通过 IconFn 调用 `ComponentIconResolver<T>::GetIcon(component)`，自动继承 `LightComponent` 子类型分派
- 各组件头文件（`TransformComponent.h` 等）：保持不变。所有 Registry 元信息在 `ComponentRegistrations.cpp` 一处声明，组件文件不引入编辑器/序列化依赖

---

## 3. 现状分析

### 3.1 当前"逐组件重复分支"的分布

| 组件 | Scene::Copy 列表 | Scene::OnComponentAdded | Serializer 写 | Serializer 读 | Inspector Draw | Inspector AddMenu | Hierarchy 图标条 | EditorIconManager 图标 |
|------|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| `NameComponent` | ? | ○ | ? | ? | 顶部特殊 | ? | ? | ? |
| `TransformComponent` | ? | ○ | ? | ? | ? | ? | ? | ? |
| `RelationshipComponent` | ? | ○ | ? | ? | ? | ? | ? | ? |
| `MeshFilterComponent` | ? | ○ | ? | ? | ? | ? | ? | ? |
| `MeshRendererComponent` | ? | ● | ? | ? | ? | ? | ? | ? |
| `SpriteRendererComponent` | ? | ● | ? | ? | ? | ? | ? | ? |
| `LightComponent` | ? | ○ | ? | ? | ? | ?（Directional/Point/Spot 三条） | ?（子类型图标） | ?（+ LightType 子图标） |
| `PostProcessVolumeComponent` | ? | ○ | ? | ? | ? | ? | ? | ? |
| `CameraComponent` | ? | ● | ? | ? | ? | ? | ? | ? |

图例：? = 有分支；○ = 空特化；● = 有实质逻辑；? = 无需

**观察**：
- 单个组件从新增到出现在编辑器全部 UI 中，至少需要在 **7 处**同步（Trait + Copy + OnComponentAdded + Serializer 读写 + Inspector Draw + Inspector AddMenu + Hierarchy 图标条）
- 除 `OnComponentAdded` 之外，其余 6 处**都可以走 Registry 一次注册**

### 3.2 SceneSerializer.cpp 现状规模

- 全文件 **734 行**
- `SerializeEntity` 内 9 个组件的 `if (entity.HasComponent<T>()) { ... }` 分支，占约 **200 行**
- `Deserialize` 内 9 个组件的 `YAML::Node xxNode = entity["XxComponent"];` 分支，占约 **250 行**
- Registry 化后，这 450 行会**均摊到 `ComponentRegistrations.cpp` 的 9 个 free function 中**，且每个 function 只关心自己组件的 YAML 结构 ?? 单个组件的读写紧邻在一起，可读性显著提升

### 3.3 InspectorPanel 现状

- `InspectorPanel::DrawComponents` 内 8 个 `DrawComponent<T>(name, entity, [](T&){...})` 硬编码调用
- `InspectorPanel::DrawAddComponentButton` 内 9 条硬编码 `DrawAddComponentMenuItem<T>` + 3 条 Light 子类型 `UI::IconMenuItem`
- `DrawComponent<T>` 模板本身承担了通用外壳：`TreeNode` + 图标 + Settings 按钮 + Remove 菜单 ?? 这部分需要**运行时化**（拿 Descriptor 里的 Name / IconFn 而不是编译期 T）

### 3.4 SceneHierarchyPanel::DrawEntityComponentIcons 现状

`SceneHierarchyPanel.cpp` 中：

```cpp
if (entity.HasComponent<MeshFilterComponent>())    icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::MeshFilter));
if (entity.HasComponent<MeshRendererComponent>())  icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::MeshRenderer));
if (entity.HasComponent<SpriteRendererComponent>()) icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::SpriteRenderer));
if (entity.HasComponent<LightComponent>())
{
    LightType lightType = entity.GetComponent<LightComponent>().Type;
    icons.push_back(&EditorIconManager::GetLightIcon(lightType));
}
if (entity.HasComponent<PostProcessVolumeComponent>()) icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::PostProcessVolume));
if (entity.HasComponent<CameraComponent>())        icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::Camera));
```

6 条硬编码 `HasComponent<T>`，其中 `LightComponent` 还要手动做子类型分派。Registry 化后变成一次循环，`LightComponent` 的子类型分派由 IconFn 自动完成。

### 3.5 现有可复用的基础设施

- **`ComponentType` 枚举**（`ComponentType.h`）：所有业务组件都有对应值，Registry 用作类型键
- **`ComponentTrait<T>::Type`**（`Components.h`）：编译期 T → ComponentType 反射
- **`ComponentIconResolver<T>::GetIcon(component)`**（`EditorIconManager.h`）：编译期模板 + `LightComponent` 运行时子类型特化。Registry 的 IconFn 直接调用它，无需重复实现子类型分派
- **`AssetInspectorRegistry`**（`AssetInspectorRegistry.h`）：项目内既有 Registry 模式，静态局部 map + `std::function` + `EditorLayer::OnAttach` 显式注册 ?? P0.4 直接对齐这一模式，保持项目一致性

---

## 4. 详细设计

### 4.1 ComponentDescriptor.h

```cpp
#pragma once

#include "ComponentType.h"

#include "Lucky/Core/Base.h"

#include <functional>
#include <string>

namespace YAML { class Emitter; class Node; }

namespace Lucky
{
    class Entity;
    class Texture2D;

    /// <summary>
    /// 组件描述符：承载一种组件类型的所有跨系统元信息
    /// 由 ComponentRegistry 集中持有，供 Copy / Serializer / InspectorPanel / SceneHierarchyPanel 消费
    /// </summary>
    struct ComponentDescriptor
    {
        // ---- 静态元信息 ----

        ComponentType Type = ComponentType::None;   // 类型键（与 ComponentTrait<T>::Type 一致）
        std::string Name;                           // 显示名（Inspector 标题 / AddMenu 项名，如 "Mesh Renderer"）
        std::string SerializedKey;                  // YAML 键（如 "MeshRendererComponent"）

        // ---- 回调 ----

        /// <summary>
        /// 深拷贝：将 src 中的该组件（若存在）以值语义拷贝到 dst
        /// 直接走 entt::registry::emplace_or_replace，不触发 OnComponentAdded
        /// </summary>
        using CopyFn = std::function<void(entt::registry& dst, entt::entity dstEnt,
                                          entt::registry& src, entt::entity srcEnt)>;
        CopyFn Copy;

        /// <summary>
        /// 序列化：如果 entity 拥有该组件，向 out 写入完整 YAML 段（含 SerializedKey 顶层键）
        /// 若 entity 无该组件则不写任何内容
        /// </summary>
        using SerializeFn = std::function<void(YAML::Emitter& out, Entity entity)>;
        SerializeFn Serialize;

        /// <summary>
        /// 反序列化：从 entityNode 中读取该组件的 YAML 段并添加到 entity
        /// 若 entityNode 中不含该组件的键（!entityNode[SerializedKey]），实现方需自行无操作返回
        /// </summary>
        using DeserializeFn = std::function<void(Entity entity, const YAML::Node& entityNode)>;
        DeserializeFn Deserialize;

        /// <summary>
        /// Inspector 绘制：若 entity 拥有该组件，绘制组件面板内容（不含通用外壳 TreeNode / Settings 按钮）
        /// 外壳由 InspectorPanel::DrawComponentHeader 统一绘制
        /// </summary>
        using DrawFn = std::function<void(Entity entity)>;
        DrawFn Draw;

        /// <summary>
        /// 图标解析：给定 entity（Entity 上必须拥有该组件），返回其显示图标
        /// 允许基于组件实例内部字段返回不同图标（LightComponent 依 LightType 分派）
        /// </summary>
        using IconFn = std::function<const Ref<Texture2D>&(Entity entity)>;
        IconFn GetIcon;

        /// <summary>
        /// AddComponent 菜单默认创建：向 entity 添加该组件（走 Entity::AddComponent，触发 OnComponentAdded）
        /// nullptr 表示"该组件不出现在 AddComponent 菜单中"（如 Transform / Relationship / Name）
        /// </summary>
        using AddDefaultFn = std::function<void(Entity entity)>;
        AddDefaultFn AddDefault;

        // ---- 存在性判定 ----

        /// <summary>
        /// 判定 entity 是否拥有该组件
        /// </summary>
        using HasFn = std::function<bool(Entity entity)>;
        HasFn Has;

        // ---- 展示控制 ----

        bool ShowInAddMenu = true;      // 是否出现在 AddComponent 菜单
        bool ShowInHierarchyIcons = true;   // 是否出现在 Hierarchy 右侧图标条
        bool CanRemove = true;          // Settings 弹窗是否显示 Remove 项（Transform / Relationship / Name 不可移除）
    };
}
```

要点：
- 所有回调均为 `std::function`，允许 lambda 捕获（AddDefault 特殊变体如 Light 三子类型可用）
- **`HasFn` 是必需的**：Registry 循环需要"给定运行时 ComponentType，判定 entity 是否有此组件"，编译期 T 已丢失，`entt::registry::has<T>` 无法直接用 ?? 因此每个组件注册时封装一个 `[](Entity e) { return e.HasComponent<T>(); }`
- `SerializedKey` 单独字段而非从 `Name` 派生，因为项目里显示名（"Mesh Renderer"）与 YAML 键（"MeshRendererComponent"）**不一致**，硬派生会破坏兼容性

### 4.2 ComponentRegistry.h / .cpp

```cpp
#pragma once

#include "ComponentDescriptor.h"

#include <functional>
#include <vector>

namespace Lucky
{
    /// <summary>
    /// 组件注册表：集中管理所有组件类型的 ComponentDescriptor
    /// 
    /// 生命周期：EditorLayer::OnAttach 中调用 RegisterAll() 完成一次性注册；EditorLayer::OnDetach 中调用 Clear()
    /// 遍历顺序：与 RegisterAll 内注册顺序一致（保证 Inspector / Serializer 输出稳定）
    /// </summary>
    class ComponentRegistry
    {
    public:
        /// <summary>
        /// 一次性注册所有内置组件描述符
        /// 由 EditorLayer::OnAttach 调用（或引擎侧启动阶段调用，见【决策点 3】）
        /// </summary>
        static void RegisterAll();

        /// <summary>
        /// 清空注册表（EditorLayer::OnDetach 调用）
        /// </summary>
        static void Clear();

        /// <summary>
        /// 通过 ComponentType 查描述符
        /// </summary>
        /// <param name="type">组件类型</param>
        /// <returns>描述符指针；未注册时返回 nullptr</returns>
        static const ComponentDescriptor* GetByType(ComponentType type);

        /// <summary>
        /// 按注册顺序遍历所有描述符
        /// </summary>
        static void ForEach(const std::function<void(const ComponentDescriptor&)>& callback);

        /// <summary>
        /// 按注册顺序返回全部描述符
        /// </summary>
        static const std::vector<ComponentDescriptor>& All();
    private:
        /// <summary>
        /// 单个描述符注册（RegisterAll 内部使用）
        /// </summary>
        /// <param name="desc">描述符</param>
        static void Register(ComponentDescriptor desc);
    };
}
```

`.cpp` 实现要点：

```cpp
namespace Lucky
{
    static std::vector<ComponentDescriptor>& GetStorage()
    {
        static std::vector<ComponentDescriptor> s_Storage;
        return s_Storage;
    }

    static std::unordered_map<ComponentType, size_t>& GetIndexMap()
    {
        static std::unordered_map<ComponentType, size_t> s_IndexMap;
        return s_IndexMap;
    }

    void ComponentRegistry::Register(ComponentDescriptor desc)
    {
        LF_CORE_ASSERT(desc.Type != ComponentType::None, "ComponentRegistry::Register - Type must not be None");
        LF_CORE_ASSERT(GetIndexMap().find(desc.Type) == GetIndexMap().end(),
                       "ComponentRegistry::Register - Duplicate registration");

        GetIndexMap()[desc.Type] = GetStorage().size();
        GetStorage().push_back(std::move(desc));
    }

    // RegisterAll / GetByType / ForEach / All / Clear 实现从略，直接查 storage / indexMap
}
```

**RegisterAll 的实现**（在 [ComponentRegistrations.cpp](#48-componentregistrationscpp) 中）会按 **UI 期望顺序**（Transform 优先 → 显示相关 → Light → PostProcess → Camera）一次性注册所有组件。

### 4.3 SceneSerializer.cpp 重构

**改造前**（734 行）：

```cpp
static void SerializeEntity(YAML::Emitter& out, Entity entity)
{
    out << YAML::BeginMap;
    out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

    // Name 组件
    if (entity.HasComponent<NameComponent>()) { /* 6 行 */ }
    // Transform 组件
    if (entity.HasComponent<TransformComponent>()) { /* 10 行 */ }
    // Relationship / Light / MeshFilter / MeshRenderer / Sprite / PostProcess / Camera ... 200+ 行
    
    out << YAML::EndMap;
}
```

**改造后**：

```cpp
static void SerializeEntity(YAML::Emitter& out, Entity entity)
{
    out << YAML::BeginMap;
    out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

    ComponentRegistry::ForEach([&](const ComponentDescriptor& desc)
    {
        if (desc.Serialize)
        {
            desc.Serialize(out, entity);   // 内部自行判断 Has，无组件则不写
        }
    });

    out << YAML::EndMap;
}
```

`Deserialize` 内组件段同理：

```cpp
for (auto entity : entities)
{
    uint64_t uuid = entity["Entity"].as<uint64_t>();

    std::string entityName;
    YAML::Node nameNode = entity["NameComponent"];
    if (nameNode)
    {
        entityName = nameNode["Name"].as<std::string>();
    }

    Entity deserializedEntity = scene->CreateEntity(uuid, entityName);

    ComponentRegistry::ForEach([&](const ComponentDescriptor& desc)
    {
        if (desc.Deserialize)
        {
            desc.Deserialize(deserializedEntity, entity);   // 内部自行判断 entity[desc.SerializedKey] 是否存在
        }
    });
}
```

> **注**：`NameComponent` 因为 `CreateEntity(uuid, entityName)` 已经处理了名字，`NameComponent` 的 `DeserializeFn` 应为空（或仅在未来 Tag/Layer 字段加入时才有内容）。

### 4.4 Scene::Copy 重构

P0.3 的实现：

```cpp
CopyComponents<
    NameComponent, TransformComponent, RelationshipComponent,
    MeshFilterComponent, MeshRendererComponent, SpriteRendererComponent,
    LightComponent, PostProcessVolumeComponent, CameraComponent
>(dstRegistry, dstEntity, srcRegistry, srcEntity);
```

Registry 化后：

```cpp
ComponentRegistry::ForEach([&](const ComponentDescriptor& desc)
{
    if (desc.Copy)
    {
        desc.Copy(dstRegistry, dstEntity, srcRegistry, srcEntity);
    }
});
```

`Scene.cpp` 顶部匿名命名空间中的 `CopyComponentIfExists<T>` / `CopyComponents<T...>` 两个 helper **删除**。`CopyFn` 的实现方式在【4.8】中给出（每个组件的注册项都写一个类型确定的 lambda 复用同一份 helper 模板 ?? 该 helper 会移到 `ComponentDescriptor` 附近的匿名命名空间或作为静态 template）。

### 4.5 InspectorPanel 重构

#### 4.5.1 移除模板 `DrawComponent<T>` / `DrawAddComponentMenuItem<T>`

改为运行时版：

```cpp
private:
    /// <summary>
    /// 绘制组件通用外壳（TreeNode + 图标 + Name + Settings 按钮 + Remove 菜单）
    /// 打开时调用 desc.Draw(entity) 填充组件内容
    /// </summary>
    void DrawComponentHeader(Entity entity, const ComponentDescriptor& desc);

    /// <summary>
    /// 绘制 AddComponent 菜单的一项
    /// </summary>
    void DrawAddComponentMenuItem(Entity entity, const ComponentDescriptor& desc);
```

`DrawComponentHeader` 内部结构与原 `DrawComponent<T>` 模板完全等价，区别仅在于：
- 组件名从 `const std::string& name` 参数改为 `desc.Name`
- 图标从 `ComponentIconResolver<T>::GetIcon(component)` 改为 `desc.GetIcon(entity)`
- TreeNode 的唯一 ID 从 `typeid(T).hash_code()` 改为 `static_cast<int>(desc.Type)`
- Remove 时从 `entity.RemoveComponent<T>()` 改为运行时版（见【决策点 5】）
- `CanRemove == false` 的组件（Transform / Relationship / Name）Settings 弹窗不显示 Remove

#### 4.5.2 DrawComponents 重构

**改造前**：

```cpp
void InspectorPanel::DrawComponents(Entity entity)
{
    // Name 顶部特殊输入框（保留）
    if (entity.HasComponent<NameComponent>()) { /* InputText 逻辑 */ }
    
    DrawComponent<TransformComponent>("Transform", entity, [](TransformComponent& t) { ... });
    DrawComponent<LightComponent>("Light", entity, [](LightComponent& l) { ... });
    DrawComponent<MeshFilterComponent>(meshName + " (Mesh Filter)", entity, [](...){ ... });
    // ... 5 个 DrawComponent
    
    if (entity.HasComponent<MeshRendererComponent>()) { /* 材质编辑器 */ }
    if (entity.HasComponent<SpriteRendererComponent>()) { /* 材质编辑器 */ }

    DrawAddComponentButton(entity);
}
```

**改造后**：

```cpp
void InspectorPanel::DrawComponents(Entity entity)
{
    // Name 顶部输入框仍为特殊路径（不走 Registry Draw），因为它是 Inspector 顶部的裸 InputText，不套 TreeNode 外壳
    if (entity.HasComponent<NameComponent>())
    {
        /* InputText 逻辑，保持不变 */
    }

    // 主组件列表：Registry 循环
    ComponentRegistry::ForEach([&](const ComponentDescriptor& desc)
    {
        if (desc.Has && desc.Has(entity) && desc.Draw)
        {
            DrawComponentHeader(entity, desc);
        }
    });

    // 材质编辑器（MeshRenderer / Sprite 附加块）
    // 见【决策点 6】：MaterialEditor 是"跨组件的附加块"，不属于任何单个组件的 Draw 范围
    DrawMaterialEditors(entity);

    UI::Draw::HorizontalLine();
    DrawAddComponentButton(entity);
}
```

#### 4.5.3 DrawAddComponentButton 重构

**改造前**（40 行硬编码）：

```cpp
if (UI::BeginPopup(popupID))
{
    DrawAddComponentMenuItem<MeshFilterComponent>(entity, "Mesh Filter");
    DrawAddComponentMenuItem<MeshRendererComponent>(entity, "Mesh Renderer");
    // ... 
    
    // Light 三子类型
    bool alreadyHasLight = entity.HasComponent<LightComponent>();
    if (UI::IconMenuItem(GetLightIcon(Directional), "Directional Light", alreadyHasLight)) { ... }
    if (UI::IconMenuItem(GetLightIcon(Point), "Point Light", alreadyHasLight)) { ... }
    if (UI::IconMenuItem(GetLightIcon(Spot), "Spot Light", alreadyHasLight)) { ... }

    DrawAddComponentMenuItem<PostProcessVolumeComponent>(entity, "Post Process Volume");
    DrawAddComponentMenuItem<CameraComponent>(entity, "Camera");
    UI::EndPopup();
}
```

**改造后**（15 行）：

```cpp
if (UI::BeginPopup(popupID))
{
    ComponentRegistry::ForEach([&](const ComponentDescriptor& desc)
    {
        if (desc.ShowInAddMenu && desc.AddDefault)
        {
            bool alreadyHas = desc.Has(entity);
            const Ref<Texture2D>& icon = desc.GetIcon
                ? desc.GetIcon(entity)  // 若 entity 已有该组件，取实例图标（LightComponent 走 LightType 子图标）；否则取默认图标（IconFn 需要能兜底，见下）
                : EditorIconManager::GetComponentIcon(desc.Type);

            if (UI::IconMenuItem(icon, desc.Name.c_str(), alreadyHas))
            {
                desc.AddDefault(entity);
            }
        }
    });
    UI::EndPopup();
}
```

**关于 Light 三子类型菜单项**：见【决策点 4】。P0.4 采用 **Descriptor 层允许"一个组件类型对应多个菜单项"** 的机制（`std::vector<AddMenuItem>` 而非单个 `AddDefault`），将在【4.6】展开。

### 4.6 Light 组件三菜单项的 Descriptor 表达

因为 `LightComponent` 在 AddComponent 菜单里对应 3 条项（Directional / Point / Spot），Descriptor 需要能表达"多菜单项"，同时仍是**同一个组件类型**（Copy / Serialize / Draw 只有一份）。

**方案**：把 `AddDefault` + `Name`（作为菜单项标签）+ `GetIcon`（作为菜单项图标）三者组合成一个可数组化的 `AddMenuItem` 子结构：

```cpp
struct AddMenuItem
{
    std::string Label;          // 菜单显示名
    std::function<const Ref<Texture2D>&()> GetIcon;   // 菜单项图标（无 entity 上下文，取默认图标）
    std::function<void(Entity)> AddFn;                // 点击后执行
};

struct ComponentDescriptor
{
    // ...
    std::vector<AddMenuItem> AddMenuItems;   // 空数组表示不出现在 AddComponent 菜单
    // ...
};
```

`LightComponent` 注册时：

```cpp
desc.AddMenuItems = {
    { "Directional Light",
      []() -> const Ref<Texture2D>& { return EditorIconManager::GetLightIcon(LightType::Directional); },
      [](Entity e) { e.AddComponent<LightComponent>(LightType::Directional); } },
    { "Point Light",
      []() -> const Ref<Texture2D>& { return EditorIconManager::GetLightIcon(LightType::Point); },
      [](Entity e) { e.AddComponent<LightComponent>(LightType::Point); } },
    { "Spot Light",
      []() -> const Ref<Texture2D>& { return EditorIconManager::GetLightIcon(LightType::Spot); },
      [](Entity e) { e.AddComponent<LightComponent>(LightType::Spot); } },
};
```

其它组件（如 `MeshRendererComponent`）注册时只有一项：

```cpp
desc.AddMenuItems = {
    { "Mesh Renderer",
      []() -> const Ref<Texture2D>& { return EditorIconManager::GetComponentIcon(ComponentType::MeshRenderer); },
      [](Entity e) { e.AddComponent<MeshRendererComponent>(); } },
};
```

`DrawAddComponentButton` 循环变为：

```cpp
ComponentRegistry::ForEach([&](const ComponentDescriptor& desc)
{
    bool alreadyHas = desc.Has(entity);
    for (const AddMenuItem& item : desc.AddMenuItems)
    {
        if (UI::IconMenuItem(item.GetIcon(), item.Label.c_str(), alreadyHas))
        {
            item.AddFn(entity);
        }
    }
});
```

原 `ComponentDescriptor::AddDefault` 字段**删除**，被 `AddMenuItems[0].AddFn` 取代。

### 4.7 SceneHierarchyPanel::DrawEntityComponentIcons 重构

**改造前**：

```cpp
std::vector<const Ref<Texture2D>*> icons;
if (entity.HasComponent<MeshFilterComponent>())         icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::MeshFilter));
if (entity.HasComponent<MeshRendererComponent>())       icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::MeshRenderer));
if (entity.HasComponent<SpriteRendererComponent>())     icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::SpriteRenderer));
if (entity.HasComponent<LightComponent>())
{
    LightType lightType = entity.GetComponent<LightComponent>().Type;
    icons.push_back(&EditorIconManager::GetLightIcon(lightType));
}
if (entity.HasComponent<PostProcessVolumeComponent>())  icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::PostProcessVolume));
if (entity.HasComponent<CameraComponent>())             icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::Camera));
```

**改造后**：

```cpp
std::vector<const Ref<Texture2D>*> icons;
ComponentRegistry::ForEach([&](const ComponentDescriptor& desc)
{
    if (!desc.ShowInHierarchyIcons)
    {
        return;
    }
    if (desc.Has && desc.Has(entity) && desc.GetIcon)
    {
        icons.push_back(&desc.GetIcon(entity));
    }
});
```

`LightComponent` 子类型分派**由 IconFn 自动完成**（IconFn 内部访问 `entity.GetComponent<LightComponent>().Type` 转到 `GetLightIcon`）。

### 4.8 ComponentRegistrations.cpp

集中注册所有组件。文件顶层结构（示例，Transform + Light 展开，其余对齐）：

```cpp
#include "lcpch.h"

#include "ComponentRegistry.h"

#include "Components.h"

#include "Lucky/Scene/Entity.h"
#include "Lucky/Serialization/YamlHelpers.h"
#include "Lucky/Editor/EditorIconManager.h"
#include "Lucky/UI/PropertyGrid.h"
#include "Lucky/UI/Widgets.h"
#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Asset/AssetManager.h"

#include <yaml-cpp/yaml.h>
#include <imgui.h>

namespace Lucky
{
    namespace
    {
        // ======== 通用 helper ========

        /// <summary>
        /// 通用组件深拷贝：从 srcEntity 拷贝 T 类型组件到 dstEntity（若存在）
        /// 直接走 entt::registry，不触发 OnComponentAdded
        /// </summary>
        template<typename TComponent>
        void CopyComponentValue(entt::registry& dst, entt::entity dstEnt,
                                entt::registry& src, entt::entity srcEnt)
        {
            if (src.has<TComponent>(srcEnt))
            {
                const TComponent& srcComp = src.get<TComponent>(srcEnt);
                dst.emplace_or_replace<TComponent>(dstEnt, srcComp);
            }
        }

        // ======== TransformComponent ========

        static void Serialize_Transform(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<TransformComponent>())
            {
                return;
            }
            const TransformComponent& t = entity.GetComponent<TransformComponent>();
            out << YAML::Key << "TransformComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "Position" << YAML::Value << t.Translation;
            out << YAML::Key << "Rotation" << YAML::Value << t.GetRotation();
            out << YAML::Key << "Scale" << YAML::Value << t.Scale;
            out << YAML::EndMap;
        }

        static void Deserialize_Transform(Entity entity, const YAML::Node& entityNode)
        {
            YAML::Node node = entityNode["TransformComponent"];
            if (!node)
            {
                return;
            }
            TransformComponent& t = entity.GetComponent<TransformComponent>();
            t.Translation = node["Position"].as<glm::vec3>();
            t.SetRotation(node["Rotation"].as<glm::quat>());
            t.Scale = node["Scale"].as<glm::vec3>();
        }

        static void Draw_Transform(Entity entity)
        {
            TransformComponent& t = entity.GetComponent<TransformComponent>();
            UI::PropertyFloat3("Position", t.Translation, 0.01f);

            glm::vec3 rotationEuler = glm::degrees(t.GetRotationEuler());
            if (UI::PropertyFloat3("Rotation", rotationEuler, 1.0f))
            {
                t.SetRotationEuler(glm::radians(rotationEuler));
            }

            UI::PropertyFloat3("Scale", t.Scale, 0.01f);
        }

        // ======== LightComponent ========
        // 略：Serialize/Deserialize/Draw 从 SceneSerializer / InspectorPanel 原地迁移
        // 注意 AddMenuItems 三条子类型（Directional/Point/Spot）
        
        // ... 其余 7 个组件同构 ...
    }

    // ======== RegisterAll 一次性注册 ========

    void ComponentRegistry::RegisterAll()
    {
        // 顺序即 Inspector / Serializer 输出顺序：Transform 优先 → 显示相关 → Light → PostProcess → Camera
        // 顺序调整会影响 YAML 键序，但不影响加载正确性（yaml-cpp 按键查找而非位置）

        // ---- NameComponent（无 Draw，无 AddMenu，序列化最简） ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::None;  // 无对应枚举值：Name 是特殊系统组件
            // ... 见【决策点 2】：NameComponent 目前无 ComponentType 枚举值
        }

        // ---- TransformComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::Transform;
            desc.Name = "Transform";
            desc.SerializedKey = "TransformComponent";
            desc.Copy = &CopyComponentValue<TransformComponent>;
            desc.Serialize = &Serialize_Transform;
            desc.Deserialize = &Deserialize_Transform;
            desc.Draw = &Draw_Transform;
            desc.GetIcon = [](Entity e) -> const Ref<Texture2D>&
            {
                return EditorIconManager::GetComponentIcon(ComponentType::Transform);
            };
            desc.Has = [](Entity e) { return e.HasComponent<TransformComponent>(); };
            desc.ShowInAddMenu = false;         // Transform 不允许通过 AddComponent 添加
            desc.ShowInHierarchyIcons = false;  // Hierarchy 图标条排除 Transform（现状即如此）
            desc.CanRemove = false;
            Register(std::move(desc));
        }

        // ---- MeshFilterComponent / MeshRendererComponent / SpriteRendererComponent / LightComponent
        //      / PostProcessVolumeComponent / CameraComponent 同构 ----
        // 详见【4.9】各组件片段

        // ---- RelationshipComponent（系统组件，仅需 Copy/Serialize/Deserialize） ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::None;    // 同 Name，见【决策点 2】
            // ...
        }
    }
}
```

### 4.9 各组件注册片段（简表）

| 组件 | ShowInAddMenu | ShowInHierarchyIcons | CanRemove | AddMenuItems 数量 |
|------|:-:|:-:|:-:|:-:|
| `NameComponent` | false | false | false | 0 |
| `TransformComponent` | false | false | false | 0 |
| `RelationshipComponent` | false | false | false | 0 |
| `MeshFilterComponent` | true | true | true | 1 |
| `MeshRendererComponent` | true | true | true | 1 |
| `SpriteRendererComponent` | true | true | true | 1 |
| `LightComponent` | true | true | true | **3**（Directional/Point/Spot） |
| `PostProcessVolumeComponent` | true | true | true | 1 |
| `CameraComponent` | true | true | true | 1 |

### 4.10 EditorLayer.cpp 接入

`OnAttach` 中，`SceneManager::Init()` 之后、`AssetInspectorRegistry::Register(...)` 附近，新增：

```cpp
// 组件注册表：集中注册所有组件的 Copy / Serialize / Draw / Icon / AddMenu 元信息
ComponentRegistry::RegisterAll();
```

`OnDetach` 中：

```cpp
ComponentRegistry::Clear();
```

---

## 5. 关键决策点与方案对比

### 5.1 【决策点 1】Registry 底层容器：`vector<Descriptor>` vs `unordered_map<ComponentType, Descriptor>`

Registry 存储 9~10 个描述符，遍历 + 按 Type 查两种访问模式。

#### 方案 A：`vector<Descriptor>` + 辅助 `unordered_map<ComponentType, size_t>`（**推荐 ★★★**）

- **优点**
  - `ForEach` 遍历顺序稳定 = 注册顺序 = Inspector / Serializer 输出顺序，UI 展示可预期
  - `GetByType` 走 index map，O(1)
  - 10 个元素规模下内存开销极小
- **缺点**
  - 维护两个容器需保持一致性（`Register` 时同步写入）

#### 方案 B：`unordered_map<ComponentType, Descriptor>` 单一容器

- **优点**：单容器，简单
- **缺点**
  - `ForEach` 顺序不可控，Inspector 每次启动可能顺序不同（用户体验差）
  - Serializer 输出的 YAML 键序会 flapping，影响 diff 阅读

#### 方案 C：`vector<Descriptor>` 单一容器（无 index map）

- **优点**：最简
- **缺点**：`GetByType` 变 O(n)。虽然 n=10 无所谓，但 Ordering 特性（P0.4.x）会频繁按 Type 查，重构麻烦

**结论**：**方案 A**。

### 5.2 【决策点 2】`NameComponent` / `RelationshipComponent` 是否要有 `ComponentType` 枚举值

现状：`ComponentType` 枚举中**没有** `Name` 和 `Relationship`（只有 Transform / Light / MeshFilter / MeshRenderer / SpriteRenderer / PostProcessVolume / Camera）。但 Registry 需要 `ComponentType` 作为主键。

#### 方案 A：给 Name / Relationship 补上 `ComponentType::Name` / `ComponentType::Relationship` 枚举值（**推荐 ★★★**）

- **优点**
  - Registry 主键统一，全部组件对齐
  - 未来若需要按 Type 查 Name 或 Relationship 也自然可行
  - 与"Registry 是所有组件的唯一真源"语义一致
- **缺点**
  - 需要修改 `ComponentType.h` 枚举 ?? 但这是加值，向后兼容（枚举底层类型 `uint8_t` 有 245 个空位）
  - 需要新增 `ComponentTrait<NameComponent>` / `ComponentTrait<RelationshipComponent>` 特化，一致性小改

#### 方案 B：Descriptor 允许 `Type == None`，Name / Relationship 特殊处理

- **优点**：不动 `ComponentType` 枚举
- **缺点**：Registry 内出现"没有 Type 的 Descriptor"，破坏"以 Type 为主键"的语义，Index map 失效

**结论**：**方案 A**。在 P0.4 顺手补上枚举值。

### 5.3 【决策点 3】Registry 注册时机：全局静态 vs `EditorLayer::OnAttach` 显式

#### 方案 A：`EditorLayer::OnAttach` 显式调用 `ComponentRegistry::RegisterAll()`（**推荐 ★★★**）

- **优点**
  - 与项目内 `AssetInspectorRegistry::Register` / `SceneManager::Init` / `EditorIconManager::Init` **完全一致的模式**
  - 生命周期可控：`OnDetach` 中 `Clear()`，便于重启编辑器
  - 依赖顺序显式：Registry 依赖 `EditorIconManager::Init`（IconFn 内会调 `EditorIconManager::GetXxx`），显式顺序不会出现"静态初始化顺序 fiasco"
  - 未来若 Registry 需要读取用户配置文件调整默认顺序，OnAttach 位置能拿到 App 上下文
- **缺点**
  - `RegisterAll` 是硬编码列表，加新组件要改这里 ?? 但这本来就是"集中注册"的核心价值

#### 方案 B：全局静态初始化（每个组件旁边一个 `static ComponentRegistrar<T> reg`）

- **优点**：新增组件时"注册代码放在组件旁边"、`RegisterAll` 自动扩展
- **缺点**
  - 静态初始化顺序问题：Registry 与 `EditorIconManager` 谁先谁后不确定，IconFn 内取图标可能崩
  - 项目内没有先例，引入这种模式与现有 Registry 风格冲突
  - 静态构造函数在 static lib 中可能被链接器优化掉（Lucky 是静态库，Luck3DApp 不主动引用 `ComponentXxxRegistration.cpp` 中的符号时，注册器不会执行）?? 项目里已经碰到过类似问题（如 `AssetTypeTraits` 的静态注册要用 `#pragma comment` 或强制引用）

#### 方案 C：延迟单例 `ComponentRegistry::Instance().Init()`

- **优点**：懒加载
- **缺点**：需要在每个访问点做初始化判定，或依赖首次调用触发；生命周期含糊

**结论**：**方案 A**。与项目风格完全一致。

### 5.4 【决策点 4】Light 三菜单项的表达方式

`LightComponent` 在 AddComponent 菜单里对应 3 条项，但同一个组件类型。

#### 方案 A：Descriptor 内 `std::vector<AddMenuItem>`（**推荐 ★★★**）

- **优点**
  - 单一数据源：一个组件类型对应多少菜单项完全由该组件的 Registration 决定
  - `DrawAddComponentButton` 的循环极简单：`for (item : desc.AddMenuItems) UI::IconMenuItem(...)`
  - 未来出现"MeshFilter 一个类型三种图元（Cube / Sphere / Plane）"等情况可复用
- **缺点**
  - Descriptor 结构膨胀（多一个 vector 字段）
  - 单菜单项组件也要写 `desc.AddMenuItems = {{...}}` 一行

#### 方案 B：Descriptor 内保留单 `AddDefault`，Light 走"特殊菜单钩子" `MenuBuilderFn`

```cpp
using MenuBuilderFn = std::function<void(Entity)>;  // 由该 fn 自己调 UI::IconMenuItem
```

- **优点**：单菜单项组件更简单，只填 `AddDefault`
- **缺点**
  - Light 一个组件"跳出" Registry 循环，成为"异类"
  - 未来任何"多子类型菜单"组件都要走这条特殊路径
  - Registry 的"数据驱动"承诺被打折

#### 方案 C：把 Light 三子类型拆成三个 `ComponentType`（`DirectionalLight` / `PointLight` / `SpotLight`）

- **优点**：菜单项完全对齐
- **缺点**
  - `LightComponent` 结构不变（内部仍是一个 struct + LightType 字段），拆枚举语义不真实
  - Serializer / Copy 都要额外处理"三个 Type 映射到同一个 LightComponent"
  - 破坏现有 ECS 数据模型

**结论**：**方案 A**。

### 5.5 【决策点 5】运行时 Remove 组件的实现

编译期 `entity.RemoveComponent<T>()` 需要 T，Registry 循环中只有 `ComponentType`。

#### 方案 A：Descriptor 加 `RemoveFn`（**推荐 ★★★**）

```cpp
using RemoveFn = std::function<void(Entity)>;
RemoveFn Remove;
// 注册时：
desc.Remove = [](Entity e) { e.RemoveComponent<TComponent>(); };
```

- **优点**
  - 与其它 Fn 对称，Descriptor 自包含
  - `CanRemove == false` 时 Remove 为空 lambda 或 nullptr，Settings 弹窗自然不显示 Remove 项
- **缺点**：Descriptor 多一个字段

#### 方案 B：`entt::registry::remove_all(entity, ComponentType)`

- **优点**：不需要 RemoveFn 字段
- **缺点**：entt 不提供"按 typeid 动态 remove"的公开接口，需要维护 `type_index → remove function` 映射，工作量与方案 A 相当但更迂回

**结论**：**方案 A**。

### 5.6 【决策点 6】`MaterialEditor` 附加块的归属

`InspectorPanel::DrawComponents` 现状末尾有：

```cpp
if (entity.HasComponent<MeshRendererComponent>())
{
    // 绘制所有 Material 的 MaterialEditor 面板
}
if (entity.HasComponent<SpriteRendererComponent>())
{
    // 绘制 Material 的 MaterialEditor 面板
}
```

这不是"组件的 Draw"，而是"跨组件的附加块" ?? Material 编辑器在**组件面板之外、AddComponent 按钮之前**独立绘制。

#### 方案 A：`InspectorPanel::DrawMaterialEditors(entity)` 保持硬编码（**推荐 ★★★**）

- **优点**
  - `MaterialEditor` 是资产层的编辑器，不属于任何单个组件的 Draw 范围
  - 附加块的数量有限（当前只有 Mesh / Sprite 两条），未来若增加也是明确的"哪些组件持有 Material"的问题
  - 与 Registry 的"每个组件一份 Descriptor" 语义正交
- **缺点**
  - Inspector 内保留少量硬编码 ?? 但这本来就是 Inspector 层的组合逻辑

#### 方案 B：Descriptor 加 `DrawExtraFn`，在组件面板之后调用

- **优点**：Inspector 完全循环化
- **缺点**
  - MaterialEditor 位置需要"所有组件绘制完之后再绘制"，与"逐组件绘制外壳" 交织，实现困难
  - 违反职责单一：Descriptor.Draw 是组件内容绘制，Extra 是跨组件的编辑器，两码事

**结论**：**方案 A**。保留 `DrawMaterialEditors` 为 `InspectorPanel` 的方法，Registry 不管这块。

### 5.7 【决策点 7】`OnComponentAdded` 是否 Registry 化

`MeshRendererComponent`（材质列表按 SubMesh 数量初始化）+ `CameraComponent`（`SetViewportSize`）有实质逻辑，其它 7 个空实现。

#### 方案 A：保持编译期特化，不进 Registry（**推荐 ★★★**）

- **优点**
  - 编译期分派零开销
  - 现有代码零改动
  - Registry 化收益极低（只有 2 个组件有实质逻辑）
- **缺点**：新增有初始化逻辑的组件时，需要"注册 Descriptor + 写 OnComponentAdded 特化"两处 ?? 但这类组件本来就少

#### 方案 B：Descriptor 加 `OnAddedFn`

- **优点**：完全 Registry 化
- **缺点**
  - 破坏 `Entity::AddComponent<T>` 编译期调用链
  - `Entity::AddComponent` 内需要额外查 Registry 才知道要不要跑 `OnAddedFn`，绕圈子
  - Copy 时"绕过 OnAdded" 的语义反而更难表达

**结论**：**方案 A**。`OnComponentAdded` 保持不动。

### 5.8 【决策点 8】`ComponentRegistrations.cpp` 单文件 vs 按组件拆多个文件

#### 方案 A：单文件 `ComponentRegistrations.cpp`（**推荐 ★★★**）

- **优点**
  - 一处集中，`RegisterAll` 内部按顺序 push，UI 展示顺序一目了然
  - 9 个组件的 Serialize / Deserialize / Draw 都在同一个文件的匿名命名空间，读者阅读整套组件行为时无需在多文件间跳
  - 文件规模可控（预估 500~700 行，与现有 `SceneSerializer.cpp` 相当）
- **缺点**
  - 文件较大 ?? 但相较于 `SceneSerializer.cpp` 734 行 + `InspectorPanel.cpp` 469 行的合计规模，反而是净减少

#### 方案 B：每个组件一个 `TransformComponentRegistration.cpp` 等

- **优点**
  - 单文件规模小
  - 新增组件时只加文件，不改现有文件
- **缺点**
  - 9~10 个文件，包含引用重复
  - 注册顺序需要额外的机制保证（每个文件的静态初始化顺序不可控，见【决策点 3 方案 B】）
  - 需要修改 build script（`premake5.lua`）添加新文件

**结论**：**方案 A**。

### 5.9 【决策点 9】YAML 兼容性保证

C 档改造涉及 SceneSerializer 全量重写，需保证**旧 .luck3d 文件 100% 能被新代码读取**、**新代码写出的 YAML 与旧代码逐字节等价**（除组件顺序可能变化外）。

#### 方案 A：Serialize/Deserialize 严格按现有 SceneSerializer 逐 case 迁移（**推荐 ★★★**）

- **优点**
  - 每个 `Serialize_Xxx` / `Deserialize_Xxx` free function 是从现有代码"copy-paste"过来，只是外壳从 `if (entity.HasComponent<T>()) { ... }` 变成 `void Serialize_Xxx(...)`
  - 现有单元/回归测试（若有）零改动
  - 版本回溯友好
- **缺点**
  - 无

#### 方案 B：借机会引入"Component 版本号" / "字段可选性表" 等元数据

- **优点**：为未来的组件版本迁移铺路
- **缺点**
  - 超出 P0.4 "纯重构" 定位
  - 引入的复杂度需要独立的 Phase 讨论

**结论**：**方案 A**。严格 1:1 迁移，YAML 格式完全不动。

### 5.10 【决策点 10】迁移期的组件绘制顺序稳定性

改造前 Inspector / Serializer 中组件顺序是硬编码的，改造后由 `RegisterAll` 内的注册顺序决定。为保证零回归，必须保证注册顺序与改造前一致。

**结论**：`RegisterAll` 严格按现有 Inspector 顺序注册：Transform → Light → MeshFilter → MeshRenderer → SpriteRenderer → PostProcessVolume → Camera；Name / Relationship 在最前面单独注册（Serializer 中 Name / Transform / Relationship 靠前）。

---

## 6. 验收标准

本 Phase 完成后应满足：

1. **编译通过**：所有新增/修改文件项目正常编译
2. **零回归**：
   - **加载现有场景**：`Assets/Scenes/New Scene.luck3d` 打开后画面像素级不变、Hierarchy 层级完全一致
   - **保存后 YAML 等价**：现有场景加载后立即保存，YAML 文件与保存前**逐行等价**（可能有键序差异，但 `git diff` 只应看到无实质变化）
   - **Inspector 显示**：选中任意实体，组件列表 UI 与改造前**像素级等价**（组件顺序、图标、字段布局、TreeNode 展开/折叠状态、Settings 按钮位置）
   - **AddComponent 菜单**：弹出后菜单项数量、顺序、Light 三子类型、置灰行为与改造前完全一致
   - **Hierarchy 右侧图标条**：每个实体的组件图标数量、顺序、Light 子类型图标与改造前完全一致
3. **`Scene::Copy` 行为不变**：P0.3 验收标准 T1~T7 全部继续通过（Registry 循环拷贝与硬编码模板列表拷贝等价）
4. **代码规模**：
   - `SceneSerializer.cpp` 从 734 行减到 ~200 行以内（只保留场景级的 Environment / RootEntityOrder / Entities 遍历）
   - `InspectorPanel.cpp` 的 `DrawComponents / DrawAddComponentButton` 从 350+ 行减到 ~50 行
   - `SceneHierarchyPanel::DrawEntityComponentIcons` 中的 6 条 `if` 归为一个循环
5. **新增组件可用性验证**：向 Registry 加一个新的空组件（如临时的 `TestComponent`），只需：
   - 定义 `struct TestComponent { int Foo = 0; };`
   - 在 `ComponentType` 添加 `Test` 枚举值 + `ComponentTrait<TestComponent>` 特化
   - 在 `ComponentRegistrations.cpp` 的 `RegisterAll` 内追加一段
   - 无需改动 `SceneSerializer` / `Scene::Copy` / `InspectorPanel::DrawComponents` / `DrawAddComponentButton` / `SceneHierarchyPanel::DrawEntityComponentIcons` 任何一处
   
   验证完删除 `TestComponent`
6. **手动验证**（选中一个 Cube 实体）：
   - Inspector 中所有组件字段编辑后立即生效（Transform / Light / MeshRenderer / Camera 等）
   - AddComponent 菜单添加 Light 三种子类型 → 图标正确、Inspector 显示对应类型的字段（Point 有 Range、Spot 有 Cutoff）
   - Remove Component 从 Settings 弹窗执行 → 组件消失
   - 保存场景 → 关闭 → 重新打开 → 所有字段值保留
7. **无副作用日志**：加载 / 保存 / 编辑 / Copy 过程中控制台不应出现新增的 `LF_CORE_ERROR` / `LF_CORE_WARN`

---

## 7. 后续 Phase 的接入点预告

### 7.1 Phase 1 - ScriptComponent 接入

Phase 1 引入 `ScriptComponent { std::string ClassName; }` 时，接入 Registry 只需 **3 处改动**：

1. `ComponentType.h`：加 `Script` 枚举值
2. `Components.h`：`ComponentTrait<ScriptComponent>` 特化
3. `ComponentRegistrations.cpp`：`RegisterAll` 内追加一段：
   ```cpp
   ComponentDescriptor desc;
   desc.Type = ComponentType::Script;
   desc.Name = "Script";
   desc.SerializedKey = "ScriptComponent";
   desc.Copy = &CopyComponentValue<ScriptComponent>;
   desc.Serialize = &Serialize_Script;
   desc.Deserialize = &Deserialize_Script;
   desc.Draw = &Draw_Script;   // 显示 ClassName 输入框（Phase 2 改下拉列表）
   // ...
   Register(std::move(desc));
   ```

**改造前的路径**（无 P0.4）：新增 `ScriptComponent` 需要修改 `SceneSerializer` / `Scene::Copy` / `InspectorPanel` / `SceneHierarchyPanel` **至少 4 个文件**共 60+ 行代码。P0.4 后只需 1 个新文件的 40 行。

### 7.2 P0.4.x - Inspector 组件拖动排序

C 档 Registry 是拖动排序的**充分必要基础设施**。P0.4 完成后，Ordering 是纯增量特性：

1. 新增 `ComponentOrderComponent { std::vector<ComponentType> Order; }`
2. 该组件本身走 Registry 序列化（`Serialize_ComponentOrder / Deserialize_ComponentOrder` 加进 Registrations，一处生效）
3. `InspectorPanel::DrawComponents` 循环改为按 `GetEffectiveDrawOrder(entity)` 遍历
4. 拖动交互 = ImGui `BeginDragDropSource/Target` + `std::swap` Order 数组
5. Transform 锁顶 = `GetEffectiveDrawOrder` 内强制 Transform 排第一

**关键点**：Ordering 落地时**不改动 P0.4 建立的任何 Registry 结构**。

### 7.3 P0.4.y - Remove 保护与组件依赖（Unity RequireComponent 语义）

未来若需要 `MeshRenderer` 依赖 `MeshFilter`（移除 `MeshFilter` 时同步移除 `MeshRenderer` 或提示），可在 Descriptor 增加：

```cpp
std::vector<ComponentType> DependsOn;   // 依赖的其它组件类型
std::vector<ComponentType> Requires;    // 要求同时存在的组件类型
```

配合 `AddDefault` / Remove 处进行依赖处理。

---

## 8. 变更清单速览

### 新增文件

- `Lucky/Source/Lucky/Scene/Components/ComponentDescriptor.h`
- `Lucky/Source/Lucky/Scene/Components/ComponentRegistry.h`
- `Lucky/Source/Lucky/Scene/Components/ComponentRegistry.cpp`
- `Lucky/Source/Lucky/Scene/Components/ComponentRegistrations.cpp`

### 修改文件

- `Lucky/Source/Lucky/Scene/Components/ComponentType.h`：新增 `Name` / `Relationship` 枚举值
- `Lucky/Source/Lucky/Scene/Components/Components.h`：新增 `ComponentTrait<NameComponent>` / `ComponentTrait<RelationshipComponent>` 特化
- `Lucky/Source/Lucky/Serialization/SceneSerializer.cpp`：`SerializeEntity / Deserialize` 组件段改为 Registry 循环，删除 9 个 `if HasComponent` 分支和 9 个 `YAML::Node xxNode = entity["Xx"]` 分支
- `Lucky/Source/Lucky/Scene/Scene.cpp`：`Scene::Copy` 改为 Registry 循环，删除顶部匿名命名空间的 `CopyComponentIfExists<T>` / `CopyComponents<T...>`
- `Luck3DApp/Source/Panels/InspectorPanel.h`：删除模板 `DrawComponent<T>` / `DrawAddComponentMenuItem<T>`；新增 `DrawComponentHeader(entity, desc)` / `DrawAddComponentMenuItem(entity, desc)` / `DrawMaterialEditors(entity)`
- `Luck3DApp/Source/Panels/InspectorPanel.cpp`：`DrawComponents / DrawAddComponentButton / DrawComponentHeader` 全部改为 Registry 驱动
- `Luck3DApp/Source/Panels/SceneHierarchyPanel.cpp`：`DrawEntityComponentIcons` 内 6 条硬编码 if 改为 Registry 循环
- `Luck3DApp/Source/EditorLayer.cpp`：`OnAttach` 新增 `ComponentRegistry::RegisterAll()`；`OnDetach` 新增 `ComponentRegistry::Clear()`

### 删除

- `InspectorPanel::DrawComponent<T>` 模板方法
- `InspectorPanel::DrawAddComponentMenuItem<T>` 模板方法
- `Scene.cpp` 顶部匿名命名空间的 `CopyComponentIfExists<T>` / `CopyComponents<T...>`
- `SceneSerializer::SerializeEntity` 内 9 个 `if HasComponent<T>` 分支
- `SceneSerializer::Deserialize` 内 9 个 `YAML::Node xxNode = entity["Xx"]` 分支
- `SceneHierarchyPanel::DrawEntityComponentIcons` 内 6 条硬编码 if
- `InspectorPanel::DrawAddComponentButton` 内 9 条 `DrawAddComponentMenuItem<T>` + Light 三条子类型硬编码

---

## 9. 实施顺序建议

C 档改动面较大，建议按下面的**增量顺序**逐步落地，每一步都能编译通过 + 手动验证：

1. **Step 1**：新建 `ComponentDescriptor.h` / `ComponentRegistry.h/.cpp`（空实现，未注册任何组件）+ `ComponentType.h` 补 Name/Relationship 枚举值 + `Components.h` 补 Trait 特化 + `EditorLayer::OnAttach` 加 `RegisterAll` 调用（此时 `RegisterAll` 内为空）
2. **Step 2**：`ComponentRegistrations.cpp` 逐个组件迁移 Serialize/Deserialize/Draw 到 Registry；每加一个组件就在原 `SceneSerializer.cpp` / `InspectorPanel.cpp` 中把对应分支**注释掉**并添加 Registry 循环兜底
3. **Step 3**：全部 9 个组件迁移完成后，在 `SceneSerializer::SerializeEntity / Deserialize` 中把硬编码分支彻底删除，只留 Registry 循环
4. **Step 4**：`Scene::Copy` 切换为 Registry 循环，删除 P0.3 的 `CopyComponents<...>`
5. **Step 5**：`InspectorPanel::DrawComponents / DrawAddComponentButton` 切换为 Registry 循环
6. **Step 6**：`SceneHierarchyPanel::DrawEntityComponentIcons` 切换为 Registry 循环
7. **Step 7**：删除所有过渡期的注释代码 + 手动跑完验收标准第 6 条

**回滚点**：Step 3 结束前，如果发现 YAML 兼容性问题，可直接把注释掉的硬编码分支恢复，Registry 循环不影响原代码正确性。

