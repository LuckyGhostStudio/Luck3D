
# 组件注册表与 Inspector 增强设计文档

> **文档版本**：v1.0  
> **创建日期**：2026-04-08  
> **前置依赖**：ECS Phase1~3（Transform 层级系统）、MaterialSystem Phase6（DirectionalLight 组件）、Serialization（场景序列化）  
> **文档说明**：本文档详细设计 Inspector 面板的组件管理功能增强，包括：组件删除（带保护机制）、AddComponent 按钮、组件注册表系统。提供三种可行方案的完整对比与实现细节，可直接对照编写代码。

---

## 目录

- [一、现状分析](#一现状分析)
  - [1.1 当前 ECS 架构概览](#11-当前-ecs-架构概览)
  - [1.2 现有组件清单](#12-现有组件清单)
  - [1.3 现有 Inspector 面板实现](#13-现有-inspector-面板实现)
  - [1.4 现有问题清单](#14-现有问题清单)
- [二、需求定义](#二需求定义)
  - [2.1 功能需求](#21-功能需求)
  - [2.2 Unity 参考](#22-unity-参考)
- [三、方案一：轻量级硬编码方案](#三方案一轻量级硬编码方案)
  - [3.1 设计思路](#31-设计思路)
  - [3.2 DrawComponent 模板修改](#32-drawcomponent-模板修改)
  - [3.3 AddComponent 按钮实现](#33-addcomponent-按钮实现)
  - [3.4 完整代码修改清单](#34-完整代码修改清单)
  - [3.5 优缺点分析](#35-优缺点分析)
- [四、方案二：组件描述符注册表（推荐）](#四方案二组件描述符注册表推荐)
  - [4.1 设计思路](#41-设计思路)
  - [4.2 核心数据结构](#42-核心数据结构)
  - [4.3 ComponentRegistry 类设计](#43-componentregistry-类设计)
  - [4.4 组件注册实现](#44-组件注册实现)
  - [4.5 InspectorPanel 重构](#45-inspectorpanel-重构)
  - [4.6 SceneHierarchyPanel 集成](#46-scenehierarchypanel-集成)
  - [4.7 序列化系统影响分析](#47-序列化系统影响分析)
  - [4.8 完整代码修改清单](#48-完整代码修改清单)
  - [4.9 优缺点分析](#49-优缺点分析)
- [五、方案三：模板 Traits 编译期约束方案](#五方案三模板-traits-编译期约束方案)
  - [5.1 设计思路](#51-设计思路)
  - [5.2 ComponentTraits 定义](#52-componenttraits-定义)
  - [5.3 DrawComponent 模板修改](#53-drawcomponent-模板修改)
  - [5.4 AddComponent 列表维护](#54-addcomponent-列表维护)
  - [5.5 完整代码修改清单](#55-完整代码修改清单)
  - [5.6 优缺点分析](#56-优缺点分析)
- [六、三方案综合对比](#六三方案综合对比)
  - [6.1 对比表](#61-对比表)
  - [6.2 推荐优先级](#62-推荐优先级)
- [七、方案二详细实现指南](#七方案二详细实现指南)
  - [7.1 新增文件清单](#71-新增文件清单)
  - [7.2 修改文件清单](#72-修改文件清单)
  - [7.3 实现步骤](#73-实现步骤)
- [八、验证方法](#八验证方法)
- [九、后续扩展预留](#九后续扩展预留)

---

## 一、现状分析

### 1.1 当前 ECS 架构概览

项目基于 **entt** 库实现 ECS 系统，核心类关系如下：

```
┌──────────────────┐     ┌──────────────────────────┐
│      Scene       │     │         Entity           │
│  m_Registry      │?────│  m_EntityID (entt::entity)│
│  m_EntityIDMap   │     │  m_Scene (Scene*)        │
│                  │     │                          │
│  CreateEntity()  │     │  AddComponent<T>()       │
│  DestroyEntity() │     │  GetComponent<T>()       │
│  OnComponentAdded│     │  HasComponent<T>()       │
└──────────────────┘     │  RemoveComponent<T>()    │
                         └──────────────────────────┘
```

**关键特性**：
- `Entity` 是轻量级句柄（entt::entity ID + Scene 指针），不持有组件数据
- 组件数据存储在 `entt::registry` 中，按类型连续排列
- 每个实体创建时自动添加 4 个默认组件：`IDComponent`、`NameComponent`、`TransformComponent`、`RelationshipComponent`
- `Scene::OnComponentAdded<T>` 模板特化用于组件添加时的初始化逻辑

### 1.2 现有组件清单

| 组件类型 | 文件路径 | 分类 | 创建时默认添加 | 说明 |
|---------|---------|------|:---:|------|
| `IDComponent` | `Components/IDComponent.h` | 系统内部 | ? | 实体唯一标识（UUID），不应在 Inspector 中显示 |
| `NameComponent` | `Components/NameComponent.h` | 系统内部 | ? | 实体名称，Inspector 中作为顶部输入框特殊处理 |
| `TransformComponent` | `Components/TransformComponent.h` | 核心 | ? | 位置/旋转/缩放，每个实体必须拥有 |
| `RelationshipComponent` | `Components/RelationshipComponent.h` | 核心 | ? | 父子关系（Parent UUID + Children UUID 列表），每个实体必须拥有 |
| `MeshFilterComponent` | `Components/MeshFilterComponent.h` | 渲染 | ? | 持有 Mesh 引用，通过 PrimitiveType 或外部模型创建 |
| `MeshRendererComponent` | `Components/MeshRendererComponent.h` | 渲染 | ? | 持有材质列表，材质索引对应 SubMesh 的 MaterialIndex |
| `DirectionalLightComponent` | `Components/DirectionalLightComponent.h` | 光照 | ? | 方向光参数（颜色 + 强度），方向由 Transform 旋转推导 |

### 1.3 现有 Inspector 面板实现

**InspectorPanel.h** 中的 `DrawComponent` 模板：

```cpp
template<typename TComponent, typename UIFunction>
void InspectorPanel::DrawComponent(const std::string& name, Entity entity, UIFunction OnOpened)
{
    if (entity.HasComponent<TComponent>())
    {
        const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap 
                                       | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
        
        auto& component = entity.GetComponent<TComponent>();
        ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail();
        
        bool opened = ImGui::TreeNodeEx((void*)typeid(TComponent).hash_code(), flags, name.c_str());
        
        ImGui::SameLine(contentRegionAvail.x - 18);
        
        // 组件设置按钮（所有组件都有，无保护机制）
        if (ImGui::Button("+", { 30, 30 }))
        {
            ImGui::OpenPopup("ComponentSettings");
        }

        bool componentRemoved = false;
        if (ImGui::BeginPopup("ComponentSettings"))
        {
            if (ImGui::MenuItem("Remove Component"))
            {
                componentRemoved = true;
            }
            ImGui::EndPopup();
        }
        
        if (opened)
        {
            OnOpened(component);
            ImGui::TreePop();
        }

        if (componentRemoved)
        {
            entity.RemoveComponent<TComponent>();
        }
    }
}
```

**InspectorPanel.cpp** 中的 `DrawComponents` 调用：

```cpp
void InspectorPanel::DrawComponents(Entity entity)
{
    // Name 组件（特殊处理，不使用 DrawComponent 模板）
    // ...InputText...
    
    DrawComponent<TransformComponent>("Transform", entity, [](TransformComponent& transform) { ... });
    DrawComponent<DirectionalLightComponent>("Directional Light", entity, [](DirectionalLightComponent& light) { ... });
    DrawComponent<MeshFilterComponent>(meshName + " (Mesh Filter)", entity, [](MeshFilterComponent& meshFilter) { ... });
    DrawComponent<MeshRendererComponent>("Mesh Renderer", entity, [&entity](MeshRendererComponent& meshRenderer) { ... });
    
    // 材质编辑器
    if (entity.HasComponent<MeshRendererComponent>()) { ... }
}
```

### 1.4 现有问题清单

| 编号 | 问题 | 影响 | 优先级 |
|------|------|------|--------|
| **I-01** | `TransformComponent` 可以被删除 | 删除后实体失去位置信息，渲染系统崩溃 | ?? 高 |
| **I-02** | 所有组件都显示 "Remove Component" 按钮 | 核心组件（Transform、Relationship）不应被删除 | ?? 高 |
| **I-03** | 没有 "Add Component" 功能 | 用户无法在 Inspector 中为已有实体添加新组件 | ?? 中 |
| **I-04** | 组件元数据分散 | 组件的约束规则（是否可删除、是否可添加等）没有统一管理 | ?? 中 |
| **I-05** | 新增组件需要修改多处代码 | Inspector、Hierarchy 创建菜单、序列化器都需要手动添加 | ?? 中 |
| **I-06** | `ComponentSettings` 弹出框 ID 冲突 | 所有组件共用同一个 Popup ID `"ComponentSettings"`，可能导致 ImGui 状态混乱 | ?? 中 |

---

## 二、需求定义

### 2.1 功能需求

1. **组件删除保护**：核心组件（Transform、Relationship）和系统组件（ID、Name）不可删除
2. **组件设置菜单**：每个可删除组件的标题栏显示设置按钮，点击弹出 "Remove Component" 菜单
3. **Add Component 按钮**：Inspector 底部显示 "Add Component" 按钮，点击弹出可添加的组件列表
4. **组件列表过滤**：已存在的组件不出现在 AddComponent 列表中（entt 不支持同类型多组件）
5. **组件分类显示**：AddComponent 列表按分类（渲染、光照等）组织

### 2.2 Unity 参考

Unity 的组件系统设计要点：

| 特性 | Unity 实现 | 本项目对应 |
|------|-----------|-----------|
| **Transform 不可删除** | Transform 组件没有右键删除选项 | `TransformComponent` 不显示删除按钮 |
| **Add Component 按钮** | Inspector 底部居中的大按钮，点击弹出搜索框 + 分类列表 | 简化版：按钮 + 弹出菜单（暂不需要搜索） |
| **组件菜单属性** | `[AddComponentMenu("Rendering/Light")]` 注解控制菜单路径 | `ComponentDescriptor::Category` 字段 |
| **DisallowMultipleComponent** | `[DisallowMultipleComponent]` 注解 | entt 天然不支持同类型多组件，无需额外处理 |
| **RequireComponent** | `[RequireComponent(typeof(MeshFilter))]` 注解 | 可在 `ComponentDescriptor` 中添加依赖列表（预留） |
| **组件右键菜单** | Reset / Remove Component / Move Up/Down / Copy/Paste | 当前只需 Remove Component |

---

## 三、方案一：轻量级硬编码方案

### 3.1 设计思路

不引入新的类或抽象层，直接在现有代码上做最小改动：
- 给 `DrawComponent` 模板增加 `removable` 参数控制是否显示删除按钮
- 在 `DrawComponents` 末尾硬编码 AddComponent 按钮和菜单项

### 3.2 DrawComponent 模板修改

**文件**：`Luck3DApp/Source/Panels/InspectorPanel.h`

```cpp
/// <summary>
/// 绘制组件
/// </summary>
/// <typeparam name="TComponent">组件类型</typeparam>
/// <typeparam name="UIFunction">组件功能函数类型</typeparam>
/// <param name="name">组件名</param>
/// <param name="entity">实体</param>
/// <param name="OnOpened">组件打开时调用</param>
/// <param name="removable">是否可以删除该组件</param>
template<typename TComponent, typename UIFunction>
void DrawComponent(const std::string& name, Entity entity, UIFunction OnOpened, bool removable = true);
```

**模板实现**：

```cpp
template<typename TComponent, typename UIFunction>
void InspectorPanel::DrawComponent(const std::string& name, Entity entity, UIFunction OnOpened, bool removable)
{
    if (entity.HasComponent<TComponent>())
    {
        const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap 
                                       | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
        
        auto& component = entity.GetComponent<TComponent>();
        ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail();
        
        // 使用组件类型哈希值作为 TreeNode ID，避免名称变化导致状态丢失
        bool opened = ImGui::TreeNodeEx((void*)typeid(TComponent).hash_code(), flags, name.c_str());
        
        // 仅可删除的组件显示设置按钮
        bool componentRemoved = false;
        if (removable)
        {
            ImGui::SameLine(contentRegionAvail.x - 18);
            
            // 使用组件类型哈希值作为 Popup ID，避免多个组件共用同一个 Popup
            std::string popupID = std::format("ComponentSettings##{}", typeid(TComponent).hash_code());
            
            if (ImGui::Button(std::format("+##{}", typeid(TComponent).hash_code()).c_str(), { 30, 30 }))
            {
                ImGui::OpenPopup(popupID.c_str());
            }

            if (ImGui::BeginPopup(popupID.c_str()))
            {
                if (ImGui::MenuItem("Remove Component"))
                {
                    componentRemoved = true;
                }
                ImGui::EndPopup();
            }
        }
        
        if (opened)
        {
            OnOpened(component);
            ImGui::TreePop();
        }

        if (componentRemoved)
        {
            entity.RemoveComponent<TComponent>();
        }
    }
}
```

### 3.3 AddComponent 按钮实现

**文件**：`Luck3DApp/Source/Panels/InspectorPanel.cpp`

在 `DrawComponents` 方法末尾添加：

```cpp
void InspectorPanel::DrawComponents(Entity entity)
{
    // ... 现有的 Name 组件、DrawComponent 调用 ...
    
    // ---- 材质编辑器 ----
    if (entity.HasComponent<MeshRendererComponent>())
    {
        MeshRendererComponent& meshRenderer = entity.GetComponent<MeshRendererComponent>();
        for (Ref<Material>& material : meshRenderer.Materials)
        {
            DrawMaterialEditor(material);
        }
    }
    
    // ========== Add Component 按钮 ==========
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // 居中显示按钮
    float buttonWidth = 250.0f;
    float availWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((availWidth - buttonWidth) * 0.5f);
    
    if (ImGui::Button("Add Component", ImVec2(buttonWidth, 30)))
    {
        ImGui::OpenPopup("AddComponentPopup");
    }
    
    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        // ---- 渲染 ----
        if (ImGui::BeginMenu("Rendering"))
        {
            if (!entity.HasComponent<MeshFilterComponent>())
            {
                if (ImGui::MenuItem("Mesh Filter"))
                {
                    entity.AddComponent<MeshFilterComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }
            
            if (!entity.HasComponent<MeshRendererComponent>())
            {
                if (ImGui::MenuItem("Mesh Renderer"))
                {
                    entity.AddComponent<MeshRendererComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }
            
            ImGui::EndMenu();
        }
        
        // ---- 光照 ----
        if (ImGui::BeginMenu("Light"))
        {
            if (!entity.HasComponent<DirectionalLightComponent>())
            {
                if (ImGui::MenuItem("Directional Light"))
                {
                    entity.AddComponent<DirectionalLightComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::EndPopup();
    }
}
```

**DrawComponent 调用修改**（核心组件传 `false`）：

```cpp
// Transform 组件 —— 不可删除
DrawComponent<TransformComponent>("Transform", entity, [](TransformComponent& transform)
{
    // ... 现有 UI 代码不变 ...
}, false);  // removable = false

// DirectionalLight 组件 —— 可删除（默认 true）
DrawComponent<DirectionalLightComponent>("Directional Light", entity, [](DirectionalLightComponent& light)
{
    // ... 现有 UI 代码不变 ...
});

// MeshFilter 组件 —— 可删除
DrawComponent<MeshFilterComponent>(meshName + " (Mesh Filter)", entity, [](MeshFilterComponent& meshFilter)
{
    // ... 现有 UI 代码不变 ...
});

// MeshRenderer 组件 —— 可删除
DrawComponent<MeshRendererComponent>("Mesh Renderer", entity, [&entity](MeshRendererComponent& meshRenderer)
{
    // ... 现有 UI 代码不变 ...
});
```

### 3.4 完整代码修改清单

| 文件 | 修改类型 | 说明 |
|------|---------|------|
| `Luck3DApp/Source/Panels/InspectorPanel.h` | 修改 | `DrawComponent` 模板增加 `removable` 参数，修改模板实现 |
| `Luck3DApp/Source/Panels/InspectorPanel.cpp` | 修改 | Transform 调用传 `false`；末尾添加 AddComponent 按钮 |

**总改动量**：约 60 行代码

### 3.5 优缺点分析

**优点**：
- ? **改动量最小**：仅修改 2 个文件，约 60 行代码
- ? **与现有代码风格完全一致**：保持模板 + lambda 的模式
- ? **编译期类型安全**：所有组件操作在编译期确定
- ? **零运行时开销**：不引入 `std::function` 或虚函数
- ? **10 分钟内可完成**

**缺点**：
- ? **新增组件需要修改多处**：每新增一个组件类型，需要在以下位置手动添加代码：
  1. `DrawComponents` 中添加 `DrawComponent<NewComponent>(...)` 调用
  2. `AddComponentPopup` 中添加 `MenuItem`
  3. `SceneHierarchyPanel::DrawEntityCreateMenu` 中添加创建菜单项（如需要）
  4. `SceneSerializer` 中添加序列化/反序列化代码
- ? **组件元数据分散**：是否可删除的信息隐藏在 `DrawComponent` 调用的参数中，不易维护
- ? **AddComponent 列表硬编码**：无法自动生成，容易遗漏

---

## 四、方案二：组件描述符注册表（推荐）

### 4.1 设计思路

引入 **组件描述符（ComponentDescriptor）** 结构体和 **组件注册表（ComponentRegistry）** 类，集中管理所有组件的元数据和操作。

核心理念：
- 每个组件类型对应一个 `ComponentDescriptor`，描述其名称、分类、约束规则和操作函数
- `ComponentRegistry` 是一个全局单例，持有所有已注册的描述符
- Inspector 和 Hierarchy 面板通过查询 `ComponentRegistry` 来动态生成 UI
- 新增组件只需在 `ComponentRegistry::Initialize()` 中添加一个注册调用

**架构图**：

```
┌─────────────────────────────────────────────────────────────────┐
│                      ComponentRegistry                          │
│  (全局单例，持有所有 ComponentDescriptor)                         │
│                                                                 │
│  Initialize()          ── 注册所有组件描述符                      │
│  GetAllDescriptors()   ── 返回所有描述符                          │
│  GetAddableDescriptors() ── 返回可添加的描述符                    │
│  GetCategorizedAddableDescriptors() ── 按分类返回可添加的描述符    │
└─────────────────────────────────────────────────────────────────┘
         │                          │
         │ 查询                     │ 查询
         ▼                          ▼
┌─────────────────┐      ┌──────────────────────┐
│  InspectorPanel │      │ SceneHierarchyPanel  │
│                 │      │                      │
│  遍历描述符绘制  │      │  遍历描述符生成       │
│  组件 UI        │      │  创建菜单            │
└─────────────────┘      └──────────────────────┘
```

### 4.2 核心数据结构

#### 4.2.1 ComponentDescriptor 结构体

**文件路径**：`Lucky/Source/Lucky/Scene/ComponentRegistry.h`

```cpp
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Lucky
{
    class Entity;

    /// <summary>
    /// 组件描述符：描述一个组件类型的元数据和操作接口
    /// </summary>
    struct ComponentDescriptor
    {
        /// 组件显示名称（如 "Transform"、"Mesh Filter"）
        std::string Name;
        
        /// 组件分类（如 "Core"、"Rendering"、"Light"）
        /// 用于 AddComponent 弹出菜单的分类显示
        std::string Category;
        
        /// 是否可以通过 Inspector 删除
        /// false 的组件不显示设置按钮和删除菜单
        /// 例如：TransformComponent、RelationshipComponent
        bool IsRemovable = true;
        
        /// 是否可以通过 AddComponent 按钮添加
        /// false 的组件不出现在 AddComponent 列表中
        /// 例如：IDComponent、NameComponent、TransformComponent、RelationshipComponent
        bool IsAddable = true;
        
        /// 是否在 Inspector 中隐藏
        /// true 的组件完全不在 Inspector 中显示
        /// 例如：IDComponent、RelationshipComponent
        bool IsHiddenInInspector = true;
        
        /// 排序权重（数值越小越靠前）
        /// 用于控制组件在 Inspector 中的显示顺序
        int SortOrder = 100;
        
        // ---- 操作函数 ----
        
        /// 检查实体是否拥有该组件
        std::function<bool(Entity)> HasComponent;
        
        /// 向实体添加该组件（使用默认参数）
        std::function<void(Entity)> AddComponent;
        
        /// 从实体移除该组件
        std::function<void(Entity)> RemoveComponent;
        
        /// 绘制该组件的 Inspector UI
        /// 参数：entity —— 当前实体
        /// 该函数负责绘制组件的所有可编辑属性
        std::function<void(Entity)> DrawInspector;
    };
}
```

**字段设计说明**：

| 字段 | 类型 | 说明 | 默认值 |
|------|------|------|--------|
| `Name` | `string` | Inspector 中显示的组件名称 | 必填 |
| `Category` | `string` | AddComponent 菜单中的分类名 | 必填 |
| `IsRemovable` | `bool` | 是否允许用户删除 | `true` |
| `IsAddable` | `bool` | 是否出现在 AddComponent 列表 | `true` |
| `IsHiddenInInspector` | `bool` | 是否在 Inspector 中完全隐藏 | `true` |
| `SortOrder` | `int` | 显示排序权重 | `100` |
| `HasComponent` | `function` | 运行时检查实体是否拥有该组件 | 必填 |
| `AddComponent` | `function` | 运行时向实体添加该组件 | 必填 |
| `RemoveComponent` | `function` | 运行时从实体移除该组件 | 必填 |
| `DrawInspector` | `function` | 运行时绘制该组件的 Inspector UI | 必填 |

### 4.3 ComponentRegistry 类设计

#### 4.3.1 头文件

**文件路径**：`Lucky/Source/Lucky/Scene/ComponentRegistry.h`（追加到 ComponentDescriptor 之后）

```cpp
/// <summary>
/// 组件注册表：管理所有已注册的组件描述符
/// 全局单例，在应用启动时调用 Initialize() 注册所有组件
/// </summary>
class ComponentRegistry
{
public:
    /// <summary>
    /// 初始化：注册所有组件描述符
    /// 应在应用启动时调用一次（如 EditorLayer::OnAttach）
    /// </summary>
    static void Initialize();
    
    /// <summary>
    /// 返回所有已注册的组件描述符
    /// </summary>
    static const std::vector<ComponentDescriptor>& GetAllDescriptors();
    
    /// <summary>
    /// 返回所有可添加的组件描述符（IsAddable == true）
    /// </summary>
    static std::vector<const ComponentDescriptor*> GetAddableDescriptors();
    
    /// <summary>
    /// 返回按分类组织的可添加组件描述符
    /// key: 分类名（如 "Rendering"）
    /// value: 该分类下的描述符指针列表
    /// </summary>
    static std::unordered_map<std::string, std::vector<const ComponentDescriptor*>> GetCategorizedAddableDescriptors();

private:
    static std::vector<ComponentDescriptor> s_Descriptors;
};
```

#### 4.3.2 实现文件

**文件路径**：`Lucky/Source/Lucky/Scene/ComponentRegistry.cpp`

```cpp
#include "lcpch.h"
#include "ComponentRegistry.h"

#include "Entity.h"

// 组件头文件
#include "Components/IDComponent.h"
#include "Components/NameComponent.h"
#include "Components/TransformComponent.h"
#include "Components/RelationshipComponent.h"
#include "Components/MeshFilterComponent.h"
#include "Components/MeshRendererComponent.h"
#include "Components/DirectionalLightComponent.h"

#include "imgui/imgui.h"

namespace Lucky
{
    std::vector<ComponentDescriptor> ComponentRegistry::s_Descriptors;

    void ComponentRegistry::Initialize()
    {
        s_Descriptors.clear();
        
        // ============================================================
        // 系统内部组件（隐藏，不可删除，不可添加）
        // ============================================================
        
        // IDComponent —— 系统内部，完全隐藏
        {
            ComponentDescriptor desc;
            desc.Name = "ID";
            desc.Category = "Internal";
            desc.IsRemovable = false;
            desc.IsAddable = false;
            desc.IsHiddenInInspector = true;
            desc.SortOrder = 0;
            desc.HasComponent = [](Entity e) { return e.HasComponent<IDComponent>(); };
            desc.AddComponent = [](Entity e) { /* 不允许手动添加 */ };
            desc.RemoveComponent = [](Entity e) { /* 不允许删除 */ };
            desc.DrawInspector = [](Entity e) { /* 隐藏，不绘制 */ };
            s_Descriptors.push_back(desc);
        }
        
        // NameComponent —— 系统内部，Inspector 中特殊处理（顶部输入框），不通过描述符绘制
        {
            ComponentDescriptor desc;
            desc.Name = "Name";
            desc.Category = "Internal";
            desc.IsRemovable = false;
            desc.IsAddable = false;
            desc.IsHiddenInInspector = true;  // 通过 DrawComponents 中的特殊代码绘制，不走描述符
            desc.SortOrder = 1;
            desc.HasComponent = [](Entity e) { return e.HasComponent<NameComponent>(); };
            desc.AddComponent = [](Entity e) { /* 不允许手动添加 */ };
            desc.RemoveComponent = [](Entity e) { /* 不允许删除 */ };
            desc.DrawInspector = [](Entity e) { /* 特殊处理 */ };
            s_Descriptors.push_back(desc);
        }
        
        // RelationshipComponent —— 系统内部，隐藏
        {
            ComponentDescriptor desc;
            desc.Name = "Relationship";
            desc.Category = "Internal";
            desc.IsRemovable = false;
            desc.IsAddable = false;
            desc.IsHiddenInInspector = true;
            desc.SortOrder = 2;
            desc.HasComponent = [](Entity e) { return e.HasComponent<RelationshipComponent>(); };
            desc.AddComponent = [](Entity e) { /* 不允许手动添加 */ };
            desc.RemoveComponent = [](Entity e) { /* 不允许删除 */ };
            desc.DrawInspector = [](Entity e) { /* 隐藏，不绘制 */ };
            s_Descriptors.push_back(desc);
        }
        
        // ============================================================
        // 核心组件（可见，不可删除，不可添加）
        // ============================================================
        
        // TransformComponent —— 核心组件，可见但不可删除、不可添加
        {
            ComponentDescriptor desc;
            desc.Name = "Transform";
            desc.Category = "Core";
            desc.IsRemovable = false;
            desc.IsAddable = false;
            desc.IsHiddenInInspector = false;
            desc.SortOrder = 10;
            desc.HasComponent = [](Entity e) { return e.HasComponent<TransformComponent>(); };
            desc.AddComponent = [](Entity e) { /* 不允许手动添加 */ };
            desc.RemoveComponent = [](Entity e) { /* 不允许删除 */ };
            desc.DrawInspector = [](Entity e)
            {
                auto& transform = e.GetComponent<TransformComponent>();
                
                ImGui::DragFloat3("Position", &transform.Translation.x, 0.1f);
                
                glm::vec3 rotationEuler = transform.GetRotationEuler();
                if (ImGui::DragFloat3("Rotation", &rotationEuler.x, 0.1f))
                {
                    transform.SetRotationEuler(rotationEuler);
                }
                
                ImGui::DragFloat3("Scale", &transform.Scale.x, 0.1f);
            };
            s_Descriptors.push_back(desc);
        }
        
        // ============================================================
        // 渲染组件（可见，可删除，可添加）
        // ============================================================
        
        // MeshFilterComponent
        {
            ComponentDescriptor desc;
            desc.Name = "Mesh Filter";
            desc.Category = "Rendering";
            desc.IsRemovable = true;
            desc.IsAddable = true;
            desc.IsHiddenInInspector = false;
            desc.SortOrder = 20;
            desc.HasComponent = [](Entity e) { return e.HasComponent<MeshFilterComponent>(); };
            desc.AddComponent = [](Entity e) { e.AddComponent<MeshFilterComponent>(); };
            desc.RemoveComponent = [](Entity e) { e.RemoveComponent<MeshFilterComponent>(); };
            desc.DrawInspector = [](Entity e)
            {
                auto& meshFilter = e.GetComponent<MeshFilterComponent>();
                
                std::string meshName = meshFilter.Mesh ? meshFilter.Mesh->GetName() : "None";
                
                char buffer[256];
                memset(buffer, 0, sizeof(buffer));
                strcpy_s(buffer, sizeof(buffer), meshName.c_str());
                
                ImGui::InputText("Mesh", buffer, sizeof(buffer));
            };
            s_Descriptors.push_back(desc);
        }
        
        // MeshRendererComponent
        {
            ComponentDescriptor desc;
            desc.Name = "Mesh Renderer";
            desc.Category = "Rendering";
            desc.IsRemovable = true;
            desc.IsAddable = true;
            desc.IsHiddenInInspector = false;
            desc.SortOrder = 21;
            desc.HasComponent = [](Entity e) { return e.HasComponent<MeshRendererComponent>(); };
            desc.AddComponent = [](Entity e) { e.AddComponent<MeshRendererComponent>(); };
            desc.RemoveComponent = [](Entity e) { e.RemoveComponent<MeshRendererComponent>(); };
            desc.DrawInspector = [](Entity e)
            {
                auto& meshRenderer = e.GetComponent<MeshRendererComponent>();
                
                const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;
                
                const std::string& strID = std::format("Materials##{0}", e.GetComponent<NameComponent>().Name);
                bool opened = ImGui::TreeNodeEx(strID.c_str(), flags);
                if (opened)
                {
                    // 材质数量
                    int materialSize = meshRenderer.Materials.size();
                    ImGui::InputInt("Size", &materialSize, 0, 0, ImGuiInputTextFlags_ReadOnly);
                    
                    // 材质列表
                    for (int i = 0; i < meshRenderer.Materials.size(); i++)
                    {
                        const std::string& label = std::format("Element {0}", i);
                        const std::string& materialName = meshRenderer.Materials[i] ? meshRenderer.Materials[i]->GetName() : "None";
                        
                        char buffer[256];
                        memset(buffer, 0, sizeof(buffer));
                        strcpy_s(buffer, sizeof(buffer), materialName.c_str());
                        
                        ImGui::InputText(label.c_str(), buffer, sizeof(buffer));
                    }
                    
                    ImGui::TreePop();
                }
            };
            s_Descriptors.push_back(desc);
        }
        
        // ============================================================
        // 光照组件（可见，可删除，可添加）
        // ============================================================
        
        // DirectionalLightComponent
        {
            ComponentDescriptor desc;
            desc.Name = "Directional Light";
            desc.Category = "Light";
            desc.IsRemovable = true;
            desc.IsAddable = true;
            desc.IsHiddenInInspector = false;
            desc.SortOrder = 30;
            desc.HasComponent = [](Entity e) { return e.HasComponent<DirectionalLightComponent>(); };
            desc.AddComponent = [](Entity e) { e.AddComponent<DirectionalLightComponent>(); };
            desc.RemoveComponent = [](Entity e) { e.RemoveComponent<DirectionalLightComponent>(); };
            desc.DrawInspector = [](Entity e)
            {
                auto& light = e.GetComponent<DirectionalLightComponent>();
                
                ImGui::ColorEdit3("Color", &light.Color.x);
                ImGui::DragFloat("Intensity", &light.Intensity, 0.01f, 0.0f, 10.0f);
            };
            s_Descriptors.push_back(desc);
        }
        
        // ============================================================
        // TODO: 未来新增组件在此处注册
        // ============================================================
        
        // 按 SortOrder 排序
        std::sort(s_Descriptors.begin(), s_Descriptors.end(), [](const ComponentDescriptor& a, const ComponentDescriptor& b)
        {
            return a.SortOrder < b.SortOrder;
        });
    }
    
    const std::vector<ComponentDescriptor>& ComponentRegistry::GetAllDescriptors()
    {
        return s_Descriptors;
    }
    
    std::vector<const ComponentDescriptor*> ComponentRegistry::GetAddableDescriptors()
    {
        std::vector<const ComponentDescriptor*> result;
        for (const auto& desc : s_Descriptors)
        {
            if (desc.IsAddable)
            {
                result.push_back(&desc);
            }
        }
        return result;
    }
    
    std::unordered_map<std::string, std::vector<const ComponentDescriptor*>> ComponentRegistry::GetCategorizedAddableDescriptors()
    {
        std::unordered_map<std::string, std::vector<const ComponentDescriptor*>> result;
        for (const auto& desc : s_Descriptors)
        {
            if (desc.IsAddable)
            {
                result[desc.Category].push_back(&desc);
            }
        }
        return result;
    }
}
```

### 4.4 组件注册实现

#### 4.4.1 注册辅助宏（可选）

如果觉得每次注册都写一大段代码太冗长，可以定义辅助函数模板简化注册：

```cpp
// ComponentRegistry.h 中添加（可选）

/// <summary>
/// 注册组件的辅助函数模板
/// </summary>
template<typename TComponent>
static ComponentDescriptor CreateDescriptor(
    const std::string& name,
    const std::string& category,
    bool removable,
    bool addable,
    bool hidden,
    int sortOrder,
    std::function<void(Entity)> drawInspector)
{
    ComponentDescriptor desc;
    desc.Name = name;
    desc.Category = category;
    desc.IsRemovable = removable;
    desc.IsAddable = addable;
    desc.IsHiddenInInspector = hidden;
    desc.SortOrder = sortOrder;
    desc.HasComponent = [](Entity e) { return e.HasComponent<TComponent>(); };
    desc.AddComponent = [](Entity e) { e.AddComponent<TComponent>(); };
    desc.RemoveComponent = [](Entity e) { e.RemoveComponent<TComponent>(); };
    desc.DrawInspector = drawInspector;
    return desc;
}
```

使用示例：

```cpp
s_Descriptors.push_back(CreateDescriptor<DirectionalLightComponent>(
    "Directional Light", "Light",
    true,   // removable
    true,   // addable
    false,  // hidden
    30,     // sortOrder
    [](Entity e)
    {
        auto& light = e.GetComponent<DirectionalLightComponent>();
        ImGui::ColorEdit3("Color", &light.Color.x);
        ImGui::DragFloat("Intensity", &light.Intensity, 0.01f, 0.0f, 10.0f);
    }
));
```

> **注意**：辅助函数是可选的，不影响功能。如果你更喜欢显式写出每个字段（更清晰），可以不使用。

### 4.5 InspectorPanel 重构

#### 4.5.1 InspectorPanel.h 修改

```cpp
#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Scene/Entity.h"
#include "Lucky/Renderer/Material.h"

#include "imgui/imgui.h"

namespace Lucky
{
    class InspectorPanel : public EditorPanel
    {
    public:
        InspectorPanel() = default;
        InspectorPanel(const Ref<Scene>& scene);
        ~InspectorPanel() override = default;
        
        void SetScene(const Ref<Scene>& scene);

        void OnUpdate(DeltaTime dt) override;
        
        void OnGUI() override;
        
        void DrawComponents(Entity entity);
        
        void OnEvent(Event& event) override;
    private:
        /// <summary>
        /// 绘制单个组件的 TreeNode 框架（标题栏 + 设置按钮 + 删除逻辑）
        /// 组件内部 UI 由 ComponentDescriptor::DrawInspector 绘制
        /// </summary>
        /// <param name="desc">组件描述符</param>
        /// <param name="entity">实体</param>
        void DrawComponentFromDescriptor(const struct ComponentDescriptor& desc, Entity entity);
        
        /// <summary>
        /// 绘制 Add Component 按钮和弹出菜单
        /// </summary>
        /// <param name="entity">实体</param>
        void DrawAddComponentButton(Entity entity);
        
        // TODO Move to MaterialEditor class
        void DrawMaterialEditor(Ref<Material>& material);
    private:
        Ref<Scene> m_Scene;
    };
}
```

> **注意**：原有的 `DrawComponent` 模板被移除，替换为 `DrawComponentFromDescriptor` 方法。

#### 4.5.2 InspectorPanel.cpp 修改

```cpp
#include "InspectorPanel.h"

#include "Lucky/Scene/SelectionManager.h"
#include "Lucky/Scene/ComponentRegistry.h"

#include "Lucky/Scene/Components/NameComponent.h"
#include "Lucky/Scene/Components/MeshRendererComponent.h"

#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Utils/PlatformUtils.h"

#include <format>

namespace Lucky
{
    InspectorPanel::InspectorPanel(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
        
    }

    void InspectorPanel::SetScene(const Ref<Scene>& scene)
    {
        m_Scene = scene;
    }

    void InspectorPanel::OnUpdate(DeltaTime dt)
    {
        
    }

    void InspectorPanel::OnGUI()
    {
        UUID selectionID = SelectionManager::GetSelection();
        if (selectionID != 0)
        {
            DrawComponents(m_Scene->GetEntityWithUUID(selectionID));
        }
    }

    void InspectorPanel::DrawComponents(Entity entity)
    {
        // ---- Name 组件（特殊处理，不走描述符系统） ----
        if (entity.HasComponent<NameComponent>())
        {
            std::string& name = entity.GetComponent<NameComponent>().Name;

            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            strcpy_s(buffer, sizeof(buffer), name.c_str());
            
            if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
            {
                name = std::string(buffer);
            }
        }
        
        // ---- 遍历所有组件描述符，绘制可见的组件 ----
        for (const auto& desc : ComponentRegistry::GetAllDescriptors())
        {
            // 跳过隐藏的组件
            if (desc.IsHiddenInInspector) continue;
            
            // 跳过实体没有的组件
            if (!desc.HasComponent(entity)) continue;
            
            // 绘制组件
            DrawComponentFromDescriptor(desc, entity);
        }
        
        // ---- 材质编辑器（MeshRenderer 的特殊附加 UI） ----
        if (entity.HasComponent<MeshRendererComponent>())
        {
            MeshRendererComponent& meshRenderer = entity.GetComponent<MeshRendererComponent>();
            for (Ref<Material>& material : meshRenderer.Materials)
            {
                DrawMaterialEditor(material);
            }
        }
        
        // ---- Add Component 按钮 ----
        DrawAddComponentButton(entity);
    }
    
    void InspectorPanel::DrawComponentFromDescriptor(const ComponentDescriptor& desc, Entity entity)
    {
        // 树节点标志
        const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap 
                                       | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
        
        ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail();
        
        // 使用组件名称的哈希值作为 TreeNode ID
        ImGuiID nodeID = ImGui::GetID(desc.Name.c_str());
        bool opened = ImGui::TreeNodeEx((void*)(intptr_t)nodeID, flags, desc.Name.c_str());
        
        // 仅可删除的组件显示设置按钮
        bool componentRemoved = false;
        if (desc.IsRemovable)
        {
            ImGui::SameLine(contentRegionAvail.x - 18);
            
            // 每个组件使用独立的 Popup ID
            std::string popupID = std::format("ComponentSettings##{}", desc.Name);
            std::string buttonID = std::format("+##{}", desc.Name);
            
            if (ImGui::Button(buttonID.c_str(), { 30, 30 }))
            {
                ImGui::OpenPopup(popupID.c_str());
            }

            if (ImGui::BeginPopup(popupID.c_str()))
            {
                if (ImGui::MenuItem("Remove Component"))
                {
                    componentRemoved = true;
                }
                ImGui::EndPopup();
            }
        }
        
        if (opened)
        {
            desc.DrawInspector(entity);  // 调用描述符中的绘制函数
            ImGui::TreePop();
        }

        if (componentRemoved)
        {
            desc.RemoveComponent(entity);  // 调用描述符中的移除函数
        }
    }
    
    void InspectorPanel::DrawAddComponentButton(Entity entity)
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // 居中显示按钮
        float buttonWidth = 250.0f;
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((availWidth - buttonWidth) * 0.5f);
        
        if (ImGui::Button("Add Component", ImVec2(buttonWidth, 30)))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }
        
        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            // 按分类组织的可添加组件
            auto categorized = ComponentRegistry::GetCategorizedAddableDescriptors();
            
            for (auto& [category, descriptors] : categorized)
            {
                if (ImGui::BeginMenu(category.c_str()))
                {
                    for (const auto* desc : descriptors)
                    {
                        // 已存在的组件显示为灰色（不可点击）
                        bool alreadyHas = desc->HasComponent(entity);
                        
                        if (alreadyHas)
                        {
                            ImGui::BeginDisabled();
                        }
                        
                        if (ImGui::MenuItem(desc->Name.c_str()))
                        {
                            desc->AddComponent(entity);
                            ImGui::CloseCurrentPopup();
                        }
                        
                        if (alreadyHas)
                        {
                            ImGui::EndDisabled();
                        }
                    }
                    ImGui::EndMenu();
                }
            }
            
            ImGui::EndPopup();
        }
    }

    void InspectorPanel::OnEvent(Event& event)
    {
        
    }

    void InspectorPanel::DrawMaterialEditor(Ref<Material>& material)
    {
        // ... 现有材质编辑器代码保持不变 ...
    }
}
```

### 4.6 SceneHierarchyPanel 集成

SceneHierarchyPanel 的 `DrawEntityCreateMenu` 目前是硬编码的创建菜单。方案二**不强制要求**修改 SceneHierarchyPanel，因为创建菜单的逻辑（创建实体 + 添加特定组件组合）比单纯的 AddComponent 更复杂（例如创建 Cube 需要同时添加 MeshFilter + MeshRenderer + 设置默认材质）。

**建议**：SceneHierarchyPanel 的创建菜单保持现有硬编码方式，因为它描述的是"预设实体模板"而非"单个组件添加"。未来可以引入 Prefab 系统来统一管理。

### 4.7 序列化系统影响分析

方案二**不影响**现有的序列化系统。原因：

1. `ComponentRegistry` 是纯 UI 层的概念，不参与数据持久化
2. `SceneSerializer` 仍然通过 `entity.HasComponent<T>()` 和 `entity.GetComponent<T>()` 直接操作组件
3. 新增组件时，仍然需要在 `SceneSerializer` 中手动添加序列化/反序列化代码

> **未来优化**：可以在 `ComponentDescriptor` 中添加 `Serialize` 和 `Deserialize` 函数指针，让序列化也走注册表。但这需要重构 SceneSerializer，当前阶段不建议。

### 4.8 完整代码修改清单

#### 4.8.1 新增文件

| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Scene/ComponentRegistry.h` | ComponentDescriptor 结构体 + ComponentRegistry 类声明 |
| `Lucky/Source/Lucky/Scene/ComponentRegistry.cpp` | ComponentRegistry::Initialize() 实现 + 所有组件注册 |

#### 4.8.2 修改文件

| 文件路径 | 修改内容 |
|---------|---------|
| `Luck3DApp/Source/Panels/InspectorPanel.h` | 移除 `DrawComponent` 模板，新增 `DrawComponentFromDescriptor` 和 `DrawAddComponentButton` 方法 |
| `Luck3DApp/Source/Panels/InspectorPanel.cpp` | 重写 `DrawComponents`，使用描述符遍历替代硬编码调用 |
| `Luck3DApp/Source/EditorLayer.cpp` | 在 `OnAttach` 中调用 `ComponentRegistry::Initialize()` |

#### 4.8.3 EditorLayer.cpp 修改

在 `EditorLayer::OnAttach()` 的开头添加初始化调用：

```cpp
#include "Lucky/Scene/ComponentRegistry.h"

void EditorLayer::OnAttach()
{
    LF_TRACE("EditorLayer::OnAttach");
    
    // 初始化组件注册表
    ComponentRegistry::Initialize();

    m_Scene = CreateRef<Scene>("New Scene");
    
    // ... 后续代码不变 ...
}
```

#### 4.8.4 文件变更总览

```
新增文件：
  Lucky/Source/Lucky/Scene/ComponentRegistry.h      (~80 行)
  Lucky/Source/Lucky/Scene/ComponentRegistry.cpp     (~250 行)

修改文件：
  Luck3DApp/Source/Panels/InspectorPanel.h          (重写，~50 行)
  Luck3DApp/Source/Panels/InspectorPanel.cpp        (重写 DrawComponents，~180 行；DrawMaterialEditor 不变)
  Luck3DApp/Source/EditorLayer.cpp                  (+3 行)

总改动量：约 560 行代码
```

### 4.9 优缺点分析

**优点**：
- ? **集中管理**：所有组件的元数据在 `ComponentRegistry::Initialize()` 一处定义
- ? **新增组件成本低**：只需在 `Initialize()` 中添加一个描述符注册块，Inspector 和 AddComponent 菜单自动更新
- ? **AddComponent 列表自动生成**：不需要手动维护 if-else 链
- ? **分类显示**：AddComponent 弹出菜单按 Category 自动分组
- ? **排序可控**：通过 SortOrder 控制组件在 Inspector 中的显示顺序
- ? **接近 Unity 设计理念**：描述符类似 Unity 的 `[AddComponentMenu]` + `[DisallowMultipleComponent]` 注解
- ? **可扩展性强**：未来可以轻松添加字段（图标、依赖关系、序列化函数等）

**缺点**：
- ? **引入运行时多态**：`std::function` 有少量运行时开销（但对 UI 代码来说可忽略不计）
- ? **丢失编译期类型安全**：组件操作通过 `std::function` 间接调用，类型错误只能在运行时发现
- ? **DrawInspector lambda 较长**：复杂组件（如 MeshRenderer）的绘制代码写在 lambda 中，可读性略差
- ? **需要一次性重构**：现有的 `DrawComponent` 模板调用全部需要迁移到描述符系统
- ? **改动量较大**：约 560 行代码，需要仔细测试

**缓解措施**：
- 对于 DrawInspector lambda 过长的问题，可以将绘制逻辑提取为独立的静态函数，lambda 中只做转发调用
- 对于类型安全问题，注册时的 lambda 捕获了具体的组件类型，实际上是类型安全的

---

## 五、方案三：模板 Traits 编译期约束方案

### 5.1 设计思路

利用 C++ 模板特化（Template Specialization）在编译期定义每个组件的约束属性。不引入运行时多态，所有约束在编译期确定。

### 5.2 ComponentTraits 定义

**文件路径**：`Lucky/Source/Lucky/Scene/ComponentTraits.h`

```cpp
#pragma once

namespace Lucky
{
    // 前向声明所有组件类型
    struct IDComponent;
    struct NameComponent;
    struct TransformComponent;
    struct RelationshipComponent;
    struct MeshFilterComponent;
    struct MeshRendererComponent;
    struct DirectionalLightComponent;

    /// <summary>
    /// 组件 Traits 基模板：定义组件的编译期属性默认值
    /// 每个组件类型通过模板特化来覆盖默认值
    /// </summary>
    template<typename T>
    struct ComponentTraits
    {
        static constexpr const char* Name = "Unknown Component";
        static constexpr const char* Category = "Misc";
        static constexpr bool IsRemovable = true;
        static constexpr bool IsAddable = true;
        static constexpr bool IsHiddenInInspector = false;
        static constexpr int SortOrder = 100;
    };

    // ============================================================
    // 系统内部组件
    // ============================================================

    template<>
    struct ComponentTraits<IDComponent>
    {
        static constexpr const char* Name = "ID";
        static constexpr const char* Category = "Internal";
        static constexpr bool IsRemovable = false;
        static constexpr bool IsAddable = false;
        static constexpr bool IsHiddenInInspector = true;
        static constexpr int SortOrder = 0;
    };

    template<>
    struct ComponentTraits<NameComponent>
    {
        static constexpr const char* Name = "Name";
        static constexpr const char* Category = "Internal";
        static constexpr bool IsRemovable = false;
        static constexpr bool IsAddable = false;
        static constexpr bool IsHiddenInInspector = true;
        static constexpr int SortOrder = 1;
    };

    template<>
    struct ComponentTraits<RelationshipComponent>
    {
        static constexpr const char* Name = "Relationship";
        static constexpr const char* Category = "Internal";
        static constexpr bool IsRemovable = false;
        static constexpr bool IsAddable = false;
        static constexpr bool IsHiddenInInspector = true;
        static constexpr int SortOrder = 2;
    };

    // ============================================================
    // 核心组件
    // ============================================================

    template<>
    struct ComponentTraits<TransformComponent>
    {
        static constexpr const char* Name = "Transform";
        static constexpr const char* Category = "Core";
        static constexpr bool IsRemovable = false;
        static constexpr bool IsAddable = false;
        static constexpr bool IsHiddenInInspector = false;
        static constexpr int SortOrder = 10;
    };

    // ============================================================
    // 渲染组件
    // ============================================================

    template<>
    struct ComponentTraits<MeshFilterComponent>
    {
        static constexpr const char* Name = "Mesh Filter";
        static constexpr const char* Category = "Rendering";
        static constexpr bool IsRemovable = true;
        static constexpr bool IsAddable = true;
        static constexpr bool IsHiddenInInspector = false;
        static constexpr int SortOrder = 20;
    };

    template<>
    struct ComponentTraits<MeshRendererComponent>
    {
        static constexpr const char* Name = "Mesh Renderer";
        static constexpr const char* Category = "Rendering";
        static constexpr bool IsRemovable = true;
        static constexpr bool IsAddable = true;
        static constexpr bool IsHiddenInInspector = false;
        static constexpr int SortOrder = 21;
    };

    // ============================================================
    // 光照组件
    // ============================================================

    template<>
    struct ComponentTraits<DirectionalLightComponent>
    {
        static constexpr const char* Name = "Directional Light";
        static constexpr const char* Category = "Light";
        static constexpr bool IsRemovable = true;
        static constexpr bool IsAddable = true;
        static constexpr bool IsHiddenInInspector = false;
        static constexpr int SortOrder = 30;
    };
}
```

### 5.3 DrawComponent 模板修改

**文件**：`Luck3DApp/Source/Panels/InspectorPanel.h`

```cpp
template<typename TComponent, typename UIFunction>
void InspectorPanel::DrawComponent(Entity entity, UIFunction OnOpened)
{
    using Traits = ComponentTraits<TComponent>;
    
    // 隐藏的组件不绘制
    if constexpr (Traits::IsHiddenInInspector)
    {
        return;
    }
    
    if (!entity.HasComponent<TComponent>())
    {
        return;
    }
    
    const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap 
                                   | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    auto& component = entity.GetComponent<TComponent>();
    ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail();
    
    bool opened = ImGui::TreeNodeEx((void*)typeid(TComponent).hash_code(), flags, Traits::Name);
    
    bool componentRemoved = false;
    
    // 编译期决定是否显示删除按钮
    if constexpr (Traits::IsRemovable)
    {
        ImGui::SameLine(contentRegionAvail.x - 18);
        
        std::string popupID = std::format("ComponentSettings##{}", typeid(TComponent).hash_code());
        
        if (ImGui::Button(std::format("+##{}", typeid(TComponent).hash_code()).c_str(), { 30, 30 }))
        {
            ImGui::OpenPopup(popupID.c_str());
        }

        if (ImGui::BeginPopup(popupID.c_str()))
        {
            if (ImGui::MenuItem("Remove Component"))
            {
                componentRemoved = true;
            }
            ImGui::EndPopup();
        }
    }
    
    if (opened)
    {
        OnOpened(component);
        ImGui::TreePop();
    }

    if constexpr (Traits::IsRemovable)
    {
        if (componentRemoved)
        {
            entity.RemoveComponent<TComponent>();
        }
    }
}
```

**调用方式变化**：

```cpp
// 之前：
DrawComponent<TransformComponent>("Transform", entity, [](TransformComponent& t) { ... }, false);

// 之后（名称从 Traits 自动获取，removable 从 Traits 编译期确定）：
DrawComponent<TransformComponent>(entity, [](TransformComponent& t) { ... });
```

### 5.4 AddComponent 列表维护

**关键问题**：C++ 模板特化无法在运行时枚举所有已特化的类型。因此 AddComponent 列表仍然需要手动维护。

```cpp
void InspectorPanel::DrawAddComponentButton(Entity entity)
{
    // ... 按钮代码 ...
    
    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        // 必须手动列出所有可添加的组件
        // 无法从 ComponentTraits 自动生成
        
        if (ImGui::BeginMenu("Rendering"))
        {
            DrawAddComponentMenuItem<MeshFilterComponent>(entity);
            DrawAddComponentMenuItem<MeshRendererComponent>(entity);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Light"))
        {
            DrawAddComponentMenuItem<DirectionalLightComponent>(entity);
            ImGui::EndMenu();
        }
        
        ImGui::EndPopup();
    }
}

// 辅助模板
template<typename TComponent>
void InspectorPanel::DrawAddComponentMenuItem(Entity entity)
{
    using Traits = ComponentTraits<TComponent>;
    
    if constexpr (!Traits::IsAddable)
    {
        return;  // 编译期排除不可添加的组件
    }
    
    bool alreadyHas = entity.HasComponent<TComponent>();
    
    if (alreadyHas)
    {
        ImGui::BeginDisabled();
    }
    
    if (ImGui::MenuItem(Traits::Name))
    {
        entity.AddComponent<TComponent>();
        ImGui::CloseCurrentPopup();
    }
    
    if (alreadyHas)
    {
        ImGui::EndDisabled();
    }
}
```

### 5.5 完整代码修改清单

| 文件路径 | 修改类型 | 说明 |
|---------|---------|------|
| `Lucky/Source/Lucky/Scene/ComponentTraits.h` | 新增 | 所有组件的 Traits 特化 |
| `Luck3DApp/Source/Panels/InspectorPanel.h` | 修改 | `DrawComponent` 模板签名变化，新增 `DrawAddComponentMenuItem` 模板 |
| `Luck3DApp/Source/Panels/InspectorPanel.cpp` | 修改 | 调用方式变化，末尾添加 AddComponent 按钮 |

**总改动量**：约 300 行代码

### 5.6 优缺点分析

**优点**：
- ? **编译期类型安全**：所有约束在编译期确定，类型错误编译时报错
- ? **零运行时开销**：`if constexpr` 在编译期消除不需要的分支
- ? **约束与组件类型绑定**：通过 Traits 特化，约束信息与组件类型紧密关联
- ? **不引入 std::function**：保持纯模板风格

**缺点**：
- ? **AddComponent 列表无法自动生成**：C++ 无法在运行时枚举所有模板特化，必须手动维护
- ? **新增组件仍需修改多处**：
  1. 添加 Traits 特化
  2. 在 `DrawComponents` 中添加 `DrawComponent<NewComponent>(entity, ...)` 调用
  3. 在 `DrawAddComponentButton` 中添加 `DrawAddComponentMenuItem<NewComponent>(entity)` 调用
- ? **模板代码膨胀**：每个组件的 Traits 特化 + DrawComponent 实例化会增加编译时间
- ? **灵活性不足**：无法在运行时动态查询"有哪些组件可以添加"

---

## 六、三方案综合对比

### 6.1 对比表

| 维度 | 方案一（硬编码） | 方案二（描述符注册表） | 方案三（模板 Traits） |
|------|:---:|:---:|:---:|
| **实现复杂度** | ? 低（~60 行） | ?? 中（~560 行） | ?? 中（~300 行） |
| **新增组件成本** | ?? 多处修改（4 处） | ?? 一处注册 | ?? 两处修改（Traits + 手动列表） |
| **编译期类型安全** | ? 完全 | ? 运行时 | ? 完全 |
| **AddComponent 自动化** | ? 手动维护 | ? 自动生成 | ? 手动维护 |
| **运行时开销** | 零 | 极小（std::function） | 零 |
| **组件分类显示** | 需手动组织 | ? 自动按 Category 分组 | 需手动组织 |
| **组件排序控制** | 由代码顺序决定 | ? SortOrder 字段 | 由代码顺序决定 |
| **可扩展性** | 一般 | 优秀 | 良好 |
| **与现有代码兼容** | 最好（最小改动） | 需重构 Inspector | 中等 |
| **Unity 相似度** | 低 | 高 | 中 |
| **Popup ID 冲突修复** | 需手动处理 | ? 自动处理 | 需手动处理 |
| **未来扩展（依赖、图标等）** | 困难 | ? 添加字段即可 | 添加 Traits 字段 |

### 6.2 推荐优先级

| 优先级 | 方案 | 适用场景 | 推荐理由 |
|:---:|------|---------|---------|
| ?? **第一推荐** | **方案二：组件描述符注册表** | 组件数量 ≥ 5 且持续增长的项目 | 一步到位，新增组件成本最低，AddComponent 列表自动生成，最接近 Unity 设计 |
| ?? **第二推荐** | **方案一：轻量级硬编码** | 快速原型验证，或组件数量 < 5 的小项目 | 改动量最小，10 分钟完成，适合先验证功能再重构 |
| ?? **第三推荐** | **方案三：模板 Traits** | 对编译期类型安全有极高要求的项目 | 编译期安全但 AddComponent 列表无法自动化，实际收益不如方案二 |

> **本项目推荐方案二**。当前已有 7 个组件类型，且随着引擎开发推进（点光源、聚光灯、相机、刚体、碰撞体等），组件数量会快速增长。方案二的一次性投入（~560 行）可以显著降低后续每个新组件的集成成本。

---

## 七、方案二详细实现指南

### 7.1 新增文件清单

#### 7.1.1 ComponentRegistry.h

**完整路径**：`Lucky/Source/Lucky/Scene/ComponentRegistry.h`

```cpp
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Lucky
{
    class Entity;

    /// <summary>
    /// 组件描述符：描述一个组件类型的元数据和操作接口
    /// 每个组件类型对应一个描述符实例，在 ComponentRegistry::Initialize() 中注册
    /// </summary>
    struct ComponentDescriptor
    {
        std::string Name;                   // 显示名称
        std::string Category;               // 分类名（用于 AddComponent 菜单分组）
        
        bool IsRemovable = true;            // 是否可以通过 Inspector 删除
        bool IsAddable = true;              // 是否出现在 AddComponent 列表中
        bool IsHiddenInInspector = true;    // 是否在 Inspector 中完全隐藏
        int SortOrder = 100;                // 显示排序权重（越小越靠前）
        
        std::function<bool(Entity)> HasComponent;       // 检查实体是否拥有该组件
        std::function<void(Entity)> AddComponent;       // 向实体添加该组件
        std::function<void(Entity)> RemoveComponent;    // 从实体移除该组件
        std::function<void(Entity)> DrawInspector;      // 绘制该组件的 Inspector UI
    };

    /// <summary>
    /// 组件注册表：管理所有已注册的组件描述符
    /// </summary>
    class ComponentRegistry
    {
    public:
        static void Initialize();
        static const std::vector<ComponentDescriptor>& GetAllDescriptors();
        static std::vector<const ComponentDescriptor*> GetAddableDescriptors();
        static std::unordered_map<std::string, std::vector<const ComponentDescriptor*>> GetCategorizedAddableDescriptors();
    private:
        static std::vector<ComponentDescriptor> s_Descriptors;
    };
}
```

#### 7.1.2 ComponentRegistry.cpp

**完整路径**：`Lucky/Source/Lucky/Scene/ComponentRegistry.cpp`

完整代码见 [4.3.2 实现文件](#432-实现文件)。

### 7.2 修改文件清单

#### 7.2.1 InspectorPanel.h

**完整路径**：`Luck3DApp/Source/Panels/InspectorPanel.h`

完整代码见 [4.5.1 InspectorPanel.h 修改](#451-inspectorpanelh-修改)。

**关键变化**：
- 移除 `DrawComponent` 模板声明和实现
- 新增 `DrawComponentFromDescriptor` 方法声明
- 新增 `DrawAddComponentButton` 方法声明

#### 7.2.2 InspectorPanel.cpp

**完整路径**：`Luck3DApp/Source/Panels/InspectorPanel.cpp`

完整代码见 [4.5.2 InspectorPanel.cpp 修改](#452-inspectorpanelcpp-修改)。

**关键变化**：
- `DrawComponents` 方法重写：Name 组件特殊处理 → 遍历描述符绘制组件 → 材质编辑器 → AddComponent 按钮
- 新增 `DrawComponentFromDescriptor` 方法
- 新增 `DrawAddComponentButton` 方法
- `DrawMaterialEditor` 方法保持不变

#### 7.2.3 EditorLayer.cpp

**完整路径**：`Luck3DApp/Source/EditorLayer.cpp`

**修改内容**：

```cpp
// 新增 #include
#include "Lucky/Scene/ComponentRegistry.h"

// OnAttach() 开头新增
ComponentRegistry::Initialize();
```

### 7.3 实现步骤

按以下顺序实现，每步完成后可编译验证：

| 步骤 | 操作 | 验证点 |
|:---:|------|--------|
| 1 | 创建 `ComponentRegistry.h`（结构体 + 类声明） | 编译通过 |
| 2 | 创建 `ComponentRegistry.cpp`（Initialize + 所有组件注册） | 编译通过 |
| 3 | 修改 `EditorLayer.cpp`（添加 `#include` 和 `Initialize()` 调用） | 编译通过，运行时 Initialize 被调用 |
| 4 | 修改 `InspectorPanel.h`（移除旧模板，添加新方法声明） | 编译通过 |
| 5 | 修改 `InspectorPanel.cpp`（重写 DrawComponents + 新增方法） | 编译通过 |
| 6 | 运行测试：选中实体，Inspector 显示正确的组件 | Transform 无删除按钮，其他组件有删除按钮 |
| 7 | 运行测试：点击 Add Component 按钮 | 弹出分类菜单，已有组件灰显 |
| 8 | 运行测试：添加组件 | 组件成功添加，Inspector 立即显示 |
| 9 | 运行测试：删除组件 | 组件成功删除，Inspector 立即更新 |

---

## 八、验证方法

### 8.1 功能验证

| 测试用例 | 操作 | 预期结果 |
|---------|------|---------|
| **T-01** 核心组件保护 | 选中任意实体，查看 Transform 组件 | Transform 组件标题栏**没有** "+" 设置按钮 |
| **T-02** 可删除组件 | 选中带有 DirectionalLight 的实体 | DirectionalLight 组件标题栏**有** "+" 设置按钮 |
| **T-03** 删除组件 | 点击 DirectionalLight 的 "+" → "Remove Component" | 组件从 Inspector 中消失 |
| **T-04** AddComponent 按钮 | 选中空实体，点击 "Add Component" | 弹出分类菜单（Rendering / Light） |
| **T-05** 添加组件 | 在 AddComponent 菜单中选择 "Directional Light" | 组件添加成功，Inspector 中显示 |
| **T-06** 已有组件灰显 | 选中已有 DirectionalLight 的实体，打开 AddComponent | "Directional Light" 菜单项灰显不可点击 |
| **T-07** 隐藏组件 | 选中任意实体 | IDComponent 和 RelationshipComponent 不在 Inspector 中显示 |
| **T-08** 组件排序 | 选中同时拥有 Transform + MeshFilter + MeshRenderer + DirectionalLight 的实体 | 组件按 SortOrder 排列：Transform → MeshFilter → MeshRenderer → DirectionalLight |

### 8.2 回归验证

| 测试用例 | 操作 | 预期结果 |
|---------|------|---------|
| **R-01** 材质编辑器 | 选中带 MeshRenderer 的实体 | 材质编辑器正常显示在组件列表下方 |
| **R-02** 场景序列化 | 保存场景 → 重新加载 | 所有组件数据正确恢复 |
| **R-03** Hierarchy 创建菜单 | 右键 → Create → Cube | Cube 实体正确创建，带 MeshFilter + MeshRenderer |
| **R-04** 实体选择 | 在 Hierarchy 中点击不同实体 | Inspector 正确切换显示对应实体的组件 |

---

## 九、后续扩展预留

### 9.1 近期可扩展方向

| 扩展 | 说明 | ComponentDescriptor 改动 |
|------|------|------------------------|
| **组件依赖** | 如 MeshRenderer 依赖 MeshFilter | 添加 `std::vector<std::string> Dependencies` 字段 |
| **组件图标** | 每个组件在标题栏显示图标 | 添加 `ImTextureID Icon` 字段 |
| **组件启用/禁用** | 类似 Unity 的组件 Checkbox | 添加 `bool IsToggleable` 字段 + `std::function<bool(Entity)> IsEnabled` / `SetEnabled` |
| **组件搜索** | AddComponent 弹出框中添加搜索栏 | 不需要改 Descriptor，在 UI 层过滤 |
| **组件复制/粘贴** | 右键菜单中的 Copy/Paste Component | 添加 `std::function<json(Entity)> Serialize` / `Deserialize` |

### 9.2 远期架构演进

```
当前：ComponentRegistry（静态注册，编译期确定组件列表）
  ↓
中期：ComponentRegistry + 反射系统（运行时自动发现组件类型）
  ↓
远期：完整的反射 + 序列化框架（类似 Unreal 的 UCLASS/UPROPERTY）
```

> **建议**：当前阶段使用方案二的静态注册已经足够。等组件数量超过 20 个或需要支持脚本组件时，再考虑引入反射系统。

---

## 附录 A：MeshFilter 组件名称显示问题

当前 `InspectorPanel.cpp` 中 MeshFilter 的组件名使用了 `meshName + " (Mesh Filter)"` 的动态拼接方式：

```cpp
static std::string meshName;
DrawComponent<MeshFilterComponent>(meshName + " (Mesh Filter)", entity, [](MeshFilterComponent& meshFilter)
{
    meshName = meshFilter.Mesh->GetName();
    // ...
});
```

这种方式存在问题：
1. 使用了 `static` 变量，多实体切换时可能显示错误的名称
2. 组件名在 lambda 内部更新，首次绘制时 `meshName` 为空

**方案二的解决方式**：在 `DrawInspector` lambda 中，组件名固定为 "Mesh Filter"（由 `ComponentDescriptor::Name` 决定），Mesh 名称作为组件内部的属性显示。这与 Unity 的做法一致（Unity 的 MeshFilter 组件标题始终是 "Mesh Filter"，Mesh 引用在组件内部显示）。

## 附录 B：Popup ID 冲突问题

当前代码中所有组件共用 `"ComponentSettings"` 作为 Popup ID：

```cpp
ImGui::OpenPopup("ComponentSettings");
```

这会导致 ImGui 在同一帧内只能正确管理一个同名 Popup。方案二通过在 `DrawComponentFromDescriptor` 中使用组件名作为 Popup ID 后缀来解决：

```cpp
std::string popupID = std::format("ComponentSettings##{}", desc.Name);
```

方案一和方案三也需要类似的修复（使用 `typeid(TComponent).hash_code()` 作为后缀）。
