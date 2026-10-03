# Phase 2.1：脚本成为资产（.cs → AssetType::Script）

## 1. 概述

让 `.cs` 文件进入资产系统：Project 面板可见、有类型图标、可被拖拽、随 Refresh / 重命名 / 删除正确同步。

本 Phase **只做"成为资产"**：加一个资产类型、一个资产类、一个 Importer。**不碰 `ScriptComponent`**，不做 `.cs` → 类的解析（那是 P2.2），不做 Inspector 控件（那是 P2.3）。

### 1.1 关键约束

- **`AssetType` 要改四处，少一处就会出问题**：
  1. 枚举本体加 `Script`
  2. `AssetTypeToString` 加分支 —— Registry 持久化时把类型写成字符串，漏了会写成 `"None"`
  3. `StringToAssetType` 加分支 —— 读回时漏了会解析成 `None`，重启后该资产类型丢失
  4. `GetAssetTypeFromExtension` 加 `.cs` —— `AssetManager::Refresh()` 的 `ScanDirectory` 会**跳过无法识别扩展名的文件**，漏了脚本根本不会进 Registry
- **`AssetManager.cpp` 里必须补两处，缺一不可**：
  1. **`GetExpectedAssetType<Script>()` 特化** —— 不补的话 `AssetManager::GetAsset<Script>(handle)` 会命中 `static_assert(sizeof(T) == 0, "Unsupported asset type")`，**编译期**就报错。
  2. **文件末尾「显式实例化模板」清单里加一行**：`template Ref<Script> AssetManager::GetAsset<Script>(AssetHandle handle);`。原因是 `GetAsset<T>` 的**定义在 `.cpp` 内部**（不是头文件），别的编译单元要用就必须显式实例化。**漏了它编译期不报错，只在链接期炸**（`LNK2019: 无法解析的外部符号 ... GetAsset<class Lucky::Script>`），而且**不会立刻暴露** —— 只有真的有人调 `GetAsset<Script>` 时才出现。已知会触发它的有两处：P2.3 的 `Deserialize_Script`，以及 `UI::PropertyAsset<Script>`（`UI/PropertyGrid.h` 内部就调了 `AssetManager::GetAsset<T>`）。
- **`Script` 不支持 Save**：脚本内容是用户写的，引擎不负责序列化它。但 `AssetManager::SaveAssetToFile` 会按类型找 Importer 并调用 `Save`，所以 `ScriptImporter::Save` 要**显式返回 `true`**（表示"无需保存，视为成功"），而不是用基类默认的 `false`（会被当成保存失败）。详见决策点 4.4。
- **路径解析不依赖 cwd**：一律走 `Project::GetActive()->ResolveAbsolute(metadata.FilePath)`（编码规范 §13.10）。
- 代码风格一律遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：4 空格缩进、Allman 花括号、控制语句强制花括号、公有接口写 `/// <summary>` 中文注释、智能指针只用 `Ref` / `Scope`。

### 1.2 前置条件

| 依赖 | 说明 |
|------|------|
| `AssetManager` | 已就绪（含 `AssetImporter` / `AssetRegistry` / `AssetMetadata` / `AssetHandle`） |
| `AssetManager::Refresh` | 已就绪，且 `ScanDirectory` 用 `GetAssetTypeFromExtension` 过滤 |
| `UI::PropertyAsset<T>` | 已就绪（`UI/PropertyGrid.h`），要求 `T : Asset` 且提供 `static AssetType StaticAssetType()` —— 这是 P2.3 能白拿拖拽的前提 |
| 项目文件 | `Luck3DApp/Project/Assets/Scripts/PlayerController.cs` 已存在 |

### 1.3 本 Phase **不做**的事

- 不改 `ScriptComponent`（仍是 `std::string ClassName`）
- 不做 `.cs` → `ScriptClass` 的解析（P2.2）
- 不做脚本编译 / 热重载（P3）
- 不新增任何 Inspector 控件
- 不做 `.cs` 的 Save / 新建 / 编辑能力（脚本文件由外部编辑器维护）

---

## 2. 涉及的文件

### 2.1 新建

| 路径 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Asset/Script.h` | `class Script : public Asset` |
| `Lucky/Source/Lucky/Asset/ScriptImporter.h` | `class ScriptImporter : public AssetImporter` |
| `Lucky/Source/Lucky/Asset/ScriptImporter.cpp` | Importer 实现 |

### 2.2 修改

| 文件 | 改动 |
|------|------|
| `Lucky/Source/Lucky/Asset/AssetType.h` | 枚举 + `AssetTypeToString` + `StringToAssetType` + `GetAssetTypeFromExtension` |
| `Lucky/Source/Lucky/Asset/AssetManager.cpp` | 顶部 include `ScriptImporter.h`；`GetExpectedAssetType<Script>` 特化；`Init()` 里注册 Importer |
| `Lucky/Source/Lucky/Editor/EditorIconManager.cpp` | 注册 `AssetType::Script` 的类型图标 |

### 2.3 不修改

- `Asset/Asset.h` / `AssetImporter.h` / `AssetRegistry.*` / `AssetMetadata.h` / `AssetHandle.h`：接口已够用
- `Scene/Components/ScriptComponent.h`：P2.1 完全不感知
- `Scripting/*`：P2.1 不依赖脚本引擎
- 任何 `.cs` / `.lua`：不涉及

### 2.4 新增文件要不要重新生成工程

`Asset/Script.h` / `ScriptImporter.h` / `.cpp` 与现有代码同目录，premake 的 `files` 是目录通配，**但仍需重新跑一次 `Scripts/Setup-Windows.bat`** 让 `.vcxproj` 收录新文件，否则 VS 里看不到、也编不进去。

---

## 3. 现状回顾

### 3.1 `AssetType`（改造对象）

`Asset/AssetType.h` 当前形态：

```cpp
enum class AssetType : uint8_t
{
    None = 0,       // 无效类型
    Material,       // 材质（.lmat）
    Mesh,           // 网格（.lmesh）
    Texture2D,      // 2D 纹理（.png/.jpg/.tga/.bmp/.hdr）
    Scene,          // 场景（.luck3d）
    Shader          // 着色器（预留）
};

inline const char* AssetTypeToString(AssetType type)
{
    switch (type)
    {
        case AssetType::Material:   return "Material";
        // ...
        default:                    return "None";
    }
}

inline AssetType StringToAssetType(const std::string& str)
{
    if (str == "Material")  return AssetType::Material;
    // ...
    return AssetType::None;
}

inline AssetType GetAssetTypeFromExtension(const std::string& extension)
{
    if (extension == ".lmat")  { return AssetType::Material; }
    if (extension == ".lmesh") { return AssetType::Mesh; }
    // 纹理 / 场景 / shader 同理
    return AssetType::None;
}
```

**注意**：`AssetType` 是 `uint8_t` 枚举，但在 Registry 里是**按字符串**持久化的（`AssetTypeToString` / `StringToAssetType`），所以新增枚举值**不需要迁移**旧的 `AssetRegistry.lcr`。

### 3.2 `AssetImporter` 接口

`Asset/AssetImporter.h`：

```cpp
class AssetImporter
{
public:
    virtual ~AssetImporter() = default;

    /// <summary>
    /// 从磁盘加载资产
    /// </summary>
    /// <returns>加载的资产实例（通过 Ref<void> 类型擦除），失败返回 nullptr</returns>
    virtual Ref<void> Load(const AssetMetadata& metadata) = 0;

    /// <summary>
    /// 将资产保存到磁盘（可选实现）
    /// </summary>
    virtual bool Save(const Ref<Asset>& asset, const std::string& filepath) { return false; }
};
```

`Save` 有默认实现（返回 `false`），子类可以只实现 `Load`。

### 3.3 `AssetManager` 的两处注册点

`Asset/AssetManager.cpp`：

```cpp
    // ---- 类型映射辅助 ----
    namespace
    {
        template<typename T>
        AssetType GetExpectedAssetType()
        {
            static_assert(sizeof(T) == 0, "Unsupported asset type");    // 未特化的类型：编译期拦下
            return AssetType::None;
        }

        template<> AssetType GetExpectedAssetType<Material>() { return AssetType::Material; }
        template<> AssetType GetExpectedAssetType<Mesh>() { return AssetType::Mesh; }
        template<> AssetType GetExpectedAssetType<Texture2D>() { return AssetType::Texture2D; }
        template<> AssetType GetExpectedAssetType<Scene>() { return AssetType::Scene; }
    }

    void AssetManager::Init()
    {
        // 注册 Importers
        s_Data.Importers[AssetType::Material] = CreateScope<MaterialImporter>();
        s_Data.Importers[AssetType::Mesh] = CreateScope<MeshImporter>();
        s_Data.Importers[AssetType::Texture2D] = CreateScope<TextureImporter>();
        s_Data.Importers[AssetType::Scene] = CreateScope<SceneImporter>();
        // ...
    }
```

加载分发：

```cpp
    Ref<void> AssetManager::LoadAsset(const AssetMetadata& metadata)
    {
        auto it = s_Data.Importers.find(metadata.Type);
        if (it == s_Data.Importers.end())
        {
            LF_CORE_ERROR("AssetManager::LoadAsset - No importer registered for type: {0}", AssetTypeToString(metadata.Type));
            return nullptr;
        }

        return it->second->Load(metadata);
    }
```

保存分发：

```cpp
    bool AssetManager::SaveAssetToFile(const Ref<Asset>& asset, const std::string& absolutePath)
    {
        AssetType type = asset->GetAssetType();

        auto it = s_Data.Importers.find(type);
        if (it == s_Data.Importers.end())
        {
            LF_CORE_WARN("AssetManager::SaveAssetToFile - No importer registered for type: {0}, skipping save.", AssetTypeToString(type));
            return true;    // 无 Importer 视为"无需保存"，返回成功
        }

        return it->second->Save(asset, absolutePath);   // ← 注册了 Importer 就会走到这里
    }
```

**这两段决定了两个必须做的动作**：`GetExpectedAssetType<Script>` 要特化（否则 `GetAsset<Script>` 编译不过），`ScriptImporter::Save` 要覆盖（否则走基类默认的 `false`，返回"保存失败"）。

### 3.4 现有 Importer 参考

`Asset/SceneImporter.cpp`（最简形态，无 Save 之外的依赖）：

```cpp
Ref<void> SceneImporter::Load(const AssetMetadata& metadata)
{
    std::string absolutePath = Project::GetActive()->ResolveAbsolute(metadata.FilePath).string();

    Ref<Scene> scene = CreateRef<Scene>();

    if (!SceneSerializer::Deserialize(scene, absolutePath))
    {
        LF_CORE_ERROR("SceneImporter: Failed to load scene from '{0}'", metadata.FilePath);
        return nullptr;
    }

    LF_CORE_INFO("SceneImporter: Loaded scene '{0}' from '{1}'", scene->GetName(), metadata.FilePath);
    return scene;
}
```

### 3.5 资产类的既定形态

`Renderer/Material.h` / `Renderer/Mesh.h` / `Renderer/Texture.h` / `Scene/Scene.h` 统一提供：

```cpp
static AssetType StaticAssetType() { return AssetType::Material; }
```

`PropertyGrid.h` 的 `PropertyAsset<T>` 靠它做拖拽类型校验：

```cpp
template<typename T>
    requires std::is_base_of_v<Asset, T>
bool PropertyAsset(const char* label, Ref<T>& assetRef)
{
    AssetType assetType = T::StaticAssetType();
    // ...
    if (AssetManager::GetAssetType(handle) == assetType)   // 目标端按类型过滤
    {
        // ...
    }
}
```

`Asset` 基类（`Asset/Asset.h`）提供 `GetHandle()` / `GetName()` / `SetName()` / 纯虚 `GetAssetType()`。

### 3.6 图标机制

`Editor/EditorIconManager.cpp`：

```cpp
s_IconData.AssetTypeIcons[AssetType::Material]  = LoadIcon("Asset/Material.png");
s_IconData.AssetTypeIcons[AssetType::Mesh]      = LoadIcon("Asset/Mesh.png");
s_IconData.AssetTypeIcons[AssetType::Texture2D] = LoadIcon("Asset/Texture.png");
s_IconData.AssetTypeIcons[AssetType::Scene]     = LoadIcon("Asset/Scene.png");
s_IconData.AssetTypeIcons[AssetType::Shader]    = LoadIcon("Asset/Shader.png");
```

查询接口**有兜底**，未知类型不会崩：

```cpp
const Ref<Texture2D>& EditorIconManager::GetAssetTypeIcon(AssetType type)
{
    auto it = s_IconData.AssetTypeIcons.find(type);
    if (it != s_IconData.AssetTypeIcons.end() && it->second)
    {
        return it->second;
    }

    return s_IconData.FileIcon;    // 兜底：通用文件图标
}
```

现有图标文件（`Luck3DApp/Resources/Icons/`）里**没有** `Asset/Script.png`，但**有** `Component/Script.png`。

---

## 4. 关键设计决策（多方案对比）

### 4.1 决策点 1：要不要引入 `Script` 资产类

#### 方案 A：引入 `class Script : public Asset`（**推荐 ✅**）

- **优点**：
  1. `UI::PropertyAsset<T>` 要求 `T : Asset` 且提供 `StaticAssetType()` —— 有了 `Script` 类，P2.3 的 Inspector 拖拽**零新代码**
  2. 与 `Material` / `Mesh` / `Texture2D` / `Scene` 的既有形态完全一致，`Ref<Script>` 在 `Scene::Copy` 时按现有规则共享，运行态不失效
  3. `Script` 对象天然是"这个 `.cs` 的运行时代表"，后续要挂类名解析结果、字段元信息缓存都有地方放
- **缺点**：多一个（很薄的）类和一个 Importer

#### 方案 B：不加资产类，`ScriptComponent` 只存裸 `AssetHandle`

- **优点**：少写两个文件
- **缺点**：
  1. `PropertyAsset<T>` 用不了（模板约束 `T : Asset`），得**另写一套只吃 `AssetHandle` 的拖拽控件**，等于把 P2.3 白拿的拖拽代价还回去
  2. 没有载体缓存"这个 `.cs` 对应哪个类"
  3. `Scene.h` 明确写了「Asset Handle：副本为无效 Handle（副本不进入 AssetRegistry）」—— 裸 handle 在运行态场景里**取不到**，脚本根本没法实例化

#### 方案 C：`ScriptComponent` 直接存文件路径字符串，绕开资产系统

- **优点**：最简单
- **缺点**：Project 面板拖不进来（拖拽 payload 是 `AssetHandle`，不是路径）；路径改名即断链；与 `AssetManager` 的 Handle 体系割裂。**直接违背 P2 的目标，否决。**

**结论：方案 A。** 它是"能白拿拖拽 + 与现有资产一致 + 运行态可用"的唯一选择。

---

### 4.2 决策点 2：`Script` 资产对象携带什么信息

#### 方案 A：只继承基类（`Asset` 的 Handle + Name），什么都额外不存

- **优点**：最薄，职责最干净
- **缺点**：使用者每次都要 `AssetManager::GetAssetFilePath(handle)` 再自己截取文件名，重复劳动；`AssetField` 显示名字也会是空（因为没人调 `SetName`）

#### 方案 B：构造时由 Importer 传入"文件名词干"，存为 `ClassName`，同时 `SetName` 同名（**推荐 ✅**）

- **优点**：
  1. 和 `TextureImporter` 的做法一致 —— 那个注释写着「图片文件本身不携带 Asset 名字信息，此处兜底以供 Inspector / AssetField 显示」，脚本同理：`.cs` 本身没有内部名称，**文件名就是它的名字**
  2. 后续 P2.2 的类解析直接拿 `script->GetClassName()` 去匹配，不需要回查 Registry
- **缺点**：`ClassName` 是"文件名派生的猜测"，不保证真的存在这个类。**用命名和注释把语义讲清**（`GetClassName()` 文档注明"取自文件名词干，是否真的存在该类由 ScriptEngine 校验"），不要叫成 `GetResolvedClassName` 之类看起来已验证的名字。

#### 方案 C：存源码绝对路径

- **缺点**：违反规范 §13.10（资产内部一律存"相对项目根"），且路径随时可通过 `AssetMetadata` 拿到，冗余。

**结论：方案 B。**

---

### 4.3 决策点 3：`ScriptImporter::Load` 要不要读文件内容

#### 方案 A：不读，只根据 `metadata.FilePath` 造一个 `Script` 对象（**推荐 ✅**）

```cpp
Ref<void> ScriptImporter::Load(const AssetMetadata& metadata)
{
    std::string className = std::filesystem::path(metadata.FilePath).stem().string();
    return CreateRef<Script>(className);
}
```

- **优点**：
  1. `CreateAsset` / `EnsureAsset` 之外的**所有**加载路径都只关心"这个资产是什么"，不关心"它内容对不对"。`.cs` 的语义校验（有没有继承 `Entity`、类名对不对）属于脚本引擎的职责（P2.2），放在 Importer 里会形成 `Asset → Scripting` 的反向依赖
  2. 零 IO，不会因为文件被外部占用/临时无权限而加载失败
- **缺点**：文件不存在时也能"加载成功"（返回一个空壳 `Script`）。可接受 —— Registry 里的条目本来就来自磁盘扫描，文件消失会在下次 `Refresh` 时被 Unregister

#### 方案 B：Load 里读文件、扫 `class XXX : Entity` 做校验

- **优点**：拖入时就能报"这个脚本没继承 Entity"
- **缺点**：
  1. 需要**写一个 C# 语法解析器**才能可靠地找类声明 —— 正则匹配会在注释、字符串、嵌套类、泛型上出错
  2. 形成 `Asset → Scripting` 依赖，层级倒挂
  3. 真正的权威判断依据是**已编译的程序集**（`mono_class_is_subclass_of`），不是源码文本；源码对了但没编译照样跑不了
- **否决**。

#### 方案 C：Load 里读文件、算个 hash 存进 `Script`（为热重载铺路）

- **优点**：P3 的热重载要监听 `.cs` 变更，hash 是标准做法
- **缺点**：**现在没有消费者**，属于为未来预铺路。P3 真做的时候加在 `Script` 上是几行的事，届时信息更全（知道要什么粒度）。**本 Phase 不做。**

**结论：方案 A。**

---

### 4.4 决策点 4：`.cs` 支不支持 `Save`

#### 方案 A：不覆盖 `Save`，用基类默认的 `return false`

- **优点**：零代码
- **缺点**：`AssetManager::SaveAssetToFile` 会因为注册了 Importer 而走到 `Save`，拿到 `false` 当作**保存失败**。将来任何"保存资产"的批量路径（比如场景保存时顺带保存引用到的资产）碰到 Script 就会报错
- **结论**：不可取

#### 方案 B：覆盖 `Save`，直接 `return true`（**推荐 ✅**）

```cpp
bool ScriptImporter::Save(const Ref<Asset>& asset, const std::string& filepath)
{
    // 脚本内容由用户在外部编辑器维护，引擎不做序列化，视为保存成功
    return true;
}
```

- **优点**：语义正确 —— "无需保存"不等于"保存失败"。和 `SaveAssetToFile` 里"没注册 Importer 就 `return true`"的处理保持同一套语义
- **缺点**：无

#### 方案 C：覆盖 `Save`，打一条 `LF_CORE_WARN` 再 `return true`

- **优点**：多一层可观测性
- **缺点**：如果批量保存路径真的会碰它，会刷屏。**不必要。**

**结论：方案 B。**

---

### 4.5 决策点 5：图标怎么给

#### 方案 A：复用已有的 `Component/Script.png`（**推荐 ✅**）

```cpp
s_IconData.AssetTypeIcons[AssetType::Script] = LoadIcon("Component/Script.png");
```

- **优点**：**零新增二进制资源**。`Resources/Icons/` 下现成有这张图（现在给 ScriptComponent 用），语义一致
- **缺点**：资产图标与组件图标长得一样。在 Project 面板里看是"脚本文件"，在 Inspector 里看是"脚本组件"，两者用同一张图**并不冲突**

#### 方案 B：新增 `Resources/Icons/Asset/Script.png`

- **优点**：可以做成和组件图标不同的样式
- **缺点**：需要一张**二进制图片**，得你手动放进仓库，我（或 AI）没法生成。**作为可选增强，不阻塞本 Phase**

#### 方案 C：什么都不做

- **优点**：`GetAssetTypeIcon` 有兜底，显示通用文件图标，不崩
- **缺点**：Project 面板里脚本文件和普通文件长得一样，体验差。**不如 A。**

**结论：方案 A 打底，B 作为可选增强。**

---

### 4.6 决策点 6：`Script.h` 放哪个目录

#### 方案 A：`Lucky/Source/Lucky/Asset/Script.h`（**推荐 ✅**）

- **优点**：
  1. `Script` 是一个**纯资产描述符**（Handle + 名字 + 类名词干），不含任何脚本运行时逻辑
  2. `Asset/` 已经是资产层（`Asset.h` / `AssetImporter.h` / `AssetRegistry.h` 都在这），资产类型的描述符放这里最自然
  3. `Scene/Components/ScriptComponent.h` 将来 include 它时，拉进来的是低层 Asset 头，不会把脚本引擎的头带进场景层
- **缺点**：与"资产类跟着自己的领域走"（`Material` 在 `Renderer/`、`Scene` 在 `Scene/`）这条惯例略有出入

#### 方案 B：`Lucky/Source/Lucky/Scripting/Script.h`

- **优点**：贴合"资产类跟着领域走"的惯例，`Script` 的领域就是脚本
- **缺点**：`Scripting/` 目前装的全是**运行时**代码（`ScriptEngine` / `ScriptGlue` / `ScriptClass`），塞一个纯资产描述符进去会让这一层的职责变模糊
- **可选**：如果你更认同"跟着领域走"，选 B 也没问题，只是本 Phase 之后所有文档里的路径都要跟着改

**结论：方案 A。**

---

## 5. 实现步骤（按依赖顺序）

每步完成后 `Lucky` 与 `Luck3DApp` 都应能编译通过。

### Step 1：扩展 `AssetType.h`

**文件**：`Lucky/Source/Lucky/Asset/AssetType.h`

**1) 枚举加一项**：

```cpp
    enum class AssetType : uint8_t
    {
        None = 0,       // 无效类型
        Material,       // 材质（.lmat）
        Mesh,           // 网格（.lmesh）
        Texture2D,      // 2D 纹理（.png/.jpg/.tga/.bmp/.hdr）
        Scene,          // 场景（.luck3d）
        Shader,         // 着色器（预留）
        Script          // 脚本（.cs）
    };
```

**要点**：新值加在**末尾**，不打乱既有取值。虽然 Registry 是按字符串持久化（不怕数值变化），但保持追加习惯没有坏处。

**2) `AssetTypeToString` 加分支**：

```cpp
            case AssetType::Shader:     return "Shader";
            case AssetType::Script:     return "Script";
            default:                    return "None";
```

**3) `StringToAssetType` 加分支**：

```cpp
        if (str == "Shader")    return AssetType::Shader;
        if (str == "Script")    return AssetType::Script;
        return AssetType::None;
```

**4) `GetAssetTypeFromExtension` 加分支**：

```cpp
        // 脚本
        if (extension == ".cs")
        {
            return AssetType::Script;
        }

        return AssetType::None;
```

**⚠️ 大小写**：现有实现全是**区分大小写**的（`".lmat"` / `".png"` ...）。`.cs` 延续同一约定，不额外做小写化。若将来要支持 `.CS`，应作为**全局**改动统一处理，不要只给脚本开特例。

**5) 全局检查有没有漏掉的映射点**：

在仓库里搜 `AssetType::`，确认没有别的地方做了"类型 → 行为"的硬编码 switch（例如某个面板的"新建资产"菜单、图标映射、扩展名反向映射）需要补 `Script`。

```bash
# 用编辑器搜索（Grep）执行，确认每一处出现都是"枚举列举"而不是"缺 Script 分支会错的行为"
AssetType::
```

**这一步的产出**：`.cs` 已被识别为 `AssetType::Script`，`Refresh()` 会把它注册进 Registry。

---

### Step 2：新建 `Script.h`

**文件**：`Lucky/Source/Lucky/Asset/Script.h`

```cpp
#pragma once

#include "Asset.h"

#include <string>

namespace Lucky
{
    /// <summary>
    /// 脚本资产：代表项目里一个 .cs 文件
    /// 只承载"这个脚本叫什么"，不做类解析与实例化（那是 ScriptEngine 的职责）
    /// </summary>
    class Script : public Asset
    {
    public:
        /// <summary>
        /// 构造脚本资产
        /// </summary>
        /// <param name="className">文件名词干（如 "PlayerController"），同时作为资产显示名</param>
        Script(const std::string& className)
            : m_ClassName(className)
        {
            SetName(className);
        }

        static AssetType StaticAssetType() { return AssetType::Script; }

        AssetType GetAssetType() const override { return AssetType::Script; }

        /// <summary>
        /// 获取类名词干（取自文件名，如 "PlayerController"）
        /// 该名字是否对应程序集里一个真实存在的类，由 ScriptEngine 校验
        /// </summary>
        const std::string& GetClassName() const { return m_ClassName; }
    private:
        std::string m_ClassName;    // 文件名词干
    };
}
```

**要点**：

- 遵循规范 §6.1：有封装（private 成员）用 `class`；§6.2 访问修饰符顺序 public → private
- `StaticAssetType()` 是**非 const 静态函数**，和其他四个资产类保持逐字一致（`PropertyAsset<T>` 就是这么调的）
- `SetName(className)` 基类调用，保证 `AssetField` 能显示名字（参考 `TextureImporter` 里的同样做法）
- 构造函数**不用** `explicit`：单参数字符串构造允许隐式转换的风险很低，且其他资产类也没加
- 注释明确写清 `GetClassName()` 的语义边界（"是否真的存在该类由 ScriptEngine 校验"），避免被误当成已验证结果

---

### Step 3：新建 `ScriptImporter.h` / `.cpp`

**文件**：`Lucky/Source/Lucky/Asset/ScriptImporter.h`

```cpp
#pragma once

#include "AssetImporter.h"

namespace Lucky
{
    /// <summary>
    /// 脚本导入器：把 .cs 文件注册为 Script 资产
    /// 不读取源码内容，也不做 Save（脚本内容由用户在外部编辑器维护）
    /// </summary>
    class ScriptImporter : public AssetImporter
    {
    public:
        Ref<void> Load(const AssetMetadata& metadata) override;

        bool Save(const Ref<Asset>& asset, const std::string& filepath) override;
    };
}
```

**文件**：`Lucky/Source/Lucky/Asset/ScriptImporter.cpp`

```cpp
#include "lcpch.h"
#include "ScriptImporter.h"

#include "Lucky/Asset/Script.h"

#include <filesystem>

namespace Lucky
{
    Ref<void> ScriptImporter::Load(const AssetMetadata& metadata)
    {
        // 路径契约：metadata.FilePath 是"相对项目根"（规范 §13.10）
        // 这里只取文件名，不需要转绝对路径，因此不引入 Project 依赖
        std::string className = std::filesystem::path(metadata.FilePath).stem().string();

        LF_CORE_TRACE("ScriptImporter: Registered script '{0}'", className);
        return CreateRef<Script>(className);
    }

    bool ScriptImporter::Save(const Ref<Asset>& asset, const std::string& filepath)
    {
        // 脚本内容由用户在外部编辑器维护，引擎不做序列化，视为保存成功
        return true;
    }
}
```

**要点**：

- include 顺序遵循规范 §3.3：`lcpch.h` → 自己的头 → 项目头 → 标准库
- `Load` **不需要** `ResolveAbsolute`，因为它不碰磁盘。**不要**为了"看起来完整"而加一个用不上的绝对路径转换 —— 那会平白引入 `Project.h` 依赖
- `Save` 的参数 `asset` / `filepath` 未使用，**不要**为了让编译器闭嘴而写 `(void)asset;` —— MSVC 未使用**函数参数**不产生警告（只有未使用**局部变量**才警告）。若你所在配置确实报 C4100，用 `[[maybe_unused]]` 加在参数上，而不是 `(void)` 强转
- `LF_CORE_TRACE` 而不是 `INFO`：启动时每个 `.cs` 都会 Load 一次，用 TRACE 级别避免刷屏

---

### Step 4：改 `AssetManager.cpp`

**文件**：`Lucky/Source/Lucky/Asset/AssetManager.cpp`

**1) 顶部 include 区加一行**（和 `MaterialImporter.h` 等同组）：

```cpp
#include "ScriptImporter.h"
```

**2) `GetExpectedAssetType` 特化区加一行**：

```cpp
        template<> AssetType GetExpectedAssetType<Scene>() { return AssetType::Scene; }
        template<> AssetType GetExpectedAssetType<Script>() { return AssetType::Script; }
```

**要点**：这一行同时需要 `Script` 类型可见 —— 因为 `GetExpectedAssetType<Script>` 出现在这里，`AssetManager.cpp` 必须能拿到 `Script` 的声明。`ScriptImporter.h` 里 include 了 `AssetImporter.h`，但**没有** include `Script.h`，所以这里要**显式** include：

```cpp
#include "Lucky/Asset/Script.h"
```

按规范 §3.3 的项目内头分组放（`#include "Lucky/Asset/Script.h"`）。放在同组的其他 `Lucky/...` include 之间即可。

**3) `Init()` 里注册 Importer**：

```cpp
        s_Data.Importers[AssetType::Scene] = CreateScope<SceneImporter>();
        s_Data.Importers[AssetType::Script] = CreateScope<ScriptImporter>();
```

**要点**：注册顺序不影响行为（`Load` 时按 `metadata.Type` 查表）。

**4) 文件末尾的「显式实例化模板」清单加一行**：

```cpp
    template Ref<Scene> AssetManager::GetAsset<Scene>(AssetHandle handle);
    template Ref<Script> AssetManager::GetAsset<Script>(AssetHandle handle);
```

**要点**：这段在 `AssetManager.cpp` 的最末尾，上面有一句注释「显式实例化模板（避免链接错误）」。`GetAsset<T>` 的定义在 .cpp 内部，没列在这里的类型在链接时找不到实现 —— **这是本 Phase 唯一一个"编译能过、链接才失败"的坑**（见 1.1 关键约束第 2 条）。

---

### Step 5：注册类型图标

**文件**：`Lucky/Source/Lucky/Editor/EditorIconManager.cpp`

在既有图标加载区追加：

```cpp
        s_IconData.AssetTypeIcons[AssetType::Shader]    = LoadIcon("Asset/Shader.png");
        s_IconData.AssetTypeIcons[AssetType::Script]    = LoadIcon("Component/Script.png");
```

**要点**：

- 复用 `Component/Script.png`（现成文件，零新增二进制资源），见决策点 4.5
- 类型图标 map 的**键是 `AssetType`**，`.cs` 与 ScriptComponent 共用同一张图完全没问题
- 想区分样式就自己放一张 `Resources/Icons/Asset/Script.png`，再把字符串改成 `"Asset/Script.png"`
- **不要**删掉 `GetAssetTypeIcon` 里的兜底 `return s_IconData.FileIcon;` —— 它保护的是所有未来新增类型

---

### Step 6：premake 重新生成 + 编译验证

```bash
Scripts/Setup-Windows.bat
```

- 确认 `Lucky.vcxproj` 里出现了 `Script.h` / `ScriptImporter.h` / `ScriptImporter.cpp`
- Debug 编译 `Lucky` → 通过
- Debug 编译 `Luck3DApp` → 通过

**如果报 `static_assert(sizeof(T) == 0, "Unsupported asset type")`**：说明 Step 4-2 的 `GetExpectedAssetType<Script>` 没加，或 `Script.h` 没 include 进来。

**如果报 `LNK2019` / `LNK2001`**：说明新 `.cpp` 没进工程，重新跑一次 premake。

---

## 6. 疑点问答

### 6.1 一个 `.cs` 里有多个类怎么办？

**本 Phase 不管**。`Script` 资产代表的是**文件**，`ClassName` 取的是文件名词干。P2.2 的类解析按"简单类名 == 文件名词干"去程序集里找，找不到或不唯一就报错 —— 语义上和 Unity 一致（Unity 也要求 MonoBehaviour 的文件名与类名一致）。P2.1 不引入任何校验，因为校验需要程序集，那是 P2.2 的事。

### 6.2 加了 `AssetType::Script` 之后，`AssetRegistry.lcr` 会怎样？

第一次启动时 `Refresh()` 扫到 `Assets/Scripts/PlayerController.cs`（现在它是可识别扩展名了），把它注册进去，Registry 里多出一条：

```yaml
  - Handle: <新生成的 handle>
    Type: Script
    FilePath: Assets/Scripts/PlayerController.cs
```

**旧的 Registry 不需要迁移** —— 类型是按字符串存的，新增枚举值不影响已有记录。这个文件是**会被改动的生成物**（用编辑器跑一次就变），提交时注意别和其它改动混在一起。

### 6.3 文件名大小写 / `.cs` 大小写

扩展名映射**区分大小写**（沿用现有约定）。`PlayerController.cs` 没问题；`PlayerController.CS` 不会被识别。文件名本身大小写自由，但 P2.2 的类名匹配会**区分大小写**（C# 类名本来就区分），所以建议文件名与类名严格一致。

### 6.4 为什么 `GetExpectedAssetType` 不特化会**编译**失败而不是运行失败？

```cpp
template<typename T>
AssetType GetExpectedAssetType()
{
    static_assert(sizeof(T) == 0, "Unsupported asset type");
    return AssetType::None;
}
```

`static_assert(sizeof(T) == 0)` 对任何完整类型都为假，所以只要有哪个 TU 实例化了未特化的版本，编译期立刻报错。这是**故意**的设计：把"忘了给新资产类型注册映射"从运行时的静默 `None` 变成编译期的硬错误。

### 6.5 `ScriptImporter::Save` 返回 `false` 到底会怎样？

`AssetManager::SaveAssetToFile` 会 `return it->second->Save(asset, absolutePath)`，调用方拿到 `false` 当作保存失败。目前没有"批量保存引用资产"的路径会碰 Script，所以暂时不会出事 —— **但这是靠运气**。按决策点 4.4 返回 `true`，语义才是对的。

### 6.6 为什么 `ScriptImporter::Load` 不转绝对路径，其它 Importer 都转了？

因为其它 Importer（Material / Mesh / Texture / Scene）都要**打开文件读内容**，必须拿到绝对路径。`ScriptImporter::Load` 只用文件名的词干，**不碰磁盘**，转绝对路径属于无用功，还会平白引入 `Project.h` 依赖。规范 §13.10 约束的是"需要绝对路径时怎么转"，不是"必须转"。

### 6.7 `Assets/Scripts/` 下还有别的文件会被扫进来吗？

`Luck3DApp/Project/Assets/Scripts/` 目前只有 `PlayerController.cs`。用户程序集编译产物落在 `Luck3DApp/Project/Binaries/`（**不在 `Assets/` 下**），所以编译产物不会被当成资产扫描到。若将来把中间文件放进 `Assets/`，需要靠"隐藏目录（以 `.` 开头）"或扩展名规则排除。

---

## 7. 验收标准

1. **编译通过**：`Lucky` 与 `Luck3DApp` 的 Debug / Release / Dist 三个 configuration 全通过。若报 `LNK2019: 无法解析的外部符号 ... GetAsset<class Lucky::Script>`，说明漏了显式实例化（见 1.1 关键约束第 2 条）
2. **Project 面板可见**：启动编辑器，Project 面板里能看到 `Assets/Scripts/PlayerController.cs`，带脚本类型图标（复用 `Component/Script.png`）
3. **Registry 正确**：`Luck3DApp/Project/AssetRegistry.lcr` 里出现 `Type: Script` 且 `FilePath: Assets/Scripts/PlayerController.cs` 的记录
4. **重启不丢**：关掉编辑器再开，那条记录仍在，`Type` 仍是 `Script`（验证 `StringToAssetType` 分支没漏）
5. **刷新幂等**：Project 面板点 Refresh（或 Ctrl+R）两次，资产数量不变、没有重复条目
6. **重命名不断链**：把 `PlayerController.cs` 改名（F2）→ Registry 里的 `FilePath` 跟着变、Handle 不变；再改回来能恢复
7. **删除同步**：删掉这个 `.cs` → Refresh 后 Registry 里的记录消失 → 再把文件放回去 → Refresh 后重新出现（新 Handle 可以，不要求复用）
8. **加载无错误**：整个过程中日志里**没有** `No importer registered for type: Script`、`Failed to load` 之类的 error
9. **代码规范**：新增/修改文件通过人工 checklist —— 控制语句全带花括号（§5.2）；无"为对齐而对齐"的空格（§5.4）；公有接口有 `/// <summary>` 中文注释（§4.1）；include 顺序符合 §3.3；智能指针只用 `Ref` / `Scope`（§7.1）；无引用外部文档的注释、无解释设计原则的说明性长注释、无"P2.2 会补齐"这类阶段性注释

### 验证 4 的补充说明

"重启不丢"是**最容易失败**的一条，因为它同时验了 `AssetTypeToString`（写）和 `StringToAssetType`（读）两条分支。只加枚举、只加 `ToString` 的话，第 2 条和第 3 条能过，第 4 条必挂。

---

## 8. 对下一 Phase 的接线点

| 位置 | 后续 Phase | 打开方式 |
|------|-----------|---------|
| `Script::GetClassName()` | **P2.2** | 直接拿它去 `ScriptEngine::EntityClasses` 里按简单类名匹配 |
| `ScriptImporter::Load` 里的 hash 计算 | **P3 热重载** | 在 `Script` 上加 `uint64_t SourceHash`，Load 时算，热重载监听变更 |
| `AssetType::Script` | **P2.3** | `PropertyAsset<Script>` 的类型校验天然生效，拖材质进来会被拒 |
| `ScriptImporter::Save` | 若将来支持编辑器内新建脚本 | 改为真正写文件（或改由专门的 `ScriptFileService` 负责） |

---

## 9. 变更清单速览

- **新增文件（3 个）**
  - `Lucky/Source/Lucky/Asset/Script.h`
  - `Lucky/Source/Lucky/Asset/ScriptImporter.h`
  - `Lucky/Source/Lucky/Asset/ScriptImporter.cpp`
- **修改文件（3 个）**
  - `Lucky/Source/Lucky/Asset/AssetType.h`：枚举 + `AssetTypeToString` + `StringToAssetType` + `GetAssetTypeFromExtension`
  - `Lucky/Source/Lucky/Asset/AssetManager.cpp`：include `Script.h` / `ScriptImporter.h`；`GetExpectedAssetType<Script>` 特化；`Init()` 注册 Importer；末尾显式实例化 `GetAsset<Script>`
  - `Lucky/Source/Lucky/Editor/EditorIconManager.cpp`：注册 `AssetType::Script` 图标（复用 `Component/Script.png`）
- **生成物（会变，注意提交时区分）**
  - `Luck3DApp/Project/AssetRegistry.lcr`：多出一条 `Type: Script` 记录
- **删除**：无
- **不改动**：`Asset.h` / `AssetImporter.h` / `AssetRegistry.*` / `AssetMetadata.h` / `Scene/Components/ScriptComponent.h` / `Scripting/*` / premake 脚本（无新目录）

---

## 10. 参考

- 编码规范 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：§3.3 include 顺序、§4.1 XML 注释、§5.2 强制花括号、§5.4 对齐规则、§6.1 class/struct、§6.2 访问修饰符顺序、§7 内存管理、§13.10 路径解析
- 资产系统设计 [PhaseA_Asset_Core_Framework.md](../AssetSystem/PhaseA_Asset_Core_Framework.md)、[PhaseD_Asset_System_Enhancement.md](../AssetSystem/PhaseD_Asset_System_Enhancement.md)
- 资产系统现状 [Asset_System_Status_Tracker.md](../AssetSystem/Asset_System_Status_Tracker.md)
- 脚本系统路线图 [ScriptSystem_Roadmap.md](ScriptSystem_Roadmap.md) 第 4 节 Phase 2
- 既有同类实现：`Asset/SceneImporter.cpp`、`Asset/TextureImporter.cpp`、`Renderer/Material.h`
