# Phase D - Part 1：Scene 纳入资产系统

## 目录

- [一、概述](#一概述)
  - [1.1 本文档范围](#11-本文档范围)
  - [1.2 设计目标](#12-设计目标)
  - [1.3 前置依赖](#13-前置依赖)
  - [1.4 术语定义](#14-术语定义)
- [二、当前问题](#二当前问题)
- [三、设计目标](#三设计目标)
- [四、Scene 继承 Asset](#四scene-继承-asset)
- [五、SceneImporter 设计](#五sceneimporter-设计)
- [六、EditorLayer 改造](#六editorlayer-改造)
  - [6.1 设计思路：不再需要 Save As](#61-设计思路不再需要-save-as)
  - [6.2 方案 A：通过 AssetManager 管理场景](#62-方案-aeditorlayer-通过-assetmanager-管理场景)
  - [6.3 方案 B：仅注册不通过 AssetManager 加载](#63-方案-beditorlayer-仅注册不通过-assetmanager-加载)
  - [6.4 方案推荐](#64-方案推荐)
- [七、场景注册到 Registry](#七场景注册到-registry)
- [八、序列化器设计分析：静态 vs 非静态](#八序列化器设计分析静态-vs-非静态)
  - [8.1 当前现状对比](#81-当前现状对比)
  - [8.2 方案分析](#82-方案分析)
  - [8.3 是否需要提取序列化基类](#83-是否需要提取序列化基类)
  - [8.4 关于进度回调的补充分析](#84-关于进度回调的补充分析)
  - [8.5 结论与推荐](#85-结论与推荐)
- [九、涉及的文件清单](#九涉及的文件清单)
- [十、分步实施策略](#十分步实施策略)
- [十一、验证清单](#十一验证清单)
- [十二、已知限制与后续扩展](#十二已知限制与后续扩展)

---

## 一、概述

### 1.1 本文档范围

本文档是 Phase D 高优先级功能的 **Part 1**，专注于将 Scene 纳入资产系统管理。

| 原始文档 | 拆分来源 |
|---------|---------|
| `PhaseD_High_Priority_Features.md` | 第二章：Part 1：Scene 纳入资产系统 |

### 1.2 设计目标

1. ? Scene 成为一等公民资产，通过 AssetHandle 统一引用
2. ? 场景文件注册到 Registry，可在 ProjectAssetsPanel 中管理
3. ? 与现有 Phase A/B 架构无缝集成
4. ? 遵循项目现有代码规范和架构风格

### 1.3 前置依赖

| 依赖 | 状态 | 说明 |
|------|------|------|
| Phase A 资产系统核心框架 | ? 已完成 | AssetHandle / AssetRegistry / AssetManager / AssetImporter |
| Phase B 独立材质文件 | ? 已完成 | MaterialSerializer::SerializeToFile/DeserializeFromFile |
| SceneSerializer | ? 已完成 | 场景 YAML 序列化/反序列化 |

### 1.4 术语定义

| 术语 | 定义 |
|------|------|
| **.luck3d** | Luck3D 场景文件格式（YAML） |
| **SceneImporter** | 场景资产导入器，通过 AssetManager 加载场景 |

---

## 二、当前问题

当前 `Scene` 类不继承 `Asset`，场景文件的打开/保存完全由 `EditorLayer` 手动处理：

```cpp
// 当前实现（EditorLayer.cpp）
void EditorLayer::OpenScene()
{
    std::string filepath = FileDialogs::OpenFile("Scene (*.luck3d)\0*.luck3d\0");
    SceneSerializer serializer(m_Scene);
    serializer.Deserialize(filepath);  // 直接反序列化，不经过 AssetManager
}
```

| 问题 | 影响 |
|------|------|
| Scene 无 AssetHandle | 无法在 ProjectAssetsPanel 中通过双击打开场景 |
| 场景不注册到 Registry | 无法追踪项目中有哪些场景文件 |
| 打开/保存不经过 AssetManager | 与资产系统脱节，无法利用缓存和统一管理 |
| 无法被其他资产引用 | 后续 Prefab 系统需要引用子场景 |

---

## 三、设计目标

1. `Scene` 继承 `Asset`，实现 `GetAssetType()` 返回 `AssetType::Scene`
2. 实现 `SceneImporter`，通过 AssetManager 加载场景
3. 场景文件注册到 Registry，拥有 AssetHandle
4. EditorLayer 的 Open/Save 通过 AssetManager 管理
5. 场景文件中记录自身的 AssetHandle

---

## 四、Scene 继承 Asset

### 方案 A：Scene 直接继承 Asset（? 推荐）

```cpp
// Scene.h
#include "Lucky/Asset/Asset.h"

class Scene : public Asset
{
public:
    AssetType GetAssetType() const override { return AssetType::Scene; }
    
    // ... 现有接口不变 ...
};
```

**优点**：
- 最简单直接
- 与 Material / Mesh / Texture 保持一致
- Scene 自带 Handle，可直接通过 `GetHandle()` 获取

**缺点**：
- Scene 是一个重量级对象（持有 entt::registry），缓存策略需要考虑
- 多场景同时加载时内存占用大

### 方案 B：SceneAsset 包装类

创建独立的 `SceneAsset` 类继承 Asset，内部持有 `Ref<Scene>`。

```cpp
class SceneAsset : public Asset
{
public:
    AssetType GetAssetType() const override { return AssetType::Scene; }
    Ref<Scene> GetScene() const { return m_Scene; }
private:
    Ref<Scene> m_Scene;
};
```

**优点**：
- 不修改现有 Scene 类
- 可以在 SceneAsset 中添加额外元数据（如场景缩略图路径）

**缺点**：
- 增加一层间接性
- 使用时需要 `sceneAsset->GetScene()` 多一步
- 与 Material/Mesh 的模式不一致（它们直接继承 Asset）

### 方案推荐

| 方案 | 推荐度 | 理由 |
|------|--------|------|
| **方案 A：直接继承** | ??? 最优 | 与现有模式一致，简单直接，Scene 本身就是资产 |
| 方案 B：包装类 | ?? | 过度设计，增加不必要的复杂度 |

**推荐方案 A**。

---

## 五、SceneImporter 设计

### 方案 A：SceneImporter 内部使用 SceneSerializer（? 推荐）

```cpp
// SceneImporter.h
#pragma once

#include "AssetImporter.h"

namespace Lucky
{
    /// <summary>
    /// 场景资产导入器：从 .luck3d 文件加载场景
    /// </summary>
    class SceneImporter : public AssetImporter
    {
    public:
        Ref<void> Load(const AssetMetadata& metadata) override;
    };
}

// SceneImporter.cpp
#include "lcpch.h"
#include "SceneImporter.h"

#include "Lucky/Scene/Scene.h"
#include "Lucky/Serialization/SceneSerializer.h"

#include <filesystem>

namespace Lucky
{
    Ref<void> SceneImporter::Load(const AssetMetadata& metadata)
    {
        std::string absolutePath = std::filesystem::absolute(metadata.FilePath).string();

        Ref<Scene> scene = CreateRef<Scene>();
        SceneSerializer serializer(scene);

        if (!serializer.Deserialize(absolutePath))
        {
            LF_CORE_ERROR("SceneImporter: Failed to load scene from '{0}'", metadata.FilePath);
            return nullptr;
        }

        scene->SetHandle(metadata.Handle);

        LF_CORE_INFO("SceneImporter: Loaded scene '{0}' from '{1}'", scene->GetName(), metadata.FilePath);
        return scene;
    }
}
```

**优点**：
- 复用现有 SceneSerializer 逻辑
- 实现简单
- 与 MaterialImporter 模式一致

### 方案 B：SceneImporter 直接解析 YAML

直接在 SceneImporter 中实现 YAML 解析，不依赖 SceneSerializer。

**优点**：完全解耦
**缺点**：代码重复，维护两份解析逻辑

### 方案推荐

| 方案 | 推荐度 | 理由 |
|------|--------|------|
| **方案 A：委托 SceneSerializer** | ??? 最优 | 复用现有逻辑，与其他 Importer 模式一致 |
| 方案 B：直接解析 | ? | 代码重复，维护成本高 |

**推荐方案 A**。

---

## 六、EditorLayer 改造

### 6.1 设计思路：不再需要 Save As

传统编辑器中 Save As 有两个用途：
1. 将当前场景保存到新路径（创建副本）
2. 新建的未保存场景首次指定保存路径

在纳入资产系统后，这两个用途都被覆盖：
- **用途 1（创建副本）**→ 通过 ProjectAssetsPanel 中「复制资产」功能实现（复制 .luck3d 文件并分配新 Handle）
- **用途 2（首次保存）**→ 不再存在，因为 `NewScene` 时就已经通过 `CreateAsset` 创建了文件，场景从诞生起就有路径和 Handle

因此，EditorLayer 只需要三个场景操作：
- **NewScene**：通过 `CreateAsset` 创建新场景文件
- **OpenScene**：通过 `ImportAsset` 注册已有文件 + `GetAsset<Scene>` 加载
- **SaveScene**：保存到已有路径（场景始终有路径）

### 6.2 方案 A：EditorLayer 通过 AssetManager 管理场景（? 推荐）

```cpp
// EditorLayer.h 新增
AssetHandle m_CurrentSceneHandle;  // 当前打开的场景 Handle

// EditorLayer.cpp 改造
void EditorLayer::NewScene()
{
    // 通过文件对话框指定保存路径（或使用默认路径）
    std::string filepath = FileDialogs::SaveFile("Scene (*.luck3d)\0*.luck3d\0");
    if (filepath.empty()) return;
    
    // 创建新的空场景
    Ref<Scene> scene = CreateRef<Scene>();
    scene->SetName("Untitled");
    
    // 计算相对路径
    std::filesystem::path relPath = std::filesystem::relative(filepath);
    std::string normalizedPath = relPath.generic_string();
    
    // 通过 CreateAsset 创建资产文件并注册到 Registry
    AssetManager::CreateAsset(scene, normalizedPath);
    
    // 序列化到文件
    SceneSerializer::Serialize(scene, filepath);
    
    m_Scene = scene;
    m_CurrentSceneHandle = scene->GetHandle();
}

void EditorLayer::OpenScene()
{
    std::string filepath = FileDialogs::OpenFile("Scene (*.luck3d)\0*.luck3d\0");
    if (filepath.empty()) return;
    
    OpenScene(filepath);
}

void EditorLayer::OpenScene(const std::string& filepath)
{
    // 注册到资产系统（如果尚未注册）
    std::filesystem::path relPath = std::filesystem::relative(filepath);
    std::string normalizedPath = relPath.generic_string();
    
    AssetHandle sceneHandle = AssetManager::ImportAsset(normalizedPath, AssetType::Scene);
    if (!sceneHandle.IsValid())
    {
        LF_CORE_ERROR("Failed to import scene: '{0}'", normalizedPath);
        return;
    }
    
    // 通过 AssetManager 加载场景
    // 注意：场景不使用缓存（每次打开都是新实例）
    AssetManager::UnloadAsset(sceneHandle);  // 清除旧缓存
    Ref<Scene> scene = AssetManager::GetAsset<Scene>(sceneHandle);
    if (!scene)
    {
        LF_CORE_ERROR("Failed to load scene asset: '{0}'", normalizedPath);
        return;
    }
    
    m_Scene = scene;
    m_CurrentSceneHandle = sceneHandle;
}

void EditorLayer::SaveScene()
{
    // 场景始终有路径（NewScene 时已通过 CreateAsset 创建文件）
    // 因此不需要判断 Handle 是否有效后跳转到 SaveAs
    const std::string& filepath = AssetManager::GetAssetFilePath(m_CurrentSceneHandle);
    std::string absolutePath = std::filesystem::absolute(filepath).string();
    
    SceneSerializer::Serialize(m_Scene, absolutePath);
}
```

**优点**：
- 场景通过 AssetManager 统一管理
- 场景文件自动注册到 Registry
- 为后续 ProjectAssetsPanel 双击打开场景奠定基础
- 无 SaveAs 逻辑，流程更简洁

**缺点**：
- 场景是重量级对象，缓存策略需要特殊处理（每次打开新实例）

### 6.3 方案 B：EditorLayer 仅注册，不通过 AssetManager 加载

```cpp
void EditorLayer::OpenScene(const std::string& filepath)
{
    // 仅注册到 Registry（获取 Handle）
    AssetHandle sceneHandle = AssetManager::ImportAsset(normalizedPath, AssetType::Scene);
    
    // 仍然使用 SceneSerializer 直接加载（不经过 AssetManager 缓存）
    m_Scene = CreateRef<Scene>();
    SceneSerializer::Deserialize(m_Scene, absolutePath);
    m_Scene->SetHandle(sceneHandle);
    
    m_CurrentSceneHandle = sceneHandle;
}
```

**优点**：改动最小，不需要处理场景缓存问题
**缺点**：加载不经过 AssetManager，与其他资产不一致

### 6.4 方案推荐

| 方案 | 推荐度 | 理由 |
|------|--------|------|
| **方案 A：完全通过 AssetManager** | ??? 最优 | 统一管理，架构一致，为后续功能奠定基础 |
| 方案 B：仅注册 | ?? | 改动小但架构不一致 |

**推荐方案 A**。注意场景加载时需要先 `UnloadAsset` 清除旧缓存，确保每次打开都是新实例。

---

## 七、场景注册到 Registry

场景文件在以下时机注册到 Registry：

1. **新建场景**（NewScene → CreateAsset）：创建时自动注册
2. **打开场景**（OpenScene → ImportAsset）：打开时如果未注册则自动注册
3. **AssetManager::Init**：启动时从 `.lcr` 文件恢复已注册的场景

场景文件中需要记录自身的 Handle：

```yaml
# .luck3d 文件新增字段
Scene: Main Scene
Handle: 7284619502847361    # 新增：场景自身的 AssetHandle
EnvironmentSettings:
  ...
```

---

## 八、序列化器设计分析：静态 vs 非静态

### 8.1 当前现状对比

| 特征 | SceneSerializer | MaterialSerializer |
|------|----------------|-------------------|
| **方法类型** | 非静态（实例方法） | 全静态 |
| **构造方式** | `SceneSerializer(Ref<Scene>)` | 无需构造 |
| **调用方式** | `serializer.Serialize(path)` | `MaterialSerializer::SerializeToFile(material, path)` |
| **状态** | 持有 `m_Scene` 成员 | 无状态 |
| **数据传递** | 构造时传入 Scene | 每次调用时传入 Material |

### 8.2 方案分析

#### 方案 A：统一为全静态方法（MaterialSerializer 风格）

```cpp
class SceneSerializer
{
public:
    static void Serialize(const Ref<Scene>& scene, const std::string& filepath);
    static bool Deserialize(Ref<Scene>& scene, const std::string& filepath);
};
```

**优点**：
- 无状态，线程安全
- 调用简洁：`SceneSerializer::Serialize(scene, path)`
- 与 MaterialSerializer 风格统一
- 不需要创建临时对象
- 语义清晰：序列化器是"工具"，不是"绑定到某个对象的服务"

**缺点**：
- 如果序列化过程中需要中间状态（如进度回调、错误收集），静态方法不方便
- 参数列表可能较长（如果未来需要传入配置选项）

#### 方案 B：统一为非静态方法（SceneSerializer 风格）

```cpp
class MaterialSerializer
{
public:
    MaterialSerializer(const Ref<Material>& material);
    bool SerializeToFile(const std::string& filepath);
    static Ref<Material> DeserializeFromFile(const std::string& filepath);
private:
    Ref<Material> m_Material;
};
```

**优点**：
- 可以在对象中保存中间状态
- 面向对象风格，符合"序列化器绑定到目标对象"的心智模型
- 如果未来需要配置选项（如序列化格式版本、压缩选项），可以通过成员变量设置

**缺点**：
- 需要创建临时对象（`SceneSerializer serializer(scene); serializer.Serialize(path);`）
- 对于无状态操作来说是不必要的开销
- 反序列化时语义尴尬：先创建空对象再填充，还是静态方法返回新对象？

#### 方案 C：保持现状不统一

**优点**：无改动成本
**缺点**：代码风格不一致，新开发者困惑

### 8.3 是否需要提取序列化基类

考虑提取一个 `AssetSerializer` 基类：

```cpp
class AssetSerializer
{
public:
    virtual ~AssetSerializer() = default;
    virtual bool Serialize(const Ref<Asset>& asset, const std::string& filepath) = 0;
    virtual Ref<Asset> Deserialize(const std::string& filepath) = 0;
};

class SceneSerializer : public AssetSerializer { ... };
class MaterialSerializer : public AssetSerializer { ... };
class MeshSerializer : public AssetSerializer { ... };
```

#### 分析

| 维度 | 分析 |
|------|------|
| **共性** | 都有 Serialize/Deserialize 方法，都操作文件路径 |
| **差异** | 格式完全不同（YAML vs 二进制）；参数类型不同（Scene vs Material vs Mesh）；内联序列化只有 Material 需要 |
| **多态使用场景** | 几乎没有——不会出现"遍历一组 AssetSerializer 并调用 Serialize"的场景 |
| **与 AssetImporter 的关系** | AssetImporter 已经承担了"通过类型分发加载逻辑"的职责，Serializer 基类会与之重叠 |

**结论：不需要提取基类。**

理由：
1. **没有多态使用场景**——不会有代码需要通过基类指针调用序列化方法
2. **与 AssetImporter 职责重叠**——AssetImporter 已经是"按类型分发加载"的抽象层
3. **各序列化器差异大于共性**——格式不同、参数不同、附加功能不同（Material 有内联模式）
4. **强制统一接口会导致不自然的设计**——如 MeshSerializer 是二进制格式，与 YAML 序列化器共享基类没有意义
5. **YAGNI 原则**——当前没有需要基类的具体需求

### 8.4 关于进度回调的补充分析

有人可能担心：全静态方法是否会影响未来的进度回调需求（如资产导入进度框）？

**结论：不会影响。** 理由如下：

1. **进度回调的主要需求是批量级别的**（"正在导入第 3/10 个资产..."），这由 Import Pipeline / AssetManager 管理，与序列化器是否静态无关
2. **即使需要单资产细粒度进度**，静态方法通过添加可选回调参数即可解决：
   ```cpp
   static Ref<Mesh> MeshSerializer::Deserialize(
       const std::string& filepath, 
       ProgressCallback callback = nullptr);  // 可选参数
   ```
3. **Unity 的做法**：Unity 的 `EditorUtility.DisplayProgressBar` 也不是由序列化器本身驱动的，而是由外部的 Import Pipeline 驱动
4. **如果未来需要复杂状态管理**（暂停/恢复/取消/重试），应引入独立的 `ImportContext` 对象，而非让序列化器变成有状态的：
   ```cpp
   struct ImportContext
   {
       ProgressCallback OnProgress;
       CancellationToken CancelToken;
       std::vector<std::string> Warnings;
   };
   
   // 序列化器仍然是静态的，但接受 Context 参数
   static Ref<Mesh> MeshSerializer::Deserialize(
       const std::string& filepath, 
       ImportContext* context = nullptr);
   ```

### 8.5 结论与推荐

| 决策 | 推荐 | 理由 |
|------|------|------|
| **统一风格** | ??? 方案 A：全静态 | 序列化器本质是无状态工具函数，静态方法最自然 |
| **提取基类** | ? 不需要 | 没有多态使用场景，差异大于共性，与 AssetImporter 重叠 |
| **进度回调** | ? 不受影响 | 进度在 Import Pipeline 层管理，静态方法可通过可选参数扩展 |

**具体建议**：

将 `SceneSerializer` 改为全静态方法，与 `MaterialSerializer` 风格统一：

```cpp
class SceneSerializer
{
public:
    /// <summary>
    /// 将场景序列化到 .luck3d 文件
    /// </summary>
    static void Serialize(const Ref<Scene>& scene, const std::string& filepath);

    /// <summary>
    /// 从 .luck3d 文件反序列化场景
    /// </summary>
    /// <param name="scene">目标场景（将被填充数据）</param>
    /// <param name="filepath">文件路径</param>
    /// <returns>是否成功</returns>
    static bool Deserialize(const Ref<Scene>& scene, const std::string& filepath);

    /// <summary>
    /// 运行时序列化：序列化为二进制（预留）
    /// </summary>
    static void SerializeRuntime(const Ref<Scene>& scene, const std::string& filepath);

    /// <summary>
    /// 运行时反序列化：二进制反序列化（预留）
    /// </summary>
    static bool DeserializeRuntime(const Ref<Scene>& scene, const std::string& filepath);
};
```

**改造影响**：
- `SceneSerializer` 移除构造函数和 `m_Scene` 成员
- 所有方法改为 `static`，第一个参数传入 `const Ref<Scene>&`
- 调用方从 `SceneSerializer serializer(scene); serializer.Serialize(path);` 改为 `SceneSerializer::Serialize(scene, path);`
- `SceneImporter::Load` 中调用方式相应调整

---

## 九、涉及的文件清单

| 文件路径 | 修改内容 |
|---------|----------|
| `Lucky/Source/Lucky/Scene/Scene.h` | 继承 Asset，实现 `GetAssetType()` |
| `Lucky/Source/Lucky/Asset/SceneImporter.h`（新建） | SceneImporter 声明 |
| `Lucky/Source/Lucky/Asset/SceneImporter.cpp`（新建） | SceneImporter 实现 |
| `Lucky/Source/Lucky/Asset/AssetManager.cpp` | 注册 SceneImporter + 添加 `GetAsset<Scene>` 模板实例化 |
| `Lucky/Source/Lucky/Serialization/SceneSerializer.h` | 改为全静态方法 |
| `Lucky/Source/Lucky/Serialization/SceneSerializer.cpp` | 改为全静态实现 + 序列化时写入 Handle 字段 |
| `Luck3DApp/Source/EditorLayer.h` | 新增 `m_CurrentSceneHandle` |
| `Luck3DApp/Source/EditorLayer.cpp` | NewScene/Open/Save 改造（移除 SaveAs） |

---

## 十、分步实施策略

| 步骤 | 内容 | 依赖 | 预估工作量 |
|------|------|------|-----------|
| Step 1 | Scene.h 继承 Asset，实现 GetAssetType() | 无 | 极小 |
| Step 2 | SceneSerializer 改为全静态方法 | 无 | 小 |
| Step 3 | 创建 SceneImporter.h/cpp | Step 1, 2 | 小 |
| Step 4 | AssetManager.cpp 注册 SceneImporter + 模板实例化 | Step 3 | 极小 |
| Step 5 | SceneSerializer.cpp 序列化时写入 Handle 字段 | Step 1 | 极小 |
| Step 6 | EditorLayer 改造 NewScene/Open/Save（移除 SaveAs） | Step 4, 5 | 中 |
| Step 7 | 编译测试 + 验证 | 全部 | 小 |

---

## 十一、验证清单

| # | 验证项 | 预期结果 |
|---|--------|--------|
| 1 | 编译通过 | 无编译错误 |
| 2 | 新建场景 | 通过 CreateAsset 创建 .luck3d 文件，自动注册到 Registry |
| 3 | 打开场景 | 场景正确加载，自动注册到 Registry |
| 4 | 保存场景 | .luck3d 文件中包含 Handle 字段，内容正确更新 |
| 5 | 重启后打开场景 | Registry 中保留场景注册信息 |
| 6 | Assets.lcr 内容 | 包含场景文件的注册条目（Type: Scene） |

---

## 十二、已知限制与后续扩展

| 限制 | 影响 | 后续优化方向 |
|------|------|-------------|
| Scene 缓存策略简单 | 每次打开都重新加载 | 后续可添加场景预加载/后台加载 |
| 无多场景编辑 | 一次只能打开一个场景 | 后续可支持多场景标签页 |
| 无 Prefab 引用 | 场景不能引用子场景 | 后续 Prefab 系统需要此基础 |
