
# EditorIcon Phase4：统一类型图标接口

> 基于 `std::type_index` 的统一图标注册与获取方案，消除按分类（资产/组件/实体）分散管理图标的冗余设计。

---

## 目录

1. [背景与动机](#1-背景与动机)
2. [设计目标](#2-设计目标)
3. [架构概览](#3-架构概览)
4. [详细设计](#4-详细设计)
5. [实现步骤](#5-实现步骤)
6. [迁移策略](#6-迁移策略)
7. [调用方式对比](#7-调用方式对比)
8. [扩展性考虑](#8-扩展性考虑)

---

## 1. 背景与动机

### 1.1 当前问题

当前 `EditorIconManager` 按"分类"管理图标，存在以下问题：

| 问题 | 说明 |
|------|------|
| 接口分散 | `GetAssetTypeIcon(AssetType)`、`GetComponentIcon(ComponentType)`、`GetEntityIcon()` 三套接口 |
| 调用方需要知道分类 | 想获取 Material 图标，必须知道它是"资产"；想获取 Light 图标，必须知道它是"组件" |
| ObjectField 无法统一 | 未来 ObjectField 需要显示任意对象类型的图标，必须先判断分类再调用对应接口 |
| 枚举膨胀 | 每新增一种类型，需要在 `AssetType` 或 `ComponentType` 枚举中加值 |

### 1.2 设计参考

Unity 的图标系统以 **类型（Type/Class）** 作为唯一索引键，不区分对象是"组件"还是"资产"还是"实体"。`EditorGUIUtility.ObjectContent(obj, type)` 内部通过类型查找图标。

### 1.3 决策记录

- **保留 `Component` 后缀**：经分析，项目中 `Camera`（渲染层基类）与未来的 `CameraComponent`（ECS 组件）存在真实命名冲突，去掉后缀无优雅解决方案。代码层保留后缀，UI 显示层通过 DisplayName 机制去掉后缀。
- **不引入完整 TypeID 系统**：使用标准库 `std::type_index` 作为类型标识，无需自定义 TypeID 基础设施。
- **保留 `ComponentIconResolver`**：处理运行时状态决定图标的情况（如 LightComponent 的子类型）。

---

## 2. 设计目标

1. **统一入口**：对外提供 `GetIcon<T>()` 模板接口，一行代码获取任意类型的图标
2. **零分类负担**：调用方无需知道类型属于"资产/组件/实体"哪个分类
3. **运行时支持**：提供 `GetIconByTypeIndex(std::type_index)` 接口，支持运行时动态类型查询（供未来 ObjectField 使用）
4. **子类型兼容**：保留 `ComponentIconResolver` 机制，处理同一 C++ 类型内部运行时状态不同的情况
5. **向后兼容**：旧接口可保留为 deprecated 或内部转发，不破坏现有调用方

---

## 3. 架构概览

### 3.1 类图

```
┌─────────────────────────────────────────────────────────────────┐
│                      EditorIconManager                           │
├─────────────────────────────────────────────────────────────────┤
│ + Init()                                                        │
│ + Shutdown()                                                    │
│                                                                 │
│ // ---- 统一类型图标接口（新增） ----                              │
│ + GetIcon<T>() : const Ref<Texture2D>&          [编译期入口]     │
│ + GetIconByTypeIndex(type_index) : const Ref<Texture2D>&  [运行时入口] │
│                                                                 │
│ // ---- 特殊图标（非类型关联） ----                               │
│ + GetLightIcon(LightType) : const Ref<Texture2D>&               │
│ + GetFolderIcon(bool isOpen) : const Ref<Texture2D>&            │
│ + GetFileIcon() : const Ref<Texture2D>&                         │
│                                                                 │
│ // ---- 旧接口（标记 deprecated，内部转发） ----                  │
│ + GetAssetTypeIcon(AssetType) : const Ref<Texture2D>&           │
│ + GetComponentIcon(ComponentType) : const Ref<Texture2D>&       │
│ + GetEntityIcon() : const Ref<Texture2D>&                       │
│                                                                 │
│ - RegisterIcon<T>(icon)                         [Init 时调用]    │
│ - RegisterIconByTypeIndex(type_index, icon)                     │
├─────────────────────────────────────────────────────────────────┤
│                    EditorIconData (内部)                          │
│ - TypeIcons : unordered_map<type_index, Ref<Texture2D>>         │
│ - LightIcons : unordered_map<LightType, Ref<Texture2D>>         │
│ - FolderIcon, FolderOpenIcon, FileIcon                          │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│              ComponentIconResolver<TComponent>                    │
├─────────────────────────────────────────────────────────────────┤
│ + GetIcon(const TComponent&) : const Ref<Texture2D>&            │
│   默认实现：return EditorIconManager::GetIcon<TComponent>();     │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│          ComponentIconResolver<LightComponent> (特化)             │
├─────────────────────────────────────────────────────────────────┤
│ + GetIcon(const LightComponent&) : const Ref<Texture2D>&        │
│   实现：return EditorIconManager::GetLightIcon(light.Type);      │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 数据流

```mermaid
flowchart LR
    subgraph 编译期已知类型
        A[调用方] -->|"GetIcon&lt;Material&gt;()"| B[EditorIconManager]
        B -->|typeid(Material)| C[TypeIcons Map]
        C -->|返回| D[Ref&lt;Texture2D&gt;]
    end

    subgraph 运行时动态类型
        E[ObjectField] -->|"GetIconByTypeIndex(typeIndex)"| B
    end

    subgraph 运行时子类型
        F[Hierarchy/Inspector] -->|"ComponentIconResolver&lt;LightComponent&gt;::GetIcon(light)"| G[GetLightIcon]
        G -->|LightType| H[LightIcons Map]
    end
```

---

## 4. 详细设计

### 4.1 EditorIconManager.h

```cpp
#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Texture.h"
#include "Lucky/Scene/Components/LightComponent.h"

#include <typeindex>

namespace Lucky
{
    // 前置声明
    class Entity;
    struct TransformComponent;
    struct MeshFilterComponent;
    struct MeshRendererComponent;
    struct PostProcessVolumeComponent;
    class Material;
    class Mesh;
    class Texture2D;
    class Scene;
    class Shader;

    /// <summary>
    /// 编辑器图标管理器
    /// 通过类型（std::type_index）统一注册和获取图标
    /// </summary>
    class EditorIconManager
    {
    public:
        /// <summary>
        /// 初始化：加载并注册所有图标纹理
        /// </summary>
        static void Init();

        /// <summary>
        /// 销毁：释放所有图标纹理
        /// </summary>
        static void Shutdown();

        // ======== 统一类型图标接口 ========

        /// <summary>
        /// 通过 C++ 类型获取图标（编译期主要入口）
        /// </summary>
        /// <typeparam name="T">任意已注册图标的类型</typeparam>
        /// <returns>对应的图标纹理，未注册类型返回通用文件图标</returns>
        template<typename T>
        static const Ref<Texture2D>& GetIcon()
        {
            return GetIconByTypeIndex(std::type_index(typeid(T)));
        }

        /// <summary>
        /// 通过 std::type_index 获取图标（运行时入口）
        /// </summary>
        /// <param name="typeIndex">类型索引</param>
        /// <returns>对应的图标纹理，未注册类型返回通用文件图标</returns>
        static const Ref<Texture2D>& GetIconByTypeIndex(std::type_index typeIndex);

        // ======== 特殊图标（非类型关联） ========

        /// <summary>
        /// 获取光源子类型图标（运行时状态决定）
        /// </summary>
        /// <param name="lightType">光源类型（Directional/Point/Spot）</param>
        /// <returns>对应光源类型的图标纹理</returns>
        static const Ref<Texture2D>& GetLightIcon(LightType lightType);

        /// <summary>
        /// 获取文件夹图标
        /// </summary>
        /// <param name="isOpen">是否为打开状态</param>
        /// <returns>文件夹图标纹理</returns>
        static const Ref<Texture2D>& GetFolderIcon(bool isOpen = false);

        /// <summary>
        /// 获取通用文件图标（fallback）
        /// </summary>
        /// <returns>文件图标纹理</returns>
        static const Ref<Texture2D>& GetFileIcon();

        // ======== 旧接口（deprecated，内部转发） ========

        /// <summary>
        /// [Deprecated] 获取资产类型图标，请改用 GetIcon&lt;T&gt;()
        /// </summary>
        static const Ref<Texture2D>& GetAssetTypeIcon(AssetType type);

        /// <summary>
        /// [Deprecated] 获取组件图标，请改用 GetIcon&lt;T&gt;()
        /// </summary>
        static const Ref<Texture2D>& GetComponentIcon(ComponentType type);

        /// <summary>
        /// [Deprecated] 获取实体图标，请改用 GetIcon&lt;Entity&gt;()
        /// </summary>
        static const Ref<Texture2D>& GetEntityIcon();

    private:
        /// <summary>
        /// 注册类型图标（Init 时调用）
        /// </summary>
        /// <typeparam name="T">要注册图标的类型</typeparam>
        /// <param name="icon">图标纹理</param>
        template<typename T>
        static void RegisterIcon(const Ref<Texture2D>& icon)
        {
            RegisterIconByTypeIndex(std::type_index(typeid(T)), icon);
        }

        /// <summary>
        /// 注册类型图标（内部实现）
        /// </summary>
        /// <param name="typeIndex">类型索引</param>
        /// <param name="icon">图标纹理</param>
        static void RegisterIconByTypeIndex(std::type_index typeIndex, const Ref<Texture2D>& icon);
    };

    // ======== 组件图标解析器 ========

    /// <summary>
    /// 组件图标解析器（基础模板）
    /// 默认行为：通过 GetIcon&lt;TComponent&gt;() 获取图标
    /// 对于有子类型的组件（如 LightComponent），通过模板特化实现运行时动态图标
    /// </summary>
    template<typename TComponent>
    struct ComponentIconResolver
    {
        static const Ref<Texture2D>& GetIcon(const TComponent& /*component*/)
        {
            return EditorIconManager::GetIcon<TComponent>();
        }
    };

    /// <summary>
    /// LightComponent 图标解析器特化：根据光源子类型返回不同图标
    /// </summary>
    template<>
    struct ComponentIconResolver<LightComponent>
    {
        static const Ref<Texture2D>& GetIcon(const LightComponent& light)
        {
            return EditorIconManager::GetLightIcon(light.Type);
        }
    };
}
```

### 4.2 EditorIconManager.cpp

```cpp
#include "lcpch.h"
#include "EditorIconManager.h"

#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/Components.h"
#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/Mesh.h"
#include "Lucky/Renderer/Texture.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Asset/AssetType.h"

#include <unordered_map>
#include <typeindex>

namespace Lucky
{
    /// <summary>
    /// 图标管理器内部数据
    /// </summary>
    struct EditorIconData
    {
        // 统一类型图标表：以 std::type_index 为键
        std::unordered_map<std::type_index, Ref<Texture2D>> TypeIcons;

        // 光源子类型图标（运行时状态，非类型区分）
        std::unordered_map<LightType, Ref<Texture2D>> LightIcons;

        // 通用图标（非类型关联）
        Ref<Texture2D> FolderIcon;
        Ref<Texture2D> FolderOpenIcon;
        Ref<Texture2D> FileIcon;

        // ---- 旧接口兼容数据 ----
        std::unordered_map<AssetType, std::type_index> AssetTypeToTypeIndex;
        std::unordered_map<ComponentType, std::type_index> ComponentTypeToTypeIndex;
    };

    static EditorIconData s_IconData;

    static constexpr const char* s_IconRootPath = "Resources/Icons";

    /// <summary>
    /// 加载单个图标纹理
    /// </summary>
    static Ref<Texture2D> LoadIcon(const std::string& relativePath)
    {
        std::string fullPath = std::string(s_IconRootPath) + "/" + relativePath;

        Ref<Texture2D> texture = Texture2D::Create(fullPath);
        if (!texture || texture->GetRendererID() == 0)
        {
            LF_CORE_WARN("EditorIconManager: Failed to load icon '{0}'", fullPath);
            return nullptr;
        }

        return texture;
    }

    void EditorIconManager::RegisterIconByTypeIndex(std::type_index typeIndex, const Ref<Texture2D>& icon)
    {
        s_IconData.TypeIcons[typeIndex] = icon;
    }

    void EditorIconManager::Init()
    {
        LF_CORE_INFO("EditorIconManager::Init - Loading editor icons...");

        // ---- 加载通用图标 ----
        s_IconData.FolderIcon     = LoadIcon("Common/Folder.png");
        s_IconData.FolderOpenIcon = LoadIcon("Common/FolderOpen.png");
        s_IconData.FileIcon       = LoadIcon("Common/File.png");

        // ---- 注册实体图标 ----
        RegisterIcon<Entity>(LoadIcon("Entity/Entity.png"));

        // ---- 注册组件图标 ----
        RegisterIcon<TransformComponent>(LoadIcon("Component/Transform.png"));
        RegisterIcon<MeshFilterComponent>(LoadIcon("Component/MeshFilter.png"));
        RegisterIcon<MeshRendererComponent>(LoadIcon("Component/MeshRenderer.png"));
        RegisterIcon<LightComponent>(LoadIcon("Component/Light.png"));
        RegisterIcon<PostProcessVolumeComponent>(LoadIcon("Component/PostProcessVolume.png"));

        // ---- 注册资产图标 ----
        RegisterIcon<Material>(LoadIcon("Asset/Material.png"));
        RegisterIcon<Mesh>(LoadIcon("Asset/Mesh.png"));
        RegisterIcon<Texture2D>(LoadIcon("Asset/Texture.png"));
        RegisterIcon<Scene>(LoadIcon("Asset/Scene.png"));
        // RegisterIcon<Shader>(LoadIcon("Asset/Shader.png"));  // Shader 类暂未定义为独立类型

        // ---- 加载光源子类型图标（运行时状态） ----
        s_IconData.LightIcons[LightType::Directional] = LoadIcon("Component/DirectionalLight.png");
        s_IconData.LightIcons[LightType::Point]       = LoadIcon("Component/PointLight.png");
        s_IconData.LightIcons[LightType::Spot]        = LoadIcon("Component/SpotLight.png");

        // ---- 旧接口兼容映射表 ----
        s_IconData.AssetTypeToTypeIndex = {
            { AssetType::Material,  std::type_index(typeid(Material)) },
            { AssetType::Mesh,      std::type_index(typeid(Mesh)) },
            { AssetType::Texture2D, std::type_index(typeid(Texture2D)) },
            { AssetType::Scene,     std::type_index(typeid(Scene)) },
        };

        s_IconData.ComponentTypeToTypeIndex = {
            { ComponentType::Transform,         std::type_index(typeid(TransformComponent)) },
            { ComponentType::MeshFilter,        std::type_index(typeid(MeshFilterComponent)) },
            { ComponentType::MeshRenderer,      std::type_index(typeid(MeshRendererComponent)) },
            { ComponentType::Light,             std::type_index(typeid(LightComponent)) },
            { ComponentType::PostProcessVolume, std::type_index(typeid(PostProcessVolumeComponent)) },
        };

        LF_CORE_INFO("EditorIconManager::Init - Done. Registered {0} type icons.", s_IconData.TypeIcons.size());
    }

    void EditorIconManager::Shutdown()
    {
        LF_CORE_INFO("EditorIconManager::Shutdown");

        s_IconData.TypeIcons.clear();
        s_IconData.LightIcons.clear();
        s_IconData.AssetTypeToTypeIndex.clear();
        s_IconData.ComponentTypeToTypeIndex.clear();
        s_IconData.FolderIcon.reset();
        s_IconData.FolderOpenIcon.reset();
        s_IconData.FileIcon.reset();
    }

    // ======== 统一接口实现 ========

    const Ref<Texture2D>& EditorIconManager::GetIconByTypeIndex(std::type_index typeIndex)
    {
        auto it = s_IconData.TypeIcons.find(typeIndex);
        if (it != s_IconData.TypeIcons.end() && it->second)
        {
            return it->second;
        }

        // fallback 到通用文件图标
        return s_IconData.FileIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetLightIcon(LightType lightType)
    {
        auto it = s_IconData.LightIcons.find(lightType);
        if (it != s_IconData.LightIcons.end() && it->second)
        {
            return it->second;
        }

        // 回退到通用 Light 组件图标
        return GetIcon<LightComponent>();
    }

    const Ref<Texture2D>& EditorIconManager::GetFolderIcon(bool isOpen)
    {
        if (isOpen && s_IconData.FolderOpenIcon)
        {
            return s_IconData.FolderOpenIcon;
        }

        return s_IconData.FolderIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetFileIcon()
    {
        return s_IconData.FileIcon;
    }

    // ======== 旧接口兼容实现（内部转发到统一接口） ========

    const Ref<Texture2D>& EditorIconManager::GetAssetTypeIcon(AssetType type)
    {
        auto it = s_IconData.AssetTypeToTypeIndex.find(type);
        if (it != s_IconData.AssetTypeToTypeIndex.end())
        {
            return GetIconByTypeIndex(it->second);
        }

        return s_IconData.FileIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetComponentIcon(ComponentType type)
    {
        auto it = s_IconData.ComponentTypeToTypeIndex.find(type);
        if (it != s_IconData.ComponentTypeToTypeIndex.end())
        {
            return GetIconByTypeIndex(it->second);
        }

        static Ref<Texture2D> s_NullIcon = nullptr;
        return s_NullIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetEntityIcon()
    {
        return GetIcon<Entity>();
    }
}
```

### 4.3 关键设计决策

#### 4.3.1 内部存储键的选择

| 方案 | 实现方式 | 优点 | 缺点 | 推荐 |
|------|---------|------|------|------|
| **A. `std::type_index`** | `std::type_index(typeid(T))` | 标准库自带，无需额外基础设施；`unordered_map` 原生支持 hash | 需要 RTTI（项目默认开启）；运行时有微小开销 | ? **推荐** |
| B. 编译期计数器 ID | 静态局部变量 + 原子计数器 | 无 RTTI 依赖；整数比较更快 | 需要自定义基础设施；跨 DLL 时 ID 不稳定 | 不推荐（过度设计） |
| C. 字符串类型名 | `typeid(T).name()` | 可调试、可序列化 | 编译器相关（MSVC 带修饰名）；字符串比较慢 | 不推荐 |

**推荐原因**：方案 A 使用 `std::type_index`，零额外代码，标准库保证 hash 和相等比较正确性，且项目已默认开启 RTTI（`typeid` 在多处使用）。

#### 4.3.2 Fallback 策略

当 `GetIconByTypeIndex` 找不到对应类型的图标时：

| 方案 | 行为 | 优点 | 缺点 | 推荐 |
|------|------|------|------|------|
| **A. 返回通用文件图标** | `return s_IconData.FileIcon;` | 永远不返回 nullptr，调用方无需判空 | 可能掩盖注册遗漏 | ? **推荐** |
| B. 返回 nullptr | `return s_NullIcon;` | 调用方可区分"未注册"和"已注册" | 所有调用方都需要判空 | 不推荐 |
| C. 断言 + 返回文件图标 | Debug 模式断言，Release 返回文件图标 | 开发期能发现遗漏 | 断言可能过于激进 | 可选（作为 A 的增强） |

**推荐原因**：方案 A 保证接口始终返回有效纹理，与当前 `GetAssetTypeIcon` 的行为一致。可在 Debug 模式下加一条 `LF_CORE_WARN` 日志提示未注册类型。

#### 4.3.3 旧接口处理策略

| 方案 | 说明 | 优点 | 缺点 | 推荐 |
|------|------|------|------|------|
| **A. 保留并标记 deprecated，内部转发** | 旧接口保留，内部通过映射表转发到新接口 | 零破坏性；调用方可逐步迁移 | 代码中保留了冗余接口 | ? **推荐（第一阶段）** |
| B. 直接删除旧接口 | 一次性移除所有旧接口 | 代码最干净 | 需要一次性修改所有调用方 | 推荐（第二阶段） |
| C. 旧接口作为 `GetIcon<T>()` 的别名宏 | `#define GetEntityIcon() GetIcon<Entity>()` | 零运行时开销 | 宏不在命名空间内，可能冲突 | 不推荐 |

**推荐原因**：方案 A 允许渐进式迁移。第一阶段保留旧接口（内部转发），第二阶段在所有调用方迁移完成后删除旧接口和兼容映射表。

#### 4.3.4 `ComponentIconResolver` 默认实现

| 方案 | 默认实现 | 优点 | 缺点 | 推荐 |
|------|---------|------|------|------|
| **A. 调用 `GetIcon<TComponent>()`** | `return EditorIconManager::GetIcon<TComponent>();` | 与统一接口完全一致；无需 `ComponentTrait` | 简洁 | ? **推荐** |
| B. 调用 `GetComponentIcon(ComponentTrait<T>::Type)` | 通过 ComponentTrait 获取枚举再查找 | 与旧系统兼容 | 多一层间接；依赖 ComponentTrait | 不推荐（旧方案） |

**推荐原因**：方案 A 让 `ComponentIconResolver` 的默认实现直接使用新的统一接口，消除对 `ComponentTrait` 的依赖（图标获取不再需要 `ComponentType` 枚举）。

#### 4.3.5 `AssetType` 枚举是否保留

`AssetType` 枚举在图标系统之外还有其他用途（序列化、Importer 注册、文件扩展名推断等），**不应删除**。只是图标获取不再以它为键。

| 用途 | 是否仍需要 `AssetType` |
|------|----------------------|
| 图标获取 | ? 改用 `GetIcon<T>()` |
| AssetImporter 注册 | ? 保留 |
| AssetRegistry 序列化 | ? 保留 |
| 文件扩展名推断 | ? 保留 |

同理，`ComponentType` 枚举在序列化、运行时反射等场景仍有价值，保留不删。

---

## 5. 实现步骤

### Step 1：修改 EditorIconManager.h

1. 添加 `#include <typeindex>` 头文件
2. 添加前置声明（Entity、各组件、各资产类型）
3. 新增 `GetIcon<T>()` 模板方法
4. 新增 `GetIconByTypeIndex(std::type_index)` 方法声明
5. 新增 `RegisterIcon<T>()` 私有模板方法
6. 新增 `RegisterIconByTypeIndex()` 私有方法声明
7. 旧接口保留，添加 `[Deprecated]` 注释
8. 修改 `ComponentIconResolver` 默认实现为调用 `GetIcon<TComponent>()`
9. 移除对 `AssetType.h` 和 `ComponentType.h` 的 include 依赖（旧接口声明需要保留则保留）

> **注意**：旧接口声明中使用了 `AssetType` 和 `ComponentType`，因此在第一阶段仍需保留这两个头文件的 include。第二阶段删除旧接口后可移除。

### Step 2：修改 EditorIconManager.cpp

1. 添加必要的 include（Entity.h、Components.h、Material.h、Mesh.h、Texture.h、Scene.h）
2. 修改 `EditorIconData` 结构体：
   - 新增 `std::unordered_map<std::type_index, Ref<Texture2D>> TypeIcons`
   - 新增旧接口兼容映射表
   - 移除旧的 `AssetTypeIcons` 和 `ComponentIcons` map
3. 实现 `RegisterIconByTypeIndex()`
4. 修改 `Init()`：使用 `RegisterIcon<T>()` 注册所有图标
5. 实现 `GetIconByTypeIndex()`
6. 修改旧接口实现为内部转发

### Step 3：迁移调用方（可选，可逐步进行）

| 文件 | 当前调用 | 迁移后 |
|------|---------|--------|
| SceneHierarchyPanel.cpp | `GetComponentIcon(ComponentType::MeshFilter)` | `GetIcon<MeshFilterComponent>()` |
| SceneHierarchyPanel.cpp | `GetEntityIcon()` | `GetIcon<Entity>()` |
| SceneHierarchyPanel.cpp | `GetAssetTypeIcon(AssetType::Scene)` | `GetIcon<Scene>()` |
| SceneHierarchyPanel.cpp | `GetLightIcon(lightType)` | 保持不变（运行时子类型） |
| InspectorPanel.h | `ComponentIconResolver<T>::GetIcon(comp)` | 无需修改（内部已自动转发） |
| ProjectAssetsPanel.cpp | `GetAssetTypeIcon(type)` | 见下方说明 |

**ProjectAssetsPanel 特殊处理**：该面板通过文件扩展名推断 `AssetType`，再获取图标。由于它拿到的是运行时的 `AssetType` 枚举值而非编译期类型，有两种处理方式：

| 方案 | 说明 | 推荐 |
|------|------|------|
| A. 继续使用旧接口 `GetAssetTypeIcon(type)` | 旧接口内部已转发到新系统 | ? 推荐（第一阶段） |
| B. 建立 `AssetType → type_index` 映射后调用 `GetIconByTypeIndex` | 需要额外映射函数 | 可选（第二阶段） |

### Step 4：清理（第二阶段，所有调用方迁移完成后）

1. 删除旧接口声明和实现
2. 删除 `EditorIconData` 中的兼容映射表
3. 移除 `EditorIconManager.h` 中对 `AssetType.h`、`ComponentType.h` 的 include（如果不再需要）

---

## 6. 迁移策略

### 6.1 两阶段迁移

```
┌─────────────────────────────────────────────────────────────────┐
│ 第一阶段：新旧共存                                                │
│                                                                 │
│ - 新增统一接口 GetIcon<T>() / GetIconByTypeIndex()               │
│ - 旧接口保留，内部转发到新系统                                     │
│ - 新代码使用新接口                                                │
│ - 旧代码无需修改，自动通过转发层使用新系统                          │
│                                                                 │
│ 验证标准：编译通过 + 所有图标正常显示                               │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│ 第二阶段：清理旧接口                                              │
│                                                                 │
│ - 逐步将所有调用方迁移到新接口                                     │
│ - 删除旧接口和兼容映射表                                          │
│ - 评估是否可移除 ComponentType 枚举（如果其他地方不再使用）          │
│                                                                 │
│ 验证标准：编译通过 + 无 deprecated 调用                            │
└─────────────────────────────────────────────────────────────────┘
```

### 6.2 风险控制

| 风险 | 缓解措施 |
|------|---------|
| 遗漏注册某个类型 | `GetIconByTypeIndex` 返回文件图标 + Debug 日志警告 |
| 旧接口转发映射表不完整 | 在 `Init()` 中静态断言映射表覆盖所有枚举值 |
| RTTI 被禁用 | 项目已在多处使用 `typeid`（如 `InspectorPanel.h` 中的 `typeid(TComponent).hash_code()`），确认 RTTI 已开启 |

---

## 7. 调用方式对比

### 7.1 获取图标

| 场景 | 旧方式 | 新方式 |
|------|--------|--------|
| Material 图标 | `GetAssetTypeIcon(AssetType::Material)` | `GetIcon<Material>()` |
| Mesh 图标 | `GetAssetTypeIcon(AssetType::Mesh)` | `GetIcon<Mesh>()` |
| Texture2D 图标 | `GetAssetTypeIcon(AssetType::Texture2D)` | `GetIcon<Texture2D>()` |
| Scene 图标 | `GetAssetTypeIcon(AssetType::Scene)` | `GetIcon<Scene>()` |
| Transform 图标 | `GetComponentIcon(ComponentType::Transform)` | `GetIcon<TransformComponent>()` |
| MeshFilter 图标 | `GetComponentIcon(ComponentType::MeshFilter)` | `GetIcon<MeshFilterComponent>()` |
| MeshRenderer 图标 | `GetComponentIcon(ComponentType::MeshRenderer)` | `GetIcon<MeshRendererComponent>()` |
| Light 图标（通用） | `GetComponentIcon(ComponentType::Light)` | `GetIcon<LightComponent>()` |
| Light 图标（按子类型） | `GetLightIcon(light.Type)` | `GetLightIcon(light.Type)` ← 保持不变 |
| Entity 图标 | `GetEntityIcon()` | `GetIcon<Entity>()` |

### 7.2 Inspector 组件图标（无变化）

```cpp
// DrawComponent 模板内部（无需修改）
const Ref<Texture2D>& componentIcon = ComponentIconResolver<TComponent>::GetIcon(component);
```

`ComponentIconResolver` 的默认实现已自动调用 `GetIcon<TComponent>()`。

### 7.3 Hierarchy 组件图标

```cpp
// 旧方式
if (entity.HasComponent<MeshFilterComponent>())
    icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::MeshFilter));

// 新方式
if (entity.HasComponent<MeshFilterComponent>())
    icons.push_back(&EditorIconManager::GetIcon<MeshFilterComponent>());
```

### 7.4 未来 ObjectField 中的使用

```cpp
// ObjectField<T> 模板内部
template<typename T>
bool ObjectField(const char* label, Ref<T>& objectRef)
{
    const Ref<Texture2D>& icon = EditorIconManager::GetIcon<T>();
    // 绘制带图标的 ObjectField UI...
}
```

---

## 8. 扩展性考虑

### 8.1 新增类型

未来新增任何需要图标的类型，只需在 `Init()` 中添加一行：

```cpp
RegisterIcon<NewType>(LoadIcon("Category/NewType.png"));
```

无需修改枚举、无需修改接口签名、无需修改调用方。

### 8.2 新增有子类型的组件

如果未来有新的组件需要根据运行时状态显示不同图标（如 `ColliderComponent` 有 Box/Sphere/Capsule 子类型）：

1. 在 `EditorIconData` 中添加子类型图标 map
2. 在 `EditorIconManager` 中添加 `GetColliderIcon(ColliderType)` 接口
3. 添加 `ComponentIconResolver<ColliderComponent>` 特化

```cpp
template<>
struct ComponentIconResolver<ColliderComponent>
{
    static const Ref<Texture2D>& GetIcon(const ColliderComponent& collider)
    {
        return EditorIconManager::GetColliderIcon(collider.Shape);
    }
};
```

### 8.3 运行时动态注册（预留）

当前设计中所有图标在 `Init()` 时静态注册。如果未来需要支持插件动态注册图标：

```cpp
// 预留接口（当前不实现）
static void RegisterIconForType(std::type_index typeIndex, const Ref<Texture2D>& icon);
```

只需将 `RegisterIconByTypeIndex` 从 private 改为 public 即可。

### 8.4 与 DisplayName 系统的配合

统一图标接口天然适配未来的 DisplayName 系统：

```cpp
// 类型 → 图标
EditorIconManager::GetIcon<LightComponent>();           // 返回图标

// 类型 → 显示名称（未来实现）
EditorDisplayName::GetName<LightComponent>();           // 返回 "Light"
EditorDisplayName::GetName<TransformComponent>();       // 返回 "Transform"
EditorDisplayName::GetName<MeshFilterComponent>();      // 返回 "Mesh Filter"
```

两者共享相同的"以类型为键"的设计哲学。

---

## 附录：涉及的文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `Lucky/Source/Lucky/Editor/EditorIconManager.h` | 修改 | 新增统一接口，修改 ComponentIconResolver 默认实现 |
| `Lucky/Source/Lucky/Editor/EditorIconManager.cpp` | 修改 | 重构内部存储，实现新接口，旧接口转发 |
| `Luck3DApp/Source/Panels/SceneHierarchyPanel.cpp` | 修改（第二阶段） | 迁移到新接口 |
| `Luck3DApp/Source/Panels/InspectorPanel.h` | 无需修改 | ComponentIconResolver 内部自动适配 |
| `Luck3DApp/Source/Panels/ProjectAssetsPanel.cpp` | 可选修改 | 可继续使用旧接口（已转发） |
