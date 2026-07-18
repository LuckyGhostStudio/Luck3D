# ProjectAssetsPanel：统一右键菜单设计

> 本文档隶属于 AssetSystem 系列，作为 [PhaseC_Content_Browser](./PhaseC_Content_Browser.md) 的增强补充。
> 目标：将 `ProjectAssetsPanel` 的右键菜单从"仅右侧资产项可用"扩展到"左侧目录树 / 右侧目录项 / 右侧资产项 / 右侧空白区"四个位置都可用，且四处共用同一份菜单实现，仅在具体行为上根据"上下文"分派。

---

## 目录

- [一、概述](#一概述)
  - [1.1 当前状态](#11-当前状态)
  - [1.2 目标行为（对齐 Unity）](#12-目标行为对齐-unity)
  - [1.3 设计约束](#13-设计约束)
- [二、核心抽象：AssetContext](#二核心抽象assetcontext)
  - [2.1 AssetContextKind 枚举](#21-assetcontextkind-枚举)
  - [2.2 AssetContext 结构体](#22-assetcontext-结构体)
  - [2.3 三种触发点的 Context 构造](#23-三种触发点的-context-构造)
- [三、统一菜单渲染函数](#三统一菜单渲染函数)
  - [3.1 菜单完整结构](#31-菜单完整结构)
  - [3.2 分派矩阵](#32-分派矩阵)
  - [3.3 方案对比：单一入口 vs 三份重载](#33-方案对比单一入口-vs-三份重载)
  - [3.4 方案推荐](#34-方案推荐)
- [四、四个触发点的接入方式](#四四个触发点的接入方式)
  - [4.1 左侧目录树节点](#41-左侧目录树节点)
  - [4.2 右侧目录项（文件夹）](#42-右侧目录项文件夹)
  - [4.3 右侧资产项](#43-右侧资产项)
  - [4.4 右侧内容区空白](#44-右侧内容区空白)
  - [4.5 方案对比：Popup 触发方式](#45-方案对比popup-触发方式)
- [五、右键即选中语义](#五右键即选中语义)
  - [5.1 需求描述](#51-需求描述)
  - [5.2 实现要点](#52-实现要点)
- [六、菜单项行为规约](#六菜单项行为规约)
  - [6.1 Create 子菜单](#61-create-子菜单)
  - [6.2 Delete](#62-delete)
  - [6.3 Rename（预留）](#63-rename预留)
  - [6.4 Show in Explorer](#64-show-in-explorer)
  - [6.5 Refresh](#65-refresh)
  - [6.6 只读信息尾部](#66-只读信息尾部)
- [七、目录级 CRUD 的实现策略](#七目录级-crud-的实现策略)
  - [7.1 方案 A：Panel 内直接实现](#71-方案-apanel-内直接实现)
  - [7.2 方案 B：下沉到 AssetManager](#72-方案-b下沉到-assetmanager)
  - [7.3 方案推荐](#73-方案推荐)
  - [7.4 目录 Delete 的级联规则](#74-目录-delete-的级联规则)
  - [7.5 目录 Rename 的级联规则（预留）](#75-目录-rename-的级联规则预留)
- [八、Create 目标目录的推导](#八create-目标目录的推导)
  - [8.1 规则](#81-规则)
  - [8.2 唯一路径生成](#82-唯一路径生成)
- [九、状态与刷新](#九状态与刷新)
- [十、文件级改动清单](#十文件级改动清单)
- [十一、完整代码骨架](#十一完整代码骨架)
  - [11.1 ProjectAssetsPanel.h](#111-projectassetspanelh)
  - [11.2 ProjectAssetsPanel.cpp（关键片段）](#112-projectassetspanelcpp关键片段)
- [十二、分步实施策略](#十二分步实施策略)
- [十三、验证清单](#十三验证清单)
- [十四、已知限制与后续扩展](#十四已知限制与后续扩展)

---

## 一、概述

### 1.1 当前状态

代码见 [ProjectAssetsPanel.cpp](../../Luck3DApp/Source/Panels/ProjectAssetsPanel.cpp)。

| 触发位置 | 右键菜单 | 现有行为 |
|---|---|---|
| 左侧目录树节点（`DrawDirectoryTreeNode`） | ? 无 | 单击 `NavigateTo` 切换浏览目录 |
| 右侧目录项（`DrawAssetItem` 的 `isDirectory` 分支） | ? 无 | 单击 `SelectionManager::SelectFolder(path)` |
| 右侧资产项（`DrawAssetItem` 的 `assetHandle.IsValid()` 分支） | ? 有（仅 Delete） | 单击 `SelectionManager::SelectAsset(handle)`；右键先选中再弹菜单 |
| 右侧内容区空白 | ? 无 | 单击 `SelectionManager::Deselect()` |

已具备的基础设施：

- `SelectionManager` 已支持 `Entity / Asset / Folder / None` 四态（[SelectionManager.h](../../Lucky/Source/Lucky/Scene/SelectionManager.h)）。
- `AssetManager` 已提供 `CreateAsset` / `EnsureAsset` / `ImportAsset` / `DeleteAsset` / `MoveAsset` / `Refresh` 等接口（[AssetManager.h](../../Lucky/Source/Lucky/Asset/AssetManager.h)）。
- `UI` 层已封装 `BeginPopupContextItem` / `BeginPopupContextWindow` / `BeginPopup` / `EndPopup`（[Widgets.h](../../Lucky/Source/Lucky/UI/Widgets.h)），会自动带上主题 padding，业务侧应统一走 `UI::` 前缀。
- SceneHierarchyPanel 已有"右键即选中 + Create 子菜单"的完整参考实现，可作为对齐样本。

### 1.2 目标行为（对齐 Unity）

- 左侧目录树、右侧目录项、右侧资产项、右侧空白 **四处右键都能弹出菜单，且菜单结构完全一致**。
- 菜单项**入口相同**，但**行为按"点在什么上"分派**，最典型的差异是 `Create *`：

  | 触发上下文 | `Create *` 的目标目录 |
  |---|---|
  | 目录（左树 / 右侧文件夹） | 该目录本身 |
  | 资产 | 该资产的父目录（即"与该资产同级"） |
  | 空白 | 当前浏览目录 `m_CurrentDirectory` |

- `Delete` / `Rename` 只对当前点中的具体项生效；点在空白上时应灰置。
- `Refresh` / `Show in Explorer` 在所有上下文中均可用。

### 1.3 设计约束

1. **不侵入 `AssetManager`**：目录级 CRUD 走 `std::filesystem` + 已有 `MoveAsset` / `DeleteAsset` 级联，避免在通用模块里堆积 UI 特化 API（详见 §7）。
2. **不打破现有资产项行为**：拖拽源、点击选中、双击（后续）等既有语义保留。
3. **遵循项目编码规范**（[Coding_Style_Guide.md](../Coding_Style_Guide.md)）：`m_` 前缀私有成员、Allman 花括号、强制花括号、`const Ref<T>&`、XML 文档注释。
4. **Popup 统一走 `UI::`**：不直接调用 `ImGui::BeginPopupContextItem`，保持主题 padding 一致。
5. **右键即选中**：与 SceneHierarchyPanel 对齐，右键触发时先把当前对象写入 `SelectionManager`，让 Inspector 同步。

---

## 二、核心抽象：AssetContext

### 2.1 AssetContextKind 枚举

用一枚枚举描述"这次右键点在什么上"，与 `SelectionType` 保持解耦（`SelectionType` 是"当前选中语义"，`AssetContextKind` 是"本次右键的定位语义"，两者含义不同不要混用）。

```cpp
/// <summary>
/// 右键上下文类别：描述本次 DrawAssetContextMenu 面向的对象类型
/// - Directory：目录（左树节点 / 右侧文件夹项）
/// - Asset：已注册资产
/// - EmptySpace：右侧内容区空白位置
/// </summary>
enum class AssetContextKind : uint8_t
{
    Directory,
    Asset,
    EmptySpace
};
```

### 2.2 AssetContext 结构体

```cpp
/// <summary>
/// 资产面板右键上下文
/// 统一封装"点在什么上 / 命中项路径 / Create 应该落到哪个目录"
/// 使 DrawAssetContextMenu 面向单一入参，避免三份重载各写一遍菜单
/// </summary>
struct AssetContext
{
    AssetContextKind Kind = AssetContextKind::EmptySpace;

    // 命中项完整路径：Directory / Asset 有效；EmptySpace 为空
    std::filesystem::path Path;

    // 仅 Asset 有效
    AssetHandle Handle;

    // Create 类操作的目标目录（构造时按 Kind 一次性算好）：
    //   - Directory：Path 本身
    //   - Asset：Path.parent_path()
    //   - EmptySpace：m_CurrentDirectory
    std::filesystem::path TargetDir;
};
```

**设计要点**：

- `TargetDir` 在构造时就落定，菜单实现里不再关心"我该怎么选目录"，只管 `ctx.TargetDir / newName`。
- `Handle` 只在 `Kind == Asset` 时有效；`Kind == Directory` 时目录本身不进 Registry，因此不需要 Handle。
- 采用 `struct`（纯数据），成员默认 public，符合规范 §6.1。

### 2.3 三种触发点的 Context 构造

集中在一个私有工厂函数里，避免三处触发点各自算 `TargetDir` 时出错：

```cpp
AssetContext MakeContext(AssetContextKind kind, const std::filesystem::path& path, AssetHandle handle) const;
```

伪代码：

```cpp
AssetContext ProjectAssetsPanel::MakeContext(AssetContextKind kind, const std::filesystem::path& path, AssetHandle handle) const
{
    AssetContext ctx;
    ctx.Kind   = kind;
    ctx.Path   = path;
    ctx.Handle = handle;

    switch (kind)
    {
        case AssetContextKind::Directory:
        {
            ctx.TargetDir = path;
            break;
        }
        case AssetContextKind::Asset:
        {
            ctx.TargetDir = path.parent_path();
            break;
        }
        case AssetContextKind::EmptySpace:
        {
            ctx.TargetDir = m_CurrentDirectory.empty() ? m_AssetsDirectory : m_CurrentDirectory;
            break;
        }
    }
    return ctx;
}
```

---

## 三、统一菜单渲染函数

### 3.1 菜单完整结构

对齐 Unity Project 窗口右键菜单，砍到本项目当前能落地的子集：

```
┌─ Create ?
│    ├─ Folder
│    ├─ Material
│    └─ Scene
├─ ──────────────
├─ Show in Explorer
├─ Reveal Selected Asset       (仅 Asset 可用)
├─ ──────────────
├─ Rename        F2            (Directory / Asset 可用；EmptySpace 灰置或隐藏)
├─ Delete        Del           (Directory / Asset 可用；EmptySpace 灰置或隐藏)
├─ Duplicate                   (仅 Asset 可用)
├─ ──────────────
├─ Refresh       Ctrl+R
├─ ──────────────
└─ (Asset 时) Handle: 0x…      (只读信息，纯展示)
     Type:   Material
     Path:   Assets/Materials/Metal.lmat
```

### 3.2 分派矩阵

同菜单结构下，根据 `ctx.Kind` 决定每一项的可用/隐藏策略：

| 菜单项 | Directory | Asset | EmptySpace | 备注 |
|---|---|---|---|---|
| `Create / Folder` | ? | ? | ? | 目标目录 = `ctx.TargetDir` |
| `Create / Material` | ? | ? | ? | 同上 |
| `Create / Scene` | ? | ? | ? | 同上 |
| `Show in Explorer` | ?（打开该目录） | ?（选中该文件） | ?（打开当前目录） | |
| `Rename` | ? | ? | ? 灰置 | 目录 Rename 见 §7.5 |
| `Delete` | ? | ? | ? 灰置 | 目录 Delete 走级联，见 §7.4 |
| `Duplicate` | ? 灰置 | ? | ? 灰置 | 仅资产 |
| `Refresh` | ? | ? | ? | 等价 `Ctrl+R` |
| 只读信息 | ? 隐藏 | ? | ? 隐藏 | 仅 Asset 展示 |

**灰置**（推荐）与**隐藏**的取舍：Unity 默认灰置，视觉稳定、用户易于知道"这里其实有这个入口，只是当前不可用"，本项目遵循此做法。用 `ImGui::BeginDisabled()` / `ImGui::EndDisabled()` 包裹菜单项即可。

### 3.3 方案对比：单一入口 vs 三份重载

#### 方案 A：单一入口 `DrawAssetContextMenu(const AssetContext& ctx)`（推荐）

```cpp
void DrawAssetContextMenu(const AssetContext& ctx);
```

- **优点**：菜单结构只写一份；后续增删菜单项只改一处；Kind 差异在项级别用 `if (ctx.Kind == ...)` 或 `BeginDisabled` 处理，非常直观。
- **缺点**：每个菜单项内部需要判定 Kind 才能决定禁用/隐藏；如果菜单差异极大时，`if` 会变多。当前差异 = "少数几项灰置 + Create 目标路径不同"，完全可控。

#### 方案 B：三份重载函数

```cpp
void DrawDirectoryContextMenu(const std::filesystem::path& dir);
void DrawAssetContextMenu(AssetHandle handle);
void DrawEmptySpaceContextMenu();
```

- **优点**：单个函数逻辑更"纯"，无 `if (Kind == …)` 分支。
- **缺点**：菜单结构写三遍，后续增改一项要动三处，**天然容易漂移导致三份菜单出现细微差异**??这恰恰是本次需求要消灭的问题。

#### 方案 C：数据驱动（菜单项配置表）

用 `std::vector<MenuItemDesc>` 描述整个菜单树，`DrawAssetContextMenu(ctx)` 只做数据渲染。

- **优点**：极致可扩展，未来插件式扩展菜单（如 Unity `MenuItem` 属性）时结构现成。
- **缺点**：对于当前仅 8-10 项的菜单是过度设计；调试和 IDE 跳转体验都不如直接写 `MenuItem`。

### 3.4 方案推荐

**推荐方案 A（单一入口 + Kind 分派）**。

**推荐原因**：

1. 与用户"菜单内容完全一致，仅少数行为按上下文分派"的诉求 1:1 对齐??单份实现自然保证内容一致。
2. 当前菜单项数量（≤10）远未到需要数据驱动的规模；SceneHierarchyPanel 也是走单函数结构，风格一致。
3. Kind 差异少（灰置几项 + Create 目标目录），`if` 分支非常克制。

---

## 四、四个触发点的接入方式

### 4.1 左侧目录树节点

位置：`DrawDirectoryTreeNode(DirectoryNode& node)`，在 `UI::BeginTreeNode` 返回之后、`if (opened)` 之前挂菜单。

```cpp
bool opened = UI::BeginTreeNode(folderClosedIcon, folderOpenIcon, strID.c_str(), isRoot, isCurrentDir, isLeaf);

// 右键菜单（挂在 TreeNode 这个 Item 上）
{
    // 用节点路径生成稳定 popup ID，避免多个目录节点共享同一 popup
    std::string popupID = std::format("##DirPopup_{}", node.FullPath.generic_string());
    if (UI::BeginPopupContextItem(popupID.c_str(), ImGuiPopupFlags_MouseButtonRight))
    {
        SelectionManager::SelectFolder(node.FullPath);   // 右键即选中
        AssetContext ctx = MakeContext(AssetContextKind::Directory, node.FullPath, AssetHandle{});
        DrawAssetContextMenu(ctx);
        UI::EndPopup();
    }
}
```

**注意**：`BeginPopupContextItem` 必须紧跟 `BeginTreeNode`，因为它作用在"上一个 item"上；且需要在 `if (opened)` 递归子节点**之前**调用，否则会关联到最后一个子节点。

### 4.2 右侧目录项（文件夹）

位置：`DrawAssetItem` 中，`isDirectory == true` 分支。将现有"仅对资产挂菜单"的 `if` 改为"对目录和资产都挂菜单，用 `ctx` 区分"。

```cpp
if (isDirectory)
{
    if (UI::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight))
    {
        SelectionManager::SelectFolder(path);
        AssetContext ctx = MakeContext(AssetContextKind::Directory, path, AssetHandle{});
        DrawAssetContextMenu(ctx);
        UI::EndPopup();
    }
}
else if (assetHandle.IsValid())
{
    if (UI::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight))
    {
        SelectionManager::SelectAsset(assetHandle);
        AssetContext ctx = MakeContext(AssetContextKind::Asset, path, assetHandle);
        DrawAssetContextMenu(ctx);
        UI::EndPopup();
    }
}
```

`BeginPopupContextItem(nullptr)` 会自动用当前 item 的 ID 生成 popup ID，右侧列表因为 `strID` 已用 `path.stem##handle` 保证唯一，天然不冲突。

### 4.3 右侧资产项

**已经存在**（当前 `DrawAssetContextMenu(AssetHandle)`），改造点：

1. 函数签名改为 `DrawAssetContextMenu(const AssetContext& ctx)`。
2. 把 `SelectionManager::SelectAsset(ctx.Handle)` 保留在 `Kind == Asset` 分支下即可，其余逻辑走 §六 的规约。

### 4.4 右侧内容区空白

位置：`DrawContentArea`，遍历所有 item **之后**挂 `BeginPopupContextWindow`。

```cpp
void ProjectAssetsPanel::DrawContentArea()
{
    if (m_CurrentDirectory.empty())
    {
        return;
    }

    for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
    {
        DrawAssetItem(entry);
    }

    // 空白右键：ImGuiPopupFlags_NoOpenOverItems 保证不与 item 菜单打架
    if (UI::BeginPopupContextWindow("##ContentAreaEmptyPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        SelectionManager::Deselect();   // 空白右键：清空选中，与"空白左键"语义一致
        AssetContext ctx = MakeContext(AssetContextKind::EmptySpace, std::filesystem::path{}, AssetHandle{});
        DrawAssetContextMenu(ctx);
        UI::EndPopup();
    }
}
```

关键 flag：`ImGuiPopupFlags_NoOpenOverItems` ?? 光标处在某个 item 之上时不响应，让 item 自己的 `BeginPopupContextItem` 独占该次右键。这样"item 菜单 vs 空白菜单"就不会同时触发。

### 4.5 方案对比：Popup 触发方式

#### 方案 A：全部走 `UI::BeginPopupContextItem` + `UI::BeginPopupContextWindow`（推荐）

- **优点**：符合 ImGui 官方 idiom；ID 自动关联到 item；`NoOpenOverItems` 天然处理"item vs 空白"边界。
- **缺点**：Popup 必须在同一帧、同一父窗口内 `EndPopup`，代码组织稍有约束（但 §4 的接入方式已经解决）。

#### 方案 B：手动 `ImGui::IsMouseClicked(ImGuiMouseButton_Right)` + `ImGui::OpenPopup`

- **优点**：更灵活，可以异步在"下一帧"打开菜单、可以自己控制 ID。
- **缺点**：需要自己判定 `IsItemHovered` / `IsWindowHovered`，容易与 SceneHierarchyPanel 的封装惯例背离；边界处理繁琐（例如 item 上右键要抑制 window popup）。

**推荐方案 A**，与 SceneHierarchyPanel 的写法一致，可读性和维护成本都最低。

---

## 五、右键即选中语义

### 5.1 需求描述

Unity 的标准行为：右键点在某个对象上时，即便你之前选中的是别的对象，也会先把右键对象设为选中态，Inspector 同步刷新。这样菜单里出现的 `Delete / Rename / ...` 才是"对我看到的那个高亮项生效"，用户心智负担最小。

SceneHierarchyPanel 已经这么做（见 §背景 codebase_search 的 `Select(id)` 分支）。

### 5.2 实现要点

在每个触发点的 `BeginPopupContextItem` 返回 `true` 之后，**先写 SelectionManager，再画菜单**：

| 触发上下文 | 右键即选中的调用 |
|---|---|
| Directory | `SelectionManager::SelectFolder(ctx.Path)` |
| Asset | `SelectionManager::SelectAsset(ctx.Handle)` |
| EmptySpace | `SelectionManager::Deselect()` |

这三行只在 `BeginPopupContextItem` 首帧打开时执行一次（`BeginPopupContextItem` 已保证只在打开帧返回 true 且执行体内的动作是幂等的）。

---

## 六、菜单项行为规约

### 6.1 Create 子菜单

```cpp
if (ImGui::BeginMenu("Create"))
{
    if (ImGui::MenuItem("Folder"))
    {
        CreateFolderAt(ctx.TargetDir);
    }

    if (ImGui::MenuItem("Material"))
    {
        CreateMaterialAt(ctx.TargetDir);
    }

    if (ImGui::MenuItem("Scene"))
    {
        CreateSceneAt(ctx.TargetDir);
    }

    ImGui::EndMenu();
}
```

- 所有 `Create *` 一律走 `ctx.TargetDir`，天然满足"目录内创建 / 与资产同级创建 / 空白落到当前目录"的三种语义（§8）。
- `CreateFolderAt` 内部走 `std::filesystem::create_directory` + 唯一命名（§8.2）+ `RebuildDirectoryTree()`。
- `CreateMaterialAt` / `CreateSceneAt` 走 `AssetManager::CreateAsset` + 唯一命名。
- **注意**：`Create Folder` 目前不进入 Registry（目录不是资产），因此**必须 `RebuildDirectoryTree`** 让树刷新；`Create Material/Scene` 进入 Registry 后**同样需要** `RebuildDirectoryTree`（因为右侧列表通过 `directory_iterator` 直接读盘，虽然会自动看见文件，但目录树侧还是需要刷新以处理"若创建的是子目录不小心被识别为资产"这类边界）。

### 6.2 Delete

```cpp
bool canDelete = (ctx.Kind == AssetContextKind::Directory) ||
                 (ctx.Kind == AssetContextKind::Asset);

ImGui::BeginDisabled(!canDelete);
if (ImGui::MenuItem("Delete"))
{
    if (ctx.Kind == AssetContextKind::Asset)
    {
        if (AssetManager::DeleteAsset(ctx.Handle))
        {
            SelectionManager::Deselect();
            RebuildDirectoryTree();
        }
    }
    else if (ctx.Kind == AssetContextKind::Directory)
    {
        DeleteFolderRecursive(ctx.Path);   // 见 §7.4
        SelectionManager::Deselect();
        RebuildDirectoryTree();
    }
}
ImGui::EndDisabled();
```

### 6.3 Rename（预留）

第一阶段先不做行内编辑，`Rename` 项灰置或直接不展示。第二阶段接入 SceneHierarchyPanel 已有的行内 rename 模式（`m_RenamingPath` + `m_RenameBuffer` 状态机）。

### 6.4 Show in Explorer

Windows 上用 `ShellExecuteW(nullptr, L"open", L"explorer.exe", L"/select,\"<absPath>\"", nullptr, SW_SHOWNORMAL)` 可以打开 Explorer 并选中文件；如果是目录，用 `/select,"<absPath>"` 会打开父目录并选中该目录本身（更符合直觉），也可直接 `ShellExecuteW(nullptr, L"open", pathW.c_str(), ...)` 直接进入该目录。

```cpp
if (ImGui::MenuItem("Show in Explorer"))
{
    std::filesystem::path target = (ctx.Kind == AssetContextKind::EmptySpace) ? m_CurrentDirectory : ctx.Path;
    RevealInExplorer(target);   // 内部按 Directory / File 分派
}
```

`RevealInExplorer` 建议放在 Panel 私有方法里（一次用不必上升到通用工具层）。若后续多面板都需要，可以下沉到 `Lucky/Utils/PlatformUtils.h`。

### 6.5 Refresh

直接调用现有的 `OnRefreshRequested()`。行为对齐 `Ctrl+R`。

### 6.6 只读信息尾部

```cpp
if (ctx.Kind == AssetContextKind::Asset)
{
    ImGui::Separator();

    // 只读展示：Handle / Type / Path
    ImGui::TextDisabled("Handle: 0x%08X", static_cast<uint32_t>(ctx.Handle));
    ImGui::TextDisabled("Type:   %s", AssetTypeToString(AssetManager::GetAssetType(ctx.Handle)));
    ImGui::TextDisabled("Path:   %s", AssetManager::GetAssetFilePath(ctx.Handle).c_str());
}
```

用 `TextDisabled` 让它明显区别于可点击菜单项；不参与点击。

---

## 七、目录级 CRUD 的实现策略

### 7.1 方案 A：Panel 内直接实现

在 `ProjectAssetsPanel` 中新增：

```cpp
void CreateFolderAt(const std::filesystem::path& parentDir);
void DeleteFolderRecursive(const std::filesystem::path& dir);
void RenameFolder(const std::filesystem::path& oldPath, const std::string& newName);   // 预留
```

内部走：

- `std::filesystem::create_directory` / `remove_all` / `rename`
- 遍历该目录已注册资产，逐个调用 `AssetManager::DeleteAsset` / `MoveAsset` 做 Registry 级联

- **优点**：不侵入通用模块；`AssetManager` 保持"只管资产、不管目录"的清爽职责。
- **缺点**：如果未来 SceneHierarchyPanel 之外还有别的地方要做目录操作，需要重复实现。

### 7.2 方案 B：下沉到 AssetManager

新增 `AssetManager::CreateFolder / DeleteFolder / RenameFolder`。

- **优点**：接口统一，其他调用方直接复用。
- **缺点**：目录本身不是 Asset，硬塞进 `AssetManager` 会让职责边界变糊；而目前也**只有 Project 面板一个调用方**，属于过度抽象。

### 7.3 方案推荐

**推荐方案 A（Panel 内实现）**。

**推荐原因**：

1. 唯一调用方是 `ProjectAssetsPanel`，YAGNI 原则；
2. `AssetManager` 语义保持"资产为一等公民"不被稀释；
3. 未来若真的出现第二个调用方，从 Panel 上抽出到 `AssetManager` 是一次代价很低的重构。

### 7.4 目录 Delete 的级联规则

删除目录**不能**直接 `remove_all`，否则 Registry 中会留下悬空条目（对应的资产文件已消失，但 Handle 还在，`GetAsset` 会失败）。正确顺序：

```
1. 遍历目录（含子目录）下所有已注册资产
   for path in recursive_directory_iterator(dir):
       if AssetManager::GetAssetHandle(path.generic_string()) is valid:
           AssetManager::DeleteAsset(handle);   // 自动清 Registry + 缓存 + 磁盘文件
2. std::filesystem::remove_all(dir);            // 兜底删除空目录 + 未识别文件
3. RebuildDirectoryTree()
4. 若 m_CurrentDirectory 在被删目录之下，回退到 m_AssetsDirectory
```

伪代码：

```cpp
void ProjectAssetsPanel::DeleteFolderRecursive(const std::filesystem::path& dir)
{
    if (!std::filesystem::exists(dir))
    {
        return;
    }

    // ---- 1. 先按 Registry 级联删除资产 ----
    std::vector<AssetHandle> toDelete;
    for (auto& entry : std::filesystem::recursive_directory_iterator(dir))
    {
        if (entry.is_directory())
        {
            continue;
        }

        AssetHandle handle = AssetManager::GetAssetHandle(entry.path().generic_string());
        if (handle.IsValid())
        {
            toDelete.push_back(handle);
        }
    }

    for (AssetHandle handle : toDelete)
    {
        AssetManager::DeleteAsset(handle);
    }

    // ---- 2. 兜底：删除目录本身（含未识别文件） ----
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    if (ec)
    {
        LF_CORE_ERROR("ProjectAssetsPanel::DeleteFolderRecursive - Failed to remove '{0}': {1}", dir.generic_string(), ec.message());
    }


    // ---- 3. m_CurrentDirectory 回退保护 ----
    if (!std::filesystem::exists(m_CurrentDirectory))
    {
        m_CurrentDirectory = m_AssetsDirectory;
    }
}
```

### 7.5 目录 Rename 的级联规则（预留）

目录 rename 涉及**其下所有子资产**的相对路径变化，`AssetManager::MoveAsset` 是"逐文件"的接口。正确顺序：

```
1. 收集目录下所有已注册资产的旧相对路径 + Handle
2. 计算每个资产的新相对路径 = replace_prefix(oldRel, oldDirRel, newDirRel)
3. std::filesystem::rename(oldDir, newDir)   // 目录整体先 rename（一次系统调用，原子性最好）
4. 对每个 Handle 调用 AssetManager::MoveAsset(handle, newRel)
   ? 注意：此时磁盘上文件已经在 newRel 位置了；MoveAsset 内部会再尝试 rename oldRel→newRel，
     会失败。因此需要给 AssetManager 增加一个 "仅更新 Registry 路径、不做磁盘 rename" 的入口，
     或者反过来："先逐文件 MoveAsset，最后再 rename 目录本身"（磁盘 IO 次数多但语义正）。
```

这块比较复杂，**第一阶段不实现**（Rename 项灰置），第二阶段单独设计一份小文档处理。

---

## 八、Create 目标目录的推导

### 8.1 规则

复述 §1.2 表格，实现落在 `MakeContext`（§2.3）中，一次算好写入 `ctx.TargetDir`：

| Kind | TargetDir |
|---|---|
| Directory | `ctx.Path` |
| Asset | `ctx.Path.parent_path()` |
| EmptySpace | `m_CurrentDirectory`（若为空则 `m_AssetsDirectory`）|

### 8.2 唯一路径生成

用户在同一目录连续 Create 时不能报错，必须自动加序号（Unity 用 `New Material.mat` / `New Material 1.mat`）。

```cpp
/// <summary>
/// 在 base 目录下生成一个不冲突的路径：
///   stem, stem 1, stem 2, ...
/// 若 stem+ext 本身不存在，直接返回 stem+ext
/// </summary>
static std::filesystem::path MakeUniquePath(const std::filesystem::path& baseDir, const std::string& stem, const std::string& ext);
```

参考实现：

```cpp
std::filesystem::path ProjectAssetsPanel::MakeUniquePath(const std::filesystem::path& baseDir, const std::string& stem, const std::string& ext)
{
    std::filesystem::path candidate = baseDir / (stem + ext);
    if (!std::filesystem::exists(candidate))
    {
        return candidate;
    }

    int index = 1;
    while (true)
    {
        std::string name = std::format("{} {}{}", stem, index, ext);
        candidate = baseDir / name;
        if (!std::filesystem::exists(candidate))
        {
            return candidate;
        }
        ++index;
    }
}
```

---

## 九、状态与刷新

任何 CRUD 完成后统一执行三件事（组合成私有方法 `AfterCrudRefresh` 或直接内联）：

1. `RebuildDirectoryTree()` ?? 左侧树立即反映磁盘变化。
2. `SelectionManager::Deselect()` ?? 若删除的正是当前选中项，Inspector 立即回到空态。判断方式：
   - 若删除 Asset：`SelectionManager::IsAssetSelected(handle)` 命中 → `Deselect`。
   - 若删除 Directory：`SelectionManager::IsFolderSelected(path)` 或选中项是该目录下的子资产 → `Deselect`。
3. `m_CurrentDirectory` 兜底：若已不存在，回退到 `m_AssetsDirectory`。

---

## 十、文件级改动清单

| 文件 | 改动 |
|---|---|
| [ProjectAssetsPanel.h](../../Luck3DApp/Source/Panels/ProjectAssetsPanel.h) | 新增 `AssetContextKind` / `AssetContext` / `MakeContext` / `DrawAssetContextMenu(const AssetContext&)` / 目录 CRUD 与 `RevealInExplorer` / `MakeUniquePath` 声明；保留现有 `DrawAssetContextMenu(AssetHandle)` 若无外部引用则移除 |
| [ProjectAssetsPanel.cpp](../../Luck3DApp/Source/Panels/ProjectAssetsPanel.cpp) | 1) `DrawDirectoryTreeNode` 挂 `UI::BeginPopupContextItem`；2) `DrawAssetItem` 里目录分支也挂 popup；3) 资产分支的 popup 改走新签名；4) `DrawContentArea` 末尾挂 `UI::BeginPopupContextWindow`；5) 实现 `MakeContext` / `DrawAssetContextMenu(ctx)` / `CreateFolderAt` / `CreateMaterialAt` / `CreateSceneAt` / `DeleteFolderRecursive` / `RevealInExplorer` / `MakeUniquePath` |
| 其他 | 无（不改 `AssetManager` / `SelectionManager` / `Widgets`） |

---

## 十一、完整代码骨架

### 11.1 ProjectAssetsPanel.h

```cpp
#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Asset/AssetType.h"
#include "Lucky/Asset/AssetHandle.h"
#include "Lucky/Renderer/Texture.h"

#include <filesystem>

namespace Lucky
{
    /// <summary>
    /// 目录树节点缓存
    /// </summary>
    struct DirectoryNode
    {
        std::string Name;                           // 目录名
        std::filesystem::path FullPath;             // 完整路径
        std::vector<DirectoryNode> SubDirectories;  // 子目录
    };

    /// <summary>
    /// 右键上下文类别：描述本次 DrawAssetContextMenu 面向的对象类型
    /// </summary>
    enum class AssetContextKind : uint8_t
    {
        Directory,      // 目录（左树 / 右侧文件夹项）
        Asset,          // 已注册资产
        EmptySpace      // 右侧内容区空白
    };

    /// <summary>
    /// 资产面板右键上下文
    /// 统一封装命中类型、命中路径、Handle 与 Create 目标目录
    /// </summary>
    struct AssetContext
    {
        AssetContextKind Kind = AssetContextKind::EmptySpace;
        std::filesystem::path Path;         // Directory / Asset 有效
        AssetHandle Handle;                 // 仅 Asset 有效
        std::filesystem::path TargetDir;    // Create 类操作的落点目录
    };

    /// <summary>
    /// 项目资产面板
    /// </summary>
    class ProjectAssetsPanel : public EditorPanel
    {
    public:
        ProjectAssetsPanel();
        ~ProjectAssetsPanel() override = default;

        void OnUpdate(DeltaTime dt) override;
        void OnGUI() override;
        void OnEvent(Event& event) override;
    private:
        // ---- 绘制 ----
        void DrawToolbar();
        void DrawDirectoryTreeNode(DirectoryNode& node);
        void DrawContentArea();
        void DrawAssetItem(const std::filesystem::directory_entry& entry);
        void DrawAssetContextMenu(const AssetContext& ctx);

        // ---- 上下文构造 ----
        AssetContext MakeContext(AssetContextKind kind, const std::filesystem::path& path, AssetHandle handle) const;

        // ---- 导航 / 刷新 ----
        void NavigateTo(const std::filesystem::path& directory);
        void OnRefreshRequested();
        void RebuildDirectoryTree();
        DirectoryNode BuildDirectoryNode(const std::filesystem::path& path);

        // ---- CRUD ----
        void CreateFolderAt(const std::filesystem::path& parentDir);
        void CreateMaterialAt(const std::filesystem::path& parentDir);
        void CreateSceneAt(const std::filesystem::path& parentDir);
        void DeleteFolderRecursive(const std::filesystem::path& dir);

        // ---- 工具 ----
        void RevealInExplorer(const std::filesystem::path& path) const;
        static std::filesystem::path MakeUniquePath(const std::filesystem::path& baseDir, const std::string& stem, const std::string& ext);

        // ---- 缩略图 / 类型识别 ----
        Ref<Texture2D> GetThumbnail(const std::filesystem::path& filepath);
        AssetType GetAssetTypeFromPath(const std::filesystem::path& filepath) const;
    private:
        std::filesystem::path m_AssetsDirectory;    // Assets 根目录
        std::filesystem::path m_CurrentDirectory;   // 当前浏览目录

        DirectoryNode m_RootNode;                   // 目录树缓存

        float m_TreePanelWidth = 200.0f;            // 目录树宽度

        bool m_IsFocused = false;                   // 当前帧面板是否处于聚焦
    };
}
```

### 11.2 ProjectAssetsPanel.cpp（关键片段）

以下仅列出**新增/改动**的关键片段，未列出的方法（`OnUpdate` / `OnGUI` / `DrawToolbar` / `NavigateTo` / `OnRefreshRequested` / `RebuildDirectoryTree` / `BuildDirectoryNode` / `GetThumbnail` / `GetAssetTypeFromPath`）保持原样。

#### 11.2.1 MakeContext

```cpp
AssetContext ProjectAssetsPanel::MakeContext(AssetContextKind kind, const std::filesystem::path& path, AssetHandle handle) const
{
    AssetContext ctx;
    ctx.Kind = kind;
    ctx.Path = path;
    ctx.Handle = handle;

    switch (kind)
    {
        case AssetContextKind::Directory:
        {
            ctx.TargetDir = path;
            break;
        }
        case AssetContextKind::Asset:
        {
            ctx.TargetDir = path.parent_path();
            break;
        }
        case AssetContextKind::EmptySpace:
        {
            ctx.TargetDir = m_CurrentDirectory.empty() ? m_AssetsDirectory : m_CurrentDirectory;
            break;
        }
    }
    return ctx;
}
```

#### 11.2.2 DrawDirectoryTreeNode（挂 popup）

```cpp
void ProjectAssetsPanel::DrawDirectoryTreeNode(DirectoryNode& node)
{
    const std::string& strID = node.Name;
    bool isRoot = node.FullPath == m_AssetsDirectory;
    bool isLeaf = node.SubDirectories.empty();
    bool isCurrentDir = (m_CurrentDirectory == node.FullPath);

    const Ref<Texture2D>& folderClosedIcon = EditorIconManager::GetFolderIcon(false);
    const Ref<Texture2D>& folderOpenIcon = EditorIconManager::GetFolderIcon(true);

    if (isRoot)
    {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    }

    bool opened = UI::BeginTreeNode(folderClosedIcon, folderOpenIcon, strID.c_str(), isRoot, isCurrentDir, isLeaf);

    if (isRoot)
    {
        ImGui::PopFont();
    }

    // ---- 右键菜单（必须紧跟 BeginTreeNode） ----
    {
        std::string popupID = std::format("##DirPopup_{}", node.FullPath.generic_string());
        if (UI::BeginPopupContextItem(popupID.c_str(), ImGuiPopupFlags_MouseButtonRight))
        {
            SelectionManager::SelectFolder(node.FullPath);
            AssetContext ctx = MakeContext(AssetContextKind::Directory, node.FullPath, AssetHandle{});
            DrawAssetContextMenu(ctx);
            UI::EndPopup();
        }
    }

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        NavigateTo(node.FullPath);
    }

    if (opened)
    {
        for (DirectoryNode& subDir : node.SubDirectories)
        {
            DrawDirectoryTreeNode(subDir);
        }

        UI::EndTreeNode();
    }
}
```

#### 11.2.3 DrawAssetItem（目录/资产都挂）

只贴关键改动（原 §"右键上下文菜单"块整块替换）：

```cpp
// ---- 右键菜单：目录 / 资产 分别挂，走统一菜单实现 ----
if (isDirectory)
{
    if (UI::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight))
    {
        SelectionManager::SelectFolder(path);
        AssetContext ctx = MakeContext(AssetContextKind::Directory, path, AssetHandle{});
        DrawAssetContextMenu(ctx);
        UI::EndPopup();
    }
}
else if (assetHandle.IsValid())
{
    if (UI::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight))
    {
        SelectionManager::SelectAsset(assetHandle);
        AssetContext ctx = MakeContext(AssetContextKind::Asset, path, assetHandle);
        DrawAssetContextMenu(ctx);
        UI::EndPopup();
    }
}
```

#### 11.2.4 DrawContentArea（空白挂 window popup）

```cpp
void ProjectAssetsPanel::DrawContentArea()
{
    if (m_CurrentDirectory.empty())
    {
        return;
    }

    for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
    {
        DrawAssetItem(entry);
    }

    // ---- 空白区右键 ----
    if (UI::BeginPopupContextWindow("##ContentAreaEmptyPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        SelectionManager::Deselect();
        AssetContext ctx = MakeContext(AssetContextKind::EmptySpace, std::filesystem::path{}, AssetHandle{});
        DrawAssetContextMenu(ctx);
        UI::EndPopup();
    }
}
```

#### 11.2.5 DrawAssetContextMenu（统一实现）

```cpp
void ProjectAssetsPanel::DrawAssetContextMenu(const AssetContext& ctx)
{
    // ---- Create 子菜单：三种上下文都可用 ----
    if (ImGui::BeginMenu("Create"))
    {
        if (ImGui::MenuItem("Folder"))
        {
            CreateFolderAt(ctx.TargetDir);
        }
        if (ImGui::MenuItem("Material"))
        {
            CreateMaterialAt(ctx.TargetDir);
        }
        if (ImGui::MenuItem("Scene"))
        {
            CreateSceneAt(ctx.TargetDir);
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();

    // ---- Show in Explorer：所有上下文都可用 ----
    if (ImGui::MenuItem("Show in Explorer"))
    {
        std::filesystem::path target = (ctx.Kind == AssetContextKind::EmptySpace) ? m_CurrentDirectory : ctx.Path;
        RevealInExplorer(target);
    }


    ImGui::Separator();

    // ---- Rename（预留，第一阶段灰置）----
    ImGui::BeginDisabled(true);
    ImGui::MenuItem("Rename", "F2");
    ImGui::EndDisabled();

    // ---- Delete ----
    bool canDelete = (ctx.Kind == AssetContextKind::Directory) || (ctx.Kind == AssetContextKind::Asset);
    ImGui::BeginDisabled(!canDelete);
    if (ImGui::MenuItem("Delete", "Del"))
    {
        if (ctx.Kind == AssetContextKind::Asset)
        {
            if (AssetManager::DeleteAsset(ctx.Handle))
            {
                SelectionManager::Deselect();
                RebuildDirectoryTree();
            }
        }
        else if (ctx.Kind == AssetContextKind::Directory)
        {
            DeleteFolderRecursive(ctx.Path);
            SelectionManager::Deselect();
            RebuildDirectoryTree();
        }
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    // ---- Refresh：所有上下文都可用 ----
    if (ImGui::MenuItem("Refresh", "Ctrl+R"))
    {
        OnRefreshRequested();
    }

    // ---- 只读信息（仅 Asset） ----
    if (ctx.Kind == AssetContextKind::Asset)
    {
        ImGui::Separator();
        ImGui::TextDisabled("Handle: 0x%08X", static_cast<uint32_t>(ctx.Handle));
        ImGui::TextDisabled("Type:   %s", AssetTypeToString(AssetManager::GetAssetType(ctx.Handle)));
        ImGui::TextDisabled("Path:   %s", AssetManager::GetAssetFilePath(ctx.Handle).c_str());
    }
}
```

#### 11.2.6 CreateFolderAt / CreateMaterialAt / CreateSceneAt

```cpp
void ProjectAssetsPanel::CreateFolderAt(const std::filesystem::path& parentDir)
{
    std::filesystem::path target = MakeUniquePath(parentDir, "New Folder", "");
    std::error_code ec;
    std::filesystem::create_directory(target, ec);
    if (ec)
    {
        LF_CORE_ERROR("ProjectAssetsPanel::CreateFolderAt - Failed to create '{0}': {1}", target.generic_string(), ec.message());
        return;
    }
    RebuildDirectoryTree();
}

void ProjectAssetsPanel::CreateMaterialAt(const std::filesystem::path& parentDir)
{
    std::filesystem::path target = MakeUniquePath(parentDir, "New Material", ".lmat");
    Ref<Material> material = CreateRef<Material>();     // 具体默认构造/默认 Shader 由 Material 侧决定
    AssetHandle handle = AssetManager::CreateAsset(material, target.generic_string());
    if (!handle.IsValid())
    {
        LF_CORE_ERROR("ProjectAssetsPanel::CreateMaterialAt - Failed to create material at '{0}'", target.generic_string());
        return;
    }
    RebuildDirectoryTree();
    SelectionManager::SelectAsset(handle);              // 与 Unity 一致：创建后自动选中
}

void ProjectAssetsPanel::CreateSceneAt(const std::filesystem::path& parentDir)
{
    std::filesystem::path target = MakeUniquePath(parentDir, "New Scene", ".luck3d");
    Ref<Scene> scene = CreateRef<Scene>();
    AssetHandle handle = AssetManager::CreateAsset(scene, target.generic_string());
    if (!handle.IsValid())
    {
        LF_CORE_ERROR("ProjectAssetsPanel::CreateSceneAt - Failed to create scene at '{0}'", target.generic_string());
        return;
    }
    RebuildDirectoryTree();
    SelectionManager::SelectAsset(handle);
}
```

> ? `Material` / `Scene` 的默认构造签名需要与项目当前 API 对齐；若默认构造不便，可在这里显式传入默认 Shader 名。

#### 11.2.7 DeleteFolderRecursive / RevealInExplorer / MakeUniquePath

见 §7.4 / §6.4 / §8.2 的伪代码，直接落地即可。`RevealInExplorer` 的 Windows 实现示例：

```cpp
void ProjectAssetsPanel::RevealInExplorer(const std::filesystem::path& path) const
{
#ifdef _WIN32
    if (!std::filesystem::exists(path))
    {
        LF_CORE_WARN("ProjectAssetsPanel::RevealInExplorer - Path does not exist: {0}", path.generic_string());
        return;
    }

    std::wstring wPath = std::filesystem::absolute(path).wstring();

    if (std::filesystem::is_directory(path))
    {
        // 直接打开该目录
        ShellExecuteW(nullptr, L"open", wPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    else
    {
        // 打开父目录并选中该文件
        std::wstring param = L"/select,\"" + wPath + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
    }
#else
    LF_CORE_WARN("ProjectAssetsPanel::RevealInExplorer - Not implemented on this platform.");
#endif
}
```

---

## 十二、分步实施策略

分两个 Sprint 落地，可独立提交与验证。

### Sprint 1：三处右键都能弹出统一菜单（最小可用）

1. 在 [ProjectAssetsPanel.h](../../Luck3DApp/Source/Panels/ProjectAssetsPanel.h) 引入 `AssetContextKind` / `AssetContext` / `MakeContext` 声明。
2. 把 [ProjectAssetsPanel.cpp](../../Luck3DApp/Source/Panels/ProjectAssetsPanel.cpp) 的 `DrawAssetContextMenu(AssetHandle)` 改造为 `DrawAssetContextMenu(const AssetContext&)`，内部**先只实现** `Delete` / `Show in Explorer` / `Refresh`（Create 空实现）。
3. `DrawDirectoryTreeNode` / `DrawAssetItem` 目录分支 / `DrawContentArea` 三处挂 popup。
4. 验证：四处右键都能弹出同一份菜单；Delete Asset 生效；Delete Directory 走级联；Show in Explorer 正确开窗。

### Sprint 2：Create 子菜单 + 只读信息

1. 实现 `CreateFolderAt` / `CreateMaterialAt` / `CreateSceneAt` / `MakeUniquePath`。
2. 打通 `AssetManager::CreateAsset` 对 `Material` / `Scene` 的默认构造路径（若已有默认构造，直接用；否则补一份最小默认构造）。
3. 在菜单中启用 Create 子菜单；补上 §6.6 的只读信息尾部。
4. 验证：Create Folder 在左树 / 右侧目录 / 右侧资产 / 右侧空白四种上下文下都能生成正确位置的目录（对齐 Unity）。

### Sprint 3（后续）：Rename 行内编辑

按 SceneHierarchyPanel 的 rename 状态机复刻，含目录 rename 级联（§7.5）。

---

## 十三、验证清单

| 编号 | 场景 | 期望 |
|---|---|---|
| V1 | 左侧目录树任意节点右键 | 弹出统一菜单 |
| V2 | 右侧文件夹项右键 | 弹出统一菜单 |
| V3 | 右侧资产项右键 | 弹出统一菜单，末尾展示 Handle/Type/Path |
| V4 | 右侧空白右键 | 弹出统一菜单，Rename/Delete/Duplicate 灰置 |
| V5 | 光标在右侧资产上右键 | **只**弹 item 菜单，不弹空白菜单（`NoOpenOverItems` 生效）|
| V6 | 右键任意对象后，Inspector 立即切换到该对象 | 右键即选中 |
| V7 | 在左树目录 `A` 上 `Create / Folder` | 新目录出现在 `A/` 下 |
| V8 | 在右侧资产 `x.lmat` 上 `Create / Material` | 新材质出现在 `x.lmat` 的父目录下（与之同级）|
| V9 | 右侧空白 `Create / Scene` | 新场景出现在 `m_CurrentDirectory` 下 |
| V10 | 连续三次 `Create / Material` | 生成 `New Material.lmat` / `New Material 1.lmat` / `New Material 2.lmat` |
| V11 | 右键目录 `Delete` | 目录被删；其下所有已注册资产也从 Registry 中移除；Inspector 若正显示这些资产则回到空态 |
| V12 | 右键资产 `Delete` | 磁盘文件 + Registry + 缓存三处同步清理 |
| V13 | `Show in Explorer` 对目录 / 资产 / 空白 | 目录：进入该目录；资产：打开父目录并选中；空白：打开当前浏览目录 |
| V14 | 任意位置 `Refresh` | 等价 `Ctrl+R`，目录树重建 |
| V15 | 删除掉 `m_CurrentDirectory` 或其祖先 | 自动回退到 `m_AssetsDirectory` |

---

## 十四、已知限制与后续扩展

1. **Rename 未实现**：第一阶段 Rename 项灰置；行内编辑与目录级联 rename 留到 Sprint 3。
2. **Duplicate 未实现**：需要 `AssetManager::CopyAsset` 接口配合，非本次范围。
3. **多选**：当前 `SelectionManager` 是单选模型，右键菜单也只对单一目标生效；未来引入多选后，`AssetContext` 需扩展为 `std::vector<AssetContext>` 或引入 `SelectionSet`。
4. **拖拽移动**：右键菜单外的能力，属于 §7 之外的独立话题（Move 已在 `AssetManager::MoveAsset`），未来做拖拽时无需改本文档结构。
5. **平台差异**：`RevealInExplorer` 仅实现 Windows；macOS / Linux 版本需要通过 `open` / `xdg-open` 分别处理，后续下沉到 `Lucky/Utils/PlatformUtils` 时补齐。
6. **性能**：`RebuildDirectoryTree` 目前是全量重建，Assets 目录很深时可能有轻微卡顿。后续可考虑"仅刷新变更目录节点"，与本次改动解耦。

---

## 附录 A：踩坑记录 ?? UI 遍历中不得直接重建数据源

### 现象

首次实现完成后，在**左侧目录树**上做 Create / Delete / Refresh 会触发 MSVC debug iterator 断言：

```
std::_Adl_verify_range<...>(const wchar_t *const &, const wchar_t *const &) xutility:1356
Lucky::ProjectAssetsPanel::DrawDirectoryTreeNode(...)   ProjectAssetsPanel.cpp:138
Lucky::ProjectAssetsPanel::DrawDirectoryTreeNode(...)   ProjectAssetsPanel.cpp:179
Lucky::ProjectAssetsPanel::OnGUI()                      ProjectAssetsPanel.cpp:74
```

右侧资产上的 Delete 不会触发（因为右侧走 `directory_iterator`，不复用 `m_RootNode` 的内存）。

### 根因

`DrawDirectoryTreeNode` 是递归绘制，正在 `for (DirectoryNode& subDir : node.SubDirectories)` 遍历子节点时，若子节点上的右键菜单触发了 `Delete / Create / Refresh`，这些操作最终都会走到 `RebuildDirectoryTree()`：

```cpp
void ProjectAssetsPanel::RebuildDirectoryTree()
{
    m_RootNode = BuildDirectoryNode(m_AssetsDirectory);   // 整棵树 destruct + rebuild
}
```

这一步会把包括当前 `node` 在内的**所有 DirectoryNode 对象整体析构 + 重建**，正在使用的 `node.SubDirectories` vector、`node.Name` 的 wchar_t 缓冲全部失效，MSVC 迭代器 debug 层立即抛出断言。

### 修复：Deferred CRUD 队列

**核心规则**：任何会改动 `m_RootNode` / `Registry` / `m_CurrentDirectory` 的写操作，**都不得在 UI 遍历中直接执行**，必须入队到帧末统一执行。

实现要点（[ProjectAssetsPanel.h](../../Luck3DApp/Source/Panels/ProjectAssetsPanel.h) / [ProjectAssetsPanel.cpp](../../Luck3DApp/Source/Panels/ProjectAssetsPanel.cpp)）：

```cpp
// header
std::vector<std::function<void()>> m_PendingActions;

void EnqueueAction(std::function<void()> action);
void FlushPendingActions();
```

```cpp
// cpp：OnGUI 末尾（ImGui::EndTable() 之后）
FlushPendingActions();

// cpp：EnqueueAction / FlushPendingActions
void ProjectAssetsPanel::EnqueueAction(std::function<void()> action)
{
    if (action)
    {
        m_PendingActions.push_back(std::move(action));
    }
}

void ProjectAssetsPanel::FlushPendingActions()
{
    if (m_PendingActions.empty())
    {
        return;
    }

    // 先 swap 到本地再执行：防止执行过程中回弹的 EnqueueAction 引发无限循环
    // 新入队项会留到下一帧执行
    std::vector<std::function<void()>> actions;
    actions.swap(m_PendingActions);

    for (const std::function<void()>& action : actions)
    {
        action();
    }
}
```

菜单里所有 CRUD 一律走 `EnqueueAction`：

```cpp
if (ImGui::MenuItem("Delete", "Del"))
{
    AssetContext captured = ctx;   // 拷贝一份：ctx 是临时引用，lambda 出栈后已失效
    EnqueueAction([this, captured]()
    {
        if (captured.Kind == AssetContextKind::Asset)
        {
            if (AssetManager::DeleteAsset(captured.Handle))
            {
                SelectionManager::Deselect();
                RebuildDirectoryTree();
            }
        }
        else if (captured.Kind == AssetContextKind::Directory)
        {
            DeleteFolderRecursive(captured.Path);
            SelectionManager::Deselect();
            RebuildDirectoryTree();
        }
    });
}
```

### 分派矩阵：菜单项是否需要入队

| 菜单项 | 是否入队 | 原因 |
|---|---|---|
| Create Folder / Material / Scene | ? | 会 `RebuildDirectoryTree()` |
| Delete（Asset / Directory） | ? | 会 `RebuildDirectoryTree()` + 修改 Registry |
| Refresh | ? | 会 `RebuildDirectoryTree()` |
| Show in Explorer | ? | 只是 `ShellExecute`，不动数据源 |
| 只读信息（Handle / Type / Path） | ? | 纯展示 |

Toolbar 的 Refresh 按钮、Ctrl+R 快捷键**同样入队**，保持"所有 refresh 触发点行为一致"的语义。

### 通用原则（后续所有 Panel 开发者请遵守）

1. **在 UI 遍历中修改被遍历的数据源，等价于在 for-range 中 `vector.clear()`**，是未定义行为。
2. Panel 内部凡是同时具备"递归绘制"与"右键菜单触发写操作"两个条件的，都要为写操作提供**延迟执行**机制。
3. SceneHierarchyPanel 已有先例：Delete Entity 走 `entityDeleted` 布尔标记 + 遍历后再 `DestroyEntity`。ProjectAssetsPanel 因菜单动作类型更多，采用更通用的 `std::function` 队列。
4. Lambda 捕获时，若菜单里持有 `const AssetContext&`（引用形参），**必须先拷贝到 lambda 值捕获**，否则出栈后引用悬空。

