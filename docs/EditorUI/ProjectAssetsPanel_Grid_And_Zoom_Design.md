# ProjectAssetsPanel：缩放工具条 + Grid 布局 设计文档

> **文档性质**：正式设计文档（Design）。只分析方案 / 不含最终代码落地。
> **代码风格**：全文代码示意严格遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)。
> **前置依赖**：PhaseR35 资产预览系统已完成（`AssetPreviewCache / AssetPreviewRenderer` 可用），缩略图 TexID 已经按 AssetHandle 缓存。
> **本文档不含**：缩略图渲染实现本身（归 PhaseR35）、滑动条取值持久化到 Preferences（归 §9 后续）。

---

## 1. 概述

### 1.1 目标

目前 Project 面板右侧内容区只有一种"列表布局"：每行一个 `TreeNode`，图标约 14~16px。资产变多后视觉识别效率低。本阶段：

- 面板**右侧内容区底部**新增一条 Unity 风格的工具栏，右端放一个**缩放滑动条**。
- 滑动条控制**图标尺寸**，**连续缩放**，跨越列表 / Grid 两种布局：
  - 滑动条最小值 = **列表布局**（图标 ~16px）
  - 滑动条大于最小值的任何位置 = **Grid 布局**，图标尺寸 = 当前滑动条值（连续，无吸附）
  - 滑动条最大值 = **128px** 的 Grid 布局
- Grid 布局要**完整继承**现有列表布局的交互：命中选中、内联重命名、拖拽源 / 目标、右键菜单、Pending Create 占位、双击打开目录 / 资产。
- **Grid 的两个核心基元抽到 UI 层**（`Lucky/Source/Lucky/UI/`），命名对齐现有 `BeginRenamableTreeNode`，供后续"资产选择面板"等场景复用：
  - `UI::BeginGrid / UI::EndGrid` —— Grid 布局原语（RAII），负责算列数 / SameLine 换行
  - `UI::BeginRenamableGridItem / UI::EndRenamableGridItem` —— 带内联重命名的单元格控件

### 1.2 前置依赖

- `AssetPreviewCache::GetOrRender(handle, type)` 可返回独立 `Ref<Texture2D>`（PhaseR35 已完成）
- `ProjectAssetsPanel::GetThumbnail(path)` 已按资产类型分派并落到缩略图缓存（PhaseR35 已完成）
- `UI::RenameController<path>` 支持跨布局复用（已完成，内部与具体节点绘制解耦）

### 1.3 本阶段不做

- **不做**滑动条值的持久化（关闭编辑器重启后回到最小值 = 列表）。P5 阶段挂到 `EditorPreferences`。
- **不做**搜索框、过滤器、Breadcrumb 等 Unity 底栏其他控件 —— 工具栏结构留口子，内容只放一个滑动条。
- **不做**异步缩略图渲染预算（Grid 布局首帧可能一次性 miss ≥ 20 张，归 PhaseR35.P5）。
- **不改**左侧目录树（继续用 TreeNode）。
- **不改**既有 `BeginRenamableTreeNode` 和 `RenameController` 签名 —— 新增的 `UI::BeginGrid` / `UI::BeginRenamableGridItem` 完全是**新增**原语，和既有 UI 控件平级，互不影响。

### 1.4 子阶段产出

| 子阶段 | 关键产出 | 验证方式 |
|---|---|---|
| **G1 底部工具栏骨架** | 右侧内容区拆成"内容区 + 底部工具栏"；工具栏右端 Slider 控制 `m_IconSize ∈ [16, 128]` | 面板下方出现 ~24px 工具栏，右端有 Slider；拖动时 `m_IconSize` 连续变化；原列表布局保持 |
| **G2 UI 原语：BeginGrid / BeginRenamableGridItem** | `Lucky/UI/Widgets.h` 新增 `UI::BeginGrid` / `UI::EndGrid`；`UI::BeginRenamableGridItem` / `UI::EndRenamableGridItem` | 单元测试式小 demo（临时面板）能跑通：按给定 cellSize 画一堆带图标 + 名字的格子，自动换行、支持 Rename、支持命中选中 |
| **G3 Project 面板接入 Grid** | `DrawContentArea` 按 `m_IconSize` 分派：等于最小值走列表、否则走 Grid；Grid 用 §G2 的 UI 原语；交互对齐 List 版本 | Slider 稍一拖动就从列表切 Grid；继续拖动图标连续变大到 128px；Grid 下所有交互（选中 / 重命名 / 拖拽 / 右键菜单 / Pending Create / 空白点击）与 List 一致 |

---

## 2. 总体架构

### 2.1 面板布局图

```
┌─────────────────────────── Project 面板 ────────────────────────────┐
│  ┌─────────────────────────── 顶部工具栏 ─────────────────────────┐ │
│  │  [Refresh]                                                      │ │
│  └────────────────────────────────────────────────────────────────┘ │
│  ┌─────────────────────── 主体 Table（2 列） ──────────────────────┐ │
│  │  ┌── 左：目录树 ──┐ │ ┌────── 右：内容区 BeginChild ──────────┐│ │
│  │  │  ├ Assets       │ │ │ ┌─────────── 内容区（上） ──────────┐ ││ │
│  │  │  │  ├ Materials │ │ │ │   list 或 grid（按 m_IconSize 分派）│ ││ │
│  │  │  │  ├ Meshes    │ │ │ │                                   │ ││ │
│  │  │  │  └ Scenes    │ │ │ └───────────────────────────────────┘ ││ │
│  │  │                 │ │ │ ┌───────── 底部工具栏（新增） ──────┐ ││ │
│  │  └─────────────────┘ │ │ │                [======o======]128 │ ││ │
│  │                      │ │ └───────────────────────────────────┘ ││ │
│  │                      │ └────────────────────────────────────────┘│ │
│  └────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────┘
```

### 2.2 控件分层

```
ProjectAssetsPanel::OnGUI
  ├─ DrawToolbar()                       （顶部工具栏，已有：Refresh 按钮）
  └─ BeginTable
       ├─ 左列：DrawDirectoryTreeNode    （已有）
       └─ 右列：BeginChild "ContentArea"
            │
            ├─ BeginChild "Content" {0, -toolbarH}   ★ 新增：留出底部空间
            │    └─ 按 m_IconSize 分派：
            │         ├─ 等于最小值 → DrawContentArea_List   （现有流程 DrawAssetItem）
            │         └─ 大于最小值 → DrawContentArea_Grid   （用 UI 原语绘制）
            │              └─ UI::BeginGrid
            │                   ├─ for item in entries:
            │                   │     UI::BeginRenamableGridItem(icon, name, ...)
            │                   │       ApplyAssetItemInteractions(...)   （拖拽源 / 右键 / 双击）
            │                   │     UI::EndRenamableGridItem()
            │                   └─ UI::EndGrid
            │
            └─ BeginChild "BottomToolbar" {0, toolbarH}  ★ 新增
                 └─ DrawBottomToolbar()
                      └─ SliderFloat(##Zoom, &m_IconSize, kMin, kMax)
```

### 2.3 Grid UI 原语的分工

| 原语 | 职责 |
|---|---|
| **`UI::BeginGrid(id, cellWidth, cellHeight)`** | 读 `GetContentRegionAvail().x` 算列数、压一个 Grid Context 栈帧（cellWidth / cellHeight / spacing / 当前列号）；返回 bool 表示是否成功进入 |
| **`UI::EndGrid()`** | 清理 Grid Context、补一个 `NewLine` 刷掉最后一行的 `SameLine` 状态 |
| **`UI::BeginRenamableGridItem(icon, name, displayName, id, selected, rename, onCommit, scopeTag, outClickOutcome, onCancel)`** | 从 Grid Context 栈顶读 cellSize，内部决定是否 SameLine 换行；画背景（Hover/Selected 高亮）；画缩略图；画名字 or 内联 InputText（Rename 态）；用 `InvisibleButton` 占满 cell bb 作为交互锚 —— 后续 `BeginDragDropSource / BeginPopupContextItem` 都挂到这个 Item 上 |
| **`UI::EndRenamableGridItem()`** | 收尾（Grid 列号 +1；若已到行末准备下行；此时不 SameLine，下次 BeginItem 时才判） |

---

## 3. 关键设计决策

### 3.1 滑动条的数据模型

- **方案 A（推荐）**：**连续浮点 `SliderFloat`**
  - `float m_IconSize ∈ [16.0f, 128.0f]`，默认 `16.0f`（= 最小值 = 列表）
  - 布局分派规则：
    - `m_IconSize <= kListThreshold` → **列表布局**（kListThreshold = kMin + 0.5f 容错）
    - `m_IconSize >  kListThreshold` → **Grid 布局**，图标尺寸 = `m_IconSize`（连续）
  - Slider 用 `ImGui::SliderFloat("##Zoom", &m_IconSize, kMin, kMax, "")` 即可，无需任何特殊 snap
  - 优点：
    - 用户体验最平滑：拖动过程中图标大小连续变化，无"卡顿"感
    - 实现最简洁，无 snap / 死区特殊逻辑
    - 列表 ↔ Grid 的切换天然发生在滑动条紧邻最小值的位置（往右一拖就 Grid），和 Unity 大体一致
  - 缺点：
    - "刚离开最小值"时 Grid 图标会非常小（17px、18px），审美上不如大图标；但这是用户自己选的尺寸，尊重用户意图
    - 相比离散档位，没有吸附感 —— 本阶段明确要连续缩放，这是预期行为
- **方案 B**：离散整型档位 `SliderInt(0..5)`，档位 0 列表、1~5 查表 32/48/64/96/128
  - 优点：吸附感强、Grid 下图标尺寸都是"审美上舒服"的大小
  - 缺点：和本阶段要求"正常连续缩放、不做 2 倍吸附"冲突；档位数固定不灵活
- **方案 C**：两个控件分开：布局模式按钮 + Grid 下的尺寸 Slider
  - 优点：语义最明确
  - 缺点：违背"单条滑动条承载"的 Unity 风格

**决策**：**采用方案 A**。常量定义：

```cpp
constexpr static float s_IconSizeMin        = 16.0f;    // Slider 最小值 = 列表布局
constexpr static float s_IconSizeMax        = 128.0f;   // Slider 最大值
constexpr static float s_ListThresholdEps   = 0.5f;     // 浮点容差：m_IconSize <= kMin + eps 判定为列表

bool IsListLayout() const { return m_IconSize <= s_IconSizeMin + s_ListThresholdEps; }
```

> **为什么最小值选 16 而不是 `GetTextLineHeight() - TreeNodeIconSizeShrink`**：
> 后者依赖 ImGui 当前字体，运行时值 ~12~14px；用一个 constexpr 固定值更稳定，Slider 的最小值 label 显示也固定。实测列表 TreeNode 在 16px 下并不影响观感（图标仍然按 `fontSize - 4` 自算，Slider 的最小值只是用来**判定布局分支**，不传给 TreeNode）。

### 3.2 滑动条在工具栏的位置

- **方案 A（推荐）**：**右端对齐**，一条底部工具栏右侧 ~140px
  - 布局：`SetCursorPosX(availW - sliderWidth - padding)` → `SetNextItemWidth(sliderWidth)` → `SliderFloat("##Zoom", ...)`
  - 优点：对齐 Unity Project 面板；为未来左侧放"视图模式按钮"、中部放"路径面包屑"预留空间
  - 缺点：无
- **方案 B**：**左端对齐 / 居中**
  - 缺点：工具栏左侧按惯例是"主要功能按钮"，塞 Zoom 不规范

**决策**：**方案 A**。工具栏默认高度 `Theme::Layout::AssetFieldHeight = 28.0f` 可以直接复用（新增一个常量 `ProjectBottomToolbarHeight = 24.0f` 更合适，不污染 AssetField）。

### 3.3 Grid 布局 UI 原语的 API 形态

**诉求**：Grid 不只是 Project 面板用，后续"资产选择面板"等也会用。两个方向的问题要定：
1. **`BeginGrid` 的职责边界**：是否要管 Item 的具体渲染？
2. **`BeginRenamableGridItem` 和 `BeginGrid` 的耦合方式**：Item 怎么知道当前所在 Grid 的 cellSize / 当前列号？

**方向 1 — BeginGrid 的职责**

- **方案 A（推荐）**：**只管布局**（列数计算 + SameLine 换行调度），不管 Item 具体渲染
  - BeginGrid 维护一个 Grid Context（cellWidth / cellHeight / spacing / columnCount / currentColumn）
  - Item 可以是任何控件：`BeginRenamableGridItem` / `BeginRenamableTreeNode`（如果以后有 Tree-in-Grid 需求）/ 用户自己的 `Image + Button` 组合 / Pending Create 占位
  - 优点：职责单一、可扩展、未来可以在 Grid 里放非 "Renamable" 的轻量格子
  - 缺点：Item 需要显式"告诉" Grid Context 自己占了一格（通过 `End*GridItem`）
- **方案 B**：**BeginGrid 接管渲染**，接收 items 容器 + 渲染回调（函数式）
  - `UI::Grid(id, cellSize, items, [](const Item& it){ ... draw ... });`
  - 优点：调用方代码最简
  - 缺点：items 类型必须统一；无法在中间插入特殊 Item（如 Pending Create）；回调签名难统一

**方向 2 — Item 到 Grid 的耦合方式**

- **方案 A（推荐）**：**Grid Context 用进程内栈**（`std::vector<GridContext>`）
  - `BeginGrid` push 一个 Context，`EndGrid` pop
  - `BeginRenamableGridItem` 从栈顶读 Context 决定换行 / 退出 Grid 后调用会 assert
  - 优点：Item API 不需要显式传 Grid handle，调用形式自然；ImGui 本身大量使用这种模式（ID Stack / Style Stack）
  - 缺点：隐式状态；但本项目已有 `UI::GenerateID` 的 ScopedStack 模式作先例
- **方案 B**：BeginGrid 返回一个显式 Context 对象，Item 显式接收
  - `auto ctx = UI::BeginGrid(...); UI::BeginRenamableGridItem(ctx, ...);`
  - 优点：无隐式状态
  - 缺点：每个 Item 调用多传一个参，调用方代码变繁；RAII 析构麻烦
- **方案 C**：Grid 不维护状态，用户手动 SameLine
  - 优点：零封装
  - 缺点：封装收益为零，与"抽成通用控件"初衷矛盾

**决策**：**方向 1 用方案 A（只管布局），方向 2 用方案 A（静态栈）**。
结果 API 形态：

```cpp
if (UI::BeginGrid("##Projects", /*cellWidth*/ cellW, /*cellHeight*/ cellH))
{
    for (auto& entry : entries)
    {
        if (UI::BeginRenamableGridItem(
                icon, strID.c_str(), displayName, path,
                isSelected, m_Rename, onCommit,
                /*scopeTag*/ kScopeContent, &clickOutcome, onCancel))
        {
            // 挂拖拽源 / 右键菜单 / 其他 ImGui 操作到当前 Item
            ApplyAssetItemInteractions(path, handle, isDirectory);
            UI::EndRenamableGridItem();
        }
    }

    // 可选：Pending Create 占位（也是一个 Grid Item）
    if (hasPendingCreate)
    {
        UI::BeginRenamableGridItem(pendingIcon, "##pending", initialName, virtualPath, ...);
        UI::EndRenamableGridItem();
    }

    UI::EndGrid();
}
```

**Grid Context 栈** 实现要点（放在 `Lucky/UI/Widgets.cpp` 的匿名命名空间，不暴露给外部）：

```cpp
namespace
{
    struct GridContext
    {
        float   CellWidth;
        float   CellHeight;
        float   Spacing;
        int     ColumnCount;
        int     CurrentColumn;
    };
    static std::vector<GridContext> s_GridStack;
}
```

`BeginGrid` 伪代码（UI 层内部，用户不直接调用）：

```cpp
bool BeginGrid(const char* id, float cellWidth, float cellHeight, float spacing /*= 8.0f*/)
{
    float availW = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, static_cast<int>((availW + spacing) / (cellWidth + spacing)));

    s_GridStack.push_back({ cellWidth, cellHeight, spacing, cols, 0 });
    ImGui::PushID(id);
    return true;    // 预留 return false（例如面板宽度 <= 0 时）
}

void EndGrid()
{
    LF_CORE_ASSERT(!s_GridStack.empty(), "EndGrid without BeginGrid");
    ImGui::PopID();
    s_GridStack.pop_back();
    ImGui::NewLine();   // 刷掉最后一行残留的 SameLine 状态
}
```

`BeginRenamableGridItem` / `EndRenamableGridItem` 内部用 `s_GridStack.back()` 做 SameLine 调度（细节见 §4.5）。

> **关于"静态栈"的线程安全**：项目所有 ImGui 调用都在主线程 UI 阶段串行执行，与 ImGui 本身的全局 Context 等价，不需要 thread_local。

### 3.4 列表 / Grid 的代码共用策略

现有 `DrawAssetItem` 把"图标 + 名字 + 命中 + 重命名 + 拖拽 + 右键"全部耦合在一起（走 `BeginRenamableTreeNode`）。Grid 布局要用新 UI 原语绘制 + 共用交互。

- **方案 A（推荐）**：**UI 层新增通用控件 + 面板层抽 Helper 共用交互**
  - UI 层新增：`BeginGrid / EndGrid` / `BeginRenamableGridItem / EndRenamableGridItem`
  - 面板层拆：
    - `DrawAssetItem_List(entry)` ← 现有 `DrawAssetItem` 拆出来的内容
    - `DrawAssetItem_Grid(entry, iconSize)` ← 新增，内部走 `UI::BeginRenamableGridItem`
    - `ApplyAssetItemInteractions(path, handle, isDirectory)` ← 两者共用的拖拽源 / 右键菜单 / 双击打开
  - 优点：
    - 两个布局的渲染清晰分开
    - UI 层 Grid 原语可被其他面板（如"资产选择面板"）直接复用
    - 共用交互集中在 helper，不散
  - 缺点：代码量比"全写在面板里"稍多，但复用价值明确
- **方案 B**：`DrawAssetItem` 内部按 `m_IconSize` 分支
  - 缺点：函数变成巨型分支；UI 原语无法被其他面板复用
- **方案 C**：Grid 相关都写在面板里，不抽 UI 层
  - 缺点：和"后续其他面板也要用"的目标冲突

**决策**：**方案 A**。
- `DrawContentArea` 分派：
  ```cpp
  if (IsListLayout())
  {
      DrawContentArea_List();
  }
  else
  {
      DrawContentArea_Grid(m_IconSize);
  }
  ```
- `DrawAssetItem_Grid` 的职责**薄**：只负责按 `UI::BeginRenamableGridItem` 的签名把 icon / name / displayName / handle / onCommit 等材料准备好 → 调用 UI 原语 → 挂 `ApplyAssetItemInteractions`。

### 3.5 Grid 单元格的内部结构

```
┌─────────────────────┐  ← 单元格 cellSize × (cellSize + nameRows)
│                     │
│    ┌───────────┐    │  ← 缩略图区域：iconSize × iconSize
│    │           │    │     居中对齐，上下左右 padding
│    │  thumb    │    │
│    │           │    │
│    └───────────┘    │
│                     │
│    AssetName.lmat   │  ← 名字区：1~2 行，超长省略号
│                     │
└─────────────────────┘
```

单元格尺寸计算（UI 层常量定义在 `Lucky/UI/Theme.h` 的 `Layout` namespace）：

```cpp
// Theme.h
constexpr float GridItemPaddingX = 8.0f;        // 单元格水平内边距
constexpr float GridItemPaddingY = 6.0f;        // 单元格垂直内边距
constexpr int   GridItemNameRows = 2;           // 名字最多 2 行
constexpr float GridItemSpacing = 8.0f;         // 单元格之间间距

// 调用方传给 UI::BeginGrid 的 cellWidth / cellHeight 计算（示例）
float textRowH = ImGui::GetTextLineHeight();
float cellW = iconSize + Theme::Layout::GridItemPaddingX * 2.0f;
float cellH = iconSize + Theme::Layout::GridItemPaddingY * 2.0f + textRowH * Theme::Layout::GridItemNameRows;
```

### 3.6 内联重命名在 Grid 布局下的位置

- 列表布局：InputText 覆盖"图标右侧的名字"位置（已有）
- Grid 布局：InputText 覆盖"图标下方的名字"位置
- 走通过 `UI::RenameController<path>::IsEditing(path, kScopeContent)` 判断，命中则在名字位置画 `ImGui::InputText` 代替 `AddText`
- `RenameController` 本身跨布局无需改动（它只记录状态，不绘制控件）
- `UI::DrawInlineInputIfEditing` 走通过 `ImGui::SetKeyboardFocusHere` + `ImGui::InputText` 实现，这些 API 和布局无关，可直接复用

### 3.7 拖拽源 / 右键菜单 / 双击打开

全部走 ImGui 原生 API，和"刚才那个 Item"关联：

```cpp
// Grid Item 伪代码
ImGui::BeginGroup();
    ImGui::InvisibleButton(id, cellSize);
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool doubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered();
    // 画缩略图 + 名字
    ...
ImGui::EndGroup();

// 拖拽源 / 右键菜单 attach 到 Item
if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
{
    ImGui::SetDragDropPayload(DragDrop::AssetHandle, &handle, sizeof(handle));
    UI::DragDropPreview(...);
    ImGui::EndDragDropSource();
}

if (ImGui::BeginPopupContextItem((id + "##ctx").c_str()))
{
    DrawAssetContextMenu(MakeContext(AssetContextKind::Asset, path, handle));
    ImGui::EndPopup();
}
```

这些语义现有列表布局已经验证过，Grid 直接复用。

### 3.8 持久化策略（本阶段不做）

- Unity 把档位存 `EditorPrefs`（machine 级）
- Luck3D 可以存：
  - **A**：`EditorPreferences`（已有基础设施）
  - **B**：当前 `Project` 配置
  - **C**：本阶段不做，MVP 重启回到默认档位 0（列表）
- **决策**：**C**。减少本阶段范围，后续 P5 挂到 `EditorPreferences`。

---

## 4. 模块与 API 设计

### 4.1 文件 / 模块布局

| 层 | 文件 | 新增内容 |
|---|---|---|
| **UI 层** | `Lucky/Source/Lucky/UI/Widgets.h` | 声明 `BeginGrid / EndGrid`、模板 `BeginRenamableGridItem<TId, FnCommit, FnCancel>` / `EndRenamableGridItem` |
| **UI 层** | `Lucky/Source/Lucky/UI/Widgets.cpp` | 实现 `BeginGrid / EndGrid`；GridContext 静态栈；`BeginRenamableGridItem` 的非模板后端（SameLine 调度、cell bb 分配、命中、画背景、画图标、画名字 / InputText 覆盖）；私有 helper `Detail::BeginGridCellPlacement` 等 |
| **UI 层** | `Lucky/Source/Lucky/UI/Theme.h` | 新增 `GridItemPaddingX / Y / NameRows / Spacing` 四个常量 |
| **面板层** | `Luck3DApp/Source/Panels/ProjectAssetsPanel.h` | 新增成员 `m_IconSize` + 常量 + 3 个 private 函数声明 |
| **面板层** | `Luck3DApp/Source/Panels/ProjectAssetsPanel.cpp` | 实现 `DrawBottomToolbar` / `DrawContentArea_List` / `DrawContentArea_Grid` / `DrawAssetItem_Grid` / `ApplyAssetItemInteractions`；重构 `OnGUI` 右侧内容区 |

### 4.2 ProjectAssetsPanel 新增成员

```cpp
// ProjectAssetsPanel.h private 节内

// ---- Zoom 滑动条 + 布局分派 ----
constexpr static float s_IconSizeMin        = 16.0f;    // = 列表布局锚点
constexpr static float s_IconSizeMax        = 128.0f;   // Grid 最大图标尺寸
constexpr static float s_ListThresholdEps   = 0.5f;     // 浮点容差
constexpr static float s_BottomToolbarHeight = 24.0f;

float m_IconSize = s_IconSizeMin;    // 默认列表布局

bool IsListLayout() const { return m_IconSize <= s_IconSizeMin + s_ListThresholdEps; }

// ---- 新增函数 ----
void DrawBottomToolbar();
void DrawContentArea_List();
void DrawContentArea_Grid(float iconSize);
void DrawAssetItem_Grid(const std::filesystem::directory_entry& entry, float iconSize);
void ApplyAssetItemInteractions(const std::filesystem::path& path, AssetHandle handle, bool isDirectory);
```

### 4.3 UI 层：BeginGrid / EndGrid

```cpp
// Lucky/Source/Lucky/UI/Widgets.h（在 BeginRenamableTreeNode 下方新增）

/// <summary>
/// Grid 布局原语：按给定 cellWidth / cellHeight 自动算列数，内部 Item 自动 SameLine 换行
/// 必须和 EndGrid 配对。不嵌套（当前实现要求栈深 ≤ 1；嵌套需求出现再放宽）。
/// </summary>
/// <param name="id">ImGui ID（与同作用域其他控件隔离）</param>
/// <param name="cellWidth">单元格宽度（含内边距）</param>
/// <param name="cellHeight">单元格高度</param>
/// <param name="spacing">单元格之间间距，默认 Theme::Layout::GridItemSpacing</param>
/// <returns>当前帧 Grid 是否可绘制（面板宽度足够放至少 1 个格子时 true）</returns>
bool BeginGrid(const char* id, float cellWidth, float cellHeight, float spacing = Theme::Layout::GridItemSpacing);

/// <summary>
/// 结束 Grid，必须和 BeginGrid 配对
/// </summary>
void EndGrid();
```

### 4.4 UI 层：BeginRenamableGridItem / EndRenamableGridItem

模板签名对齐 `BeginRenamableTreeNode`，去掉 TreeNode 专有的 `isLeaf / defaultOpen / drawRightSide`：

```cpp
// Lucky/Source/Lucky/UI/Widgets.h

/// <summary>
/// Grid 单元格 + 内联重命名：从 Grid 栈顶读 cellSize 决定布局，InvisibleButton 占满 bb
/// </summary>
/// <typeparam name="TId">RenameController 的 ID 类型（通常 std::filesystem::path 或 UUID）</typeparam>
/// <typeparam name="FnCommit">改名回调签名 void(const std::string&)</typeparam>
/// <typeparam name="FnCancel">Esc / 失焦取消回调签名 void()，可选</typeparam>
/// <param name="icon">单元格缩略图</param>
/// <param name="name">ImGui ID 字符串（需在 Grid 作用域内唯一）</param>
/// <param name="displayName">显示名（Rename 态下 InputText 的初始内容）</param>
/// <param name="id">RenameController 的目标 ID</param>
/// <param name="selected">是否选中态</param>
/// <param name="rename">RenameController 实例</param>
/// <param name="onCommit">改名提交回调</param>
/// <param name="scopeTag">作用域标签（和列表版一致，0 = 不区分）</param>
/// <param name="outClickOutcome">点击结果输出（可 nullptr）</param>
/// <param name="onCancel">取消回调（可选）</param>
/// <returns>true 表示 Item 已入栈，必须调用 EndRenamableGridItem 配对</returns>
template <typename TId, typename FnCommit, typename FnCancel = std::nullptr_t>
bool BeginRenamableGridItem(
    const Ref<Texture2D>&       icon,
    const char*                 name,
    const std::string&          displayName,
    const TId&                  id,
    bool                        selected,
    RenameController<TId>&      rename,
    FnCommit&&                  onCommit,
    int                         scopeTag = 0,
    RenameClickOutcome*         outClickOutcome = nullptr,
    FnCancel&&                  onCancel = nullptr);

/// <summary>
/// 结束 Grid Item：把当前列号推进；若达到行末，下一次 BeginItem 自动换行
/// </summary>
void EndRenamableGridItem();
```

**内部关键步骤**（Widgets.cpp 实现）：

```cpp
template <typename TId, typename FnCommit, typename FnCancel>
bool BeginRenamableGridItem(...)
{
    LF_CORE_ASSERT(!s_GridStack.empty(), "BeginRenamableGridItem must be inside BeginGrid/EndGrid");
    GridContext& ctx = s_GridStack.back();

    // 1) SameLine 调度：非当行第一个 Item 时 SameLine
    if (ctx.CurrentColumn > 0)
    {
        ImGui::SameLine(0.0f, ctx.Spacing);
    }

    bool isRenaming = rename.IsEditing(id, scopeTag);

    // 2) 分配 cell bb：InvisibleButton 占满 cellWidth × cellHeight
    ImVec2 cellMin = ImGui::GetCursorScreenPos();
    ImVec2 cellSize(ctx.CellWidth, ctx.CellHeight);
    ImVec2 cellMax(cellMin.x + ctx.CellWidth, cellMin.y + ctx.CellHeight);

    ImGui::PushID(name);
    ImGui::InvisibleButton("##cell", cellSize);
    bool isHovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    // 3) 画背景：选中态 / Hover 态
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (selected)
    {
        dl->AddRectFilled(cellMin, cellMax, Theme::Colors::SelectionFill, Theme::Layout::ChildRounding);
    }
    else if (isHovered)
    {
        dl->AddRectFilled(cellMin, cellMax, Theme::Colors::HoverFill, Theme::Layout::ChildRounding);
    }

    // 4) 画图标：居中上部，iconSize = cellWidth - 2 * padding
    float padX = Theme::Layout::GridItemPaddingX;
    float padY = Theme::Layout::GridItemPaddingY;
    float iconSize = ctx.CellWidth - padX * 2.0f;
    ImVec2 iconMin(cellMin.x + padX, cellMin.y + padY);
    ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);
    if (icon)
    {
        dl->AddImage(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(icon->GetRendererID())), iconMin, iconMax);
    }

    // 5) 画名字 or InputText：名字区在图标下方，2 行高度
    ImVec2 nameMin(cellMin.x + padX, iconMax.y + padY);
    ImVec2 nameMax(cellMax.x - padX, cellMax.y - padY);
    if (isRenaming)
    {
        // 把 ImGui cursor 定位到名字区左上角，画 InputText 覆盖
        rename.DrawInlineInputIfEditing(id, scopeTag, nameMin, nameMax, onCommit, onCancel);
    }
    else
    {
        // 画省略号截断的居中文本
        Detail::DrawTruncatedCenteredText(dl, nameMin, nameMax, displayName.c_str(), Theme::Layout::GridItemNameRows);
    }

    // 6) 命中结果上报
    if (outClickOutcome && clicked)
    {
        *outClickOutcome = RenameClickOutcome::Clicked;    // 或根据 rename 状态细分
    }

    // 不 PopID；交由 EndRenamableGridItem 完成
    return true;
}

void EndRenamableGridItem()
{
    ImGui::PopID();

    GridContext& ctx = s_GridStack.back();
    ++ctx.CurrentColumn;
    if (ctx.CurrentColumn >= ctx.ColumnCount)
    {
        ctx.CurrentColumn = 0;    // 下一次 BeginItem 的 SameLine 条件 CurrentColumn > 0 不满足，自动换行
    }
}
```

> **注意事项**（实现时务必核对）：
> - `RenameController<TId>::DrawInlineInputIfEditing` 当前的签名是针对 TreeNode 的（InputText 覆盖"图标右侧"的某个像素位置）。Grid 场景的 InputText 位置不同（图标下方），需要给 `RenameController` 补一个"任意屏幕矩形"的重载，或者在 `BeginRenamableGridItem` 内部手动处理 Rename 态的 InputText 绘制 + 提交 / 取消回调分派。推荐做法：在 Widgets.cpp 内加一个 `Detail::DrawInlineInputAtRect(min, max, buffer, imguiID, onCommit, onCancel)` helper，两种控件共用。
> - `InvisibleButton` 的 ID 用 `"##cell"` 配合外层 `PushID(name)` 保证跨帧稳定。

### 4.5 DrawBottomToolbar

```cpp
void ProjectAssetsPanel::DrawBottomToolbar()
{
    // 右端对齐 Slider：固定宽度 ~140px，距右边界 8px
    constexpr float sliderWidth = 140.0f;
    constexpr float rightMargin = 8.0f;

    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availW - sliderWidth - rightMargin);
    ImGui::SetNextItemWidth(sliderWidth);
    ImGui::SliderFloat("##ProjectZoom", &m_IconSize, s_IconSizeMin, s_IconSizeMax, "");
}
```

### 4.6 右侧内容区拆分（替换现有 BeginChild "##ContentArea" 内部）

```cpp
ImGui::BeginChild("##ContentArea", { 0, 0 });
{
    // 内容区：留出底部工具栏高度
    ImGui::BeginChild("##Content", { 0, -s_BottomToolbarHeight });
    {
        DrawContentArea();        // 内部按 IsListLayout() 分派
        // ... 空白点击 / 取消选中（原有逻辑）
    }
    ImGui::EndChild();

    // 底部工具栏
    ImGui::BeginChild("##BottomToolbar", { 0, s_BottomToolbarHeight });
    {
        DrawBottomToolbar();
    }
    ImGui::EndChild();
}
ImGui::EndChild();
```

### 4.7 DrawContentArea 内部分派

```cpp
void ProjectAssetsPanel::DrawContentArea()
{
    if (m_CurrentDirectory.empty())
    {
        return;
    }

    if (IsListLayout())
    {
        DrawContentArea_List();
    }
    else
    {
        DrawContentArea_Grid(m_IconSize);
    }
}

void ProjectAssetsPanel::DrawContentArea_List()
{
    for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
    {
        DrawAssetItem(entry);       // 保持现有列表绘制
    }
    DrawPendingCreatePlaceholder_List();    // 原有 Pending Create 占位抽成函数
}

void ProjectAssetsPanel::DrawContentArea_Grid(float iconSize)
{
    float textRowH = ImGui::GetTextLineHeight();
    float cellW = iconSize + Theme::Layout::GridItemPaddingX * 2.0f;
    float cellH = iconSize + Theme::Layout::GridItemPaddingY * 2.0f + textRowH * Theme::Layout::GridItemNameRows;

    if (!UI::BeginGrid("##ProjectsGrid", cellW, cellH))
    {
        return;
    }

    for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
    {
        DrawAssetItem_Grid(entry, iconSize);
    }
    DrawPendingCreatePlaceholder_Grid(iconSize);

    UI::EndGrid();
}
```

### 4.8 DrawAssetItem_Grid（关键：薄壳 → UI 原语）

```cpp
void ProjectAssetsPanel::DrawAssetItem_Grid(const std::filesystem::directory_entry& entry, float iconSize)
{
    const std::filesystem::path& path = entry.path();
    bool isDirectory = entry.is_directory();

    // 1) 图标（目录 → 文件夹图标；非目录 → 缩略图优先，fallback TypeIcon）
    Ref<Texture2D> icon = /* 同 DrawAssetItem 的分派逻辑 */;

    AssetHandle assetHandle;
    if (!isDirectory)
    {
        assetHandle = AssetManager::GetAssetHandle(Project::GetActive()->MakeRelative(path));
    }

    bool isSelected = (isDirectory ? SelectionManager::IsFolderSelected(path) : SelectionManager::IsAssetSelected(assetHandle));

    std::string strID = isDirectory
        ? path.filename().string()
        : std::format("{}##{}", path.stem().string(), static_cast<uint64_t>(assetHandle));

    std::string displayName = path.stem().string();

    // 2) 调 UI 原语绘制单元格 + 内联重命名
    UI::RenameClickOutcome clickOutcome = UI::RenameClickOutcome::None;
    bool opened = UI::BeginRenamableGridItem(
        icon, strID.c_str(), displayName, path,
        isSelected, m_Rename,
        [this, isDirectory, path, assetHandle](const std::string& newName)
        {
            if (isDirectory)
            {
                EnqueueAction([this, path, newName]() { RenameFolderTo(path, newName); });
            }
            else if (assetHandle.IsValid())
            {
                EnqueueAction([this, assetHandle, newName]() { RenameAssetTo(assetHandle, newName); });
            }
        },
        /*scopeTag*/ kScopeContent,
        &clickOutcome);

    if (opened)
    {
        // 3) 挂拖拽源 / 右键菜单 / 双击打开（和 List 版共用 helper）
        ApplyAssetItemInteractions(path, assetHandle, isDirectory);

        UI::EndRenamableGridItem();
    }

    // 4) 跨布局统一的点击结果处理（选中 / 启动 Rename）
    if (clickOutcome == UI::RenameClickOutcome::Clicked)
    {
        if (isDirectory) { SelectionManager::SelectFolder(path); }
        else if (assetHandle.IsValid()) { SelectionManager::SelectAsset(assetHandle); }
    }
}
```

> **要点**：
> - 这个函数是**薄壳**：所有渲染细节（画背景、画图标、画名字 / InputText）都在 UI 原语里。
> - 面板只负责：**准备材料**（icon / handle / displayName / 回调）+ **挂交互**（拖拽源 / 右键菜单 / 双击）+ **处理点击结果**（SelectionManager）。
> - **双击打开**由 `ApplyAssetItemInteractions` 内部 `IsMouseDoubleClicked + IsItemHovered` 处理。

### 4.9 ApplyAssetItemInteractions（Helper）

把 List 和 Grid 共用的拖拽源 / 右键菜单 / 双击打开抽成一个 helper。签名：

```cpp
void ProjectAssetsPanel::ApplyAssetItemInteractions(
    const std::filesystem::path& path,
    AssetHandle handle,
    bool isDirectory);
```

内部按现有 `DrawAssetItem` 的后半段（第 398 行开始的 `BeginDragDropSource` / `BeginPopupContextItem` / 双击 NavigateTo 等）封装。要点：
- 调用方约定：此 helper 必须在**当前 Item 刚绘制完**（InvisibleButton / TreeNode / GridItem 任一）时立即调用，这样 `BeginDragDropSource` / `BeginPopupContextItem` 关联到正确的 Item
- helper 内部不处理 "选中 / Rename 启动"这类"点击结果判定"（那些在调用方按 clickOutcome 分派），只做"消费当前 Item 的拖拽 / 右键 / 双击" 这类副作用

---

## 5. 交互行为细节

### 5.1 Slider 连续缩放规则

- `SliderFloat` 连续浮点，拖动过程中 `m_IconSize` 任意位置都有效
- 布局分派只有一个判定：`IsListLayout() = (m_IconSize <= s_IconSizeMin + 0.5f)`
  - 刚离开最小值时，`m_IconSize` 可能是 16.1、16.2… → `IsListLayout()` 为 false → Grid 布局，图标尺寸 ≈ 17px
  - 继续向右拖，Grid 图标连续变大到 128px
  - 拖回最小值，自动回到列表布局
- Slider 不做 snap、不做死区，所见即所得
- Slider 左右两端的 `<` / `>` 快捷按钮（ImGui 原生支持）可用于精调

### 5.2 面板宽度变化时的 Grid 响应

- `UI::BeginGrid` 内部每帧读 `GetContentRegionAvail().x` 算列数
- 面板被拉窄到无法容纳 1 个格子时 `columnCount` 保底为 1
- 格子内图标不缩放（图标尺寸由 `m_IconSize` 决定），只影响每行数量

### 5.3 内联重命名在 Grid 下的位置

- Grid Item 的"名字区"是单元格下半部分（图标下方，2 行高度）
- `BeginRenamableGridItem` 内部判定 `rename.IsEditing(id, scopeTag)` → 在该矩形画 `InputText` 覆盖显示文本
- `RenameController` 的 BeginEditing / IsEditing / 跨帧提交语义不动，只是**在哪里画 InputText 需要抽成和布局解耦的 helper**（见 §4.4 的"注意事项"）
- 回车 / 失焦提交，Esc 取消 —— 复用现有流程

### 5.4 空白点击 / 空白右键

- 现有逻辑：`ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()` 空白点击 → 取消选中
- Grid 下**也走同样判定** —— `InvisibleButton` 命中时 `IsAnyItemHovered` 为 true，空白处不命中，判定自然
- 右键菜单：空白处 `BeginPopupContextWindow` 弹出 "Create / Refresh" 等菜单，Grid Item 上 `BeginPopupContextItem` 弹出"Rename / Delete / Open"

### 5.5 Pending Create 占位

- 列表版的 Pending Create 占位（右键 Create → 立即显示占位 TreeNode 进入 Rename 态）在 Grid 布局下也要支持
- Grid 版实现：在 `DrawContentArea_Grid` 内调 `UI::BeginGrid` 之后、`UI::EndGrid` 之前，若命中 Pending Create 条件就多调一次 `UI::BeginRenamableGridItem` 画占位（icon 按 `PendingCreateKind` 分派，id 用 `m_PendingCreate.VirtualPath`，走和真实 Item 一致的 Rename 流程）
- 复用现有 `PendingCreateState` 和 `UI::RenameController` 流程，零改造

### 5.6 拖拽接收（目标）

- Grid Item 作为**目录**时要能接收其他资产拖入（移动到该目录）
- 实现：`ApplyAssetItemInteractions` helper 的 `BeginDragDropTarget` 分支判定 `isDirectory` → 检测 `DragDrop::AssetHandle` payload，接收后调 `AssetManager::MoveAsset` 到该目录（与现有列表版的拖拽目标语义一致）

---

## 6. 工程集成

### 6.1 文件改动清单

**新增内容**（追加到既有文件，不新建文件）：

| 文件 | 内容 |
|---|---|
| `Lucky/Source/Lucky/UI/Widgets.h` | 追加 `BeginGrid / EndGrid`、模板 `BeginRenamableGridItem / EndRenamableGridItem` 声明 |
| `Lucky/Source/Lucky/UI/Widgets.cpp` | 追加 GridContext 栈 + 上述函数的实现 + 私有 helper `Detail::DrawTruncatedCenteredText` / `Detail::DrawInlineInputAtRect` |
| `Lucky/Source/Lucky/UI/Theme.h` | `Layout` namespace 追加 `GridItemPaddingX / GridItemPaddingY / GridItemNameRows / GridItemSpacing` 四个常量 |
| `Luck3DApp/Source/Panels/ProjectAssetsPanel.h` | 新成员 `m_IconSize` + 相关常量 + 新函数声明 |
| `Luck3DApp/Source/Panels/ProjectAssetsPanel.cpp` | 实现 `DrawBottomToolbar` / `DrawContentArea_List` / `DrawContentArea_Grid` / `DrawAssetItem_Grid` / `ApplyAssetItemInteractions`；重构 `OnGUI` 右侧内容区 |

### 6.2 不涉及的层级

- 不改 Asset / Renderer / Scene 任何核心
- 不改 `BeginRenamableTreeNode` 已有签名
- 不改 `RenameController` 对外接口（**内部可能需要抽一个 `DrawInlineInputAtRect` helper**，但这是私有实现细节）

### 6.3 复用价值（后续面板）

- `UI::BeginGrid` + `UI::BeginRenamableGridItem` 一次写好，后续**资产选择面板**（弹窗式："选择一个 Material / Mesh"）、**Prefab 面板**、**Shader Graph 节点库面板** 等都可以直接复用，零重复编码。

---

## 7. 验收标准

### 7.1 G1 底部工具栏骨架

- [ ] 面板右侧内容区底部出现 ~24px 工具栏，右端有 SliderFloat
- [ ] Slider 拖动时 `m_IconSize` 在 16~128 之间连续变化
- [ ] 工具栏不侵占内容区高度；垂直滚动条位置正确
- [ ] 原列表布局功能全部正常（Slider 停在最小值时）：目录展开 / 选中 / 重命名 / 拖拽 / 右键菜单 / Pending Create

### 7.2 G2 UI 原语：BeginGrid / BeginRenamableGridItem

- [ ] `UI::BeginGrid` / `UI::EndGrid` 配对正确；`EndGrid` 后 ImGui cursor 恢复到换行后的位置
- [ ] `UI::BeginRenamableGridItem` 返回 true 后必须配对 `EndRenamableGridItem`，否则 LF_CORE_ASSERT
- [ ] 临时 demo 验证：按给定 cellSize 画 N 个格子，面板拉窄 / 拉宽时列数自动变化
- [ ] 临时 demo 的格子支持：命中选中（点击 → 回调） / Hover 高亮 / F2 启动 Rename / InputText 覆盖名字位置 / 回车提交 / Esc 取消

### 7.3 G3 Project 面板接入 Grid

- [ ] Slider 停在 16px → 显示列表布局（和改造前一致）
- [ ] Slider 拖离最小值一点 → 立即变 Grid 布局，图标尺寸随 Slider 连续变化
- [ ] Slider 拖到最大 → Grid 图标 128px
- [ ] 面板宽度变化时 Grid 列数自动调整
- [ ] Grid Item 中缩略图正确显示（Material 球体、Mesh 剪影、Texture 原图、其他类型图标）
- [ ] 名字超长时截断显示（省略号）
- [ ] Grid Item 单击选中（高亮背景）
- [ ] Grid Item 双击：目录 → 进入；资产 → Inspector 显示
- [ ] Grid Item 右键菜单：Rename / Delete / Open 等
- [ ] Grid Item 支持内联重命名（F2 或菜单触发），InputText 覆盖图标下方名字区
- [ ] Grid Item 拖拽源：拖到视口能创建实体（继承现有拖拽逻辑）
- [ ] Grid Item 作为目录时能接收拖拽（移动资产到该目录）
- [ ] 空白点击 → 取消选中
- [ ] Grid 下右键 Create Material / Scene 后立即出现占位 Grid Item 进入 Rename 态；提交落盘；Esc 取消
- [ ] Grid ↔ List 切换时，选中态 / Rename 态 / Pending Create 态都保持一致

---

## 8. 风险与遗留问题

### 8.1 Grid 首帧批量渲染卡顿

- 档位切到 Grid 时，一次性可能显示 20~50 个格子，若全部 miss 预览缓存，一帧内触发 20~50 次 `AssetPreviewRenderer::RenderXxx`
- 128×128 单张渲染耗时 << 1 ms，50 张 ≈ 50 ms —— **可能有感知卡顿但不致命**
- 根治方案：PhaseR35 的 P5 异步渲染队列（每帧预算 N 张），本阶段不做

### 8.2 名字截断的实现

- `ImGui::TextWrapped` 可做自动换行，但不支持省略号
- 需要自己写 `UI::DrawTruncatedText(drawList, bbMin, bbMax, text, maxLines)`：按字符长度二分 + `CalcTextSize` 判断是否能放下，不行就加 `...`
- 工作量约 30 行代码；若复杂可暂用 `TextWrapped` 做自动换行（不优雅但能用），后续迭代

### 8.3 Grid 的 ID 稳定性

- `BeginRenamableGridItem` 内部用 `ImGui::PushID(name)` + `InvisibleButton("##cell")` 保证 ID 跨帧稳定
- 调用方传入的 `name` 字符串：目录用 `path.filename().string()`，资产用 `std::format("{}##{}", stem, handle)` —— 和列表版 `DrawAssetItem` 的 `strID` 用同一套规则，不冲突
- `BeginGrid` 外层 `PushID(id)` 再隔离一层，和同窗口其他控件不碰撞

### 8.4 滑动条拖动中 OnEvent 的穿透

- Slider 拖动中若鼠标滚轮也滚动，可能触发视口相机 / 其他快捷键
- 现有 `EditorPanel::IsHovered() / IsFocused()` 守卫已覆盖，无额外处理

### 8.5 工具栏点击区的"空白点击取消"误判

- 原逻辑：内容区的 `IsMouseClicked(0) && IsWindowHovered() && !IsAnyItemHovered()` → 取消选中
- 拆成 `##Content` + `##BottomToolbar` 两个 BeginChild 后，Slider 所在的子窗口是独立的，不会被判为"内容区空白"
- 需验证：在工具栏区域拖动 Slider 时**不触发**内容区取消选中

### 8.6 "刚离开最小值" 的 Grid 图标非常小

- `m_IconSize = 17` 时 Grid 图标就是 17px，比列表 TreeNode 的 ~14px 大不了多少，审美上过渡不漂亮
- 这是**明确的"不要吸附"取舍**：用户想要什么尺寸就是什么尺寸，不替用户决定"视觉上多少才算合适"
- 若实际使用中确实觉得这段区间不好看，日后可以加一个 "Slider 吸附到 32" 的开关（Preference 项），不改默认行为

---

## 9. 后续扩展（本阶段不做）

| 后续阶段 | 内容 |
|---|---|
| **P5-Pref** | 把 `m_IconSize` 存到 `EditorPreferences`，重启恢复 |
| **P5-Async** | PhaseR35 配套的异步渲染预算（每帧最多渲 N 张） |
| **P5-Breadcrumb** | 底部工具栏左侧加当前目录面包屑 |
| **P5-Search** | 底部工具栏中部加搜索框 |
| **P5-Filter** | 按资产类型过滤的下拉 |
| **P5-SizeHint** | 滑动条右侧显示当前图标尺寸数字（如 "64 px"） |
| **P5-SnapToggle** | Preference 开关："是否吸附到 32 / 64 / 128" 可选 |
| **P5-AssetPickerPanel** | 复用 `UI::BeginGrid` + `UI::BeginRenamableGridItem` 实现"资产选择面板"（弹窗式）|

---

## 10. 一句话任务清单（给编码者）

1. **G1**：
   - `ProjectAssetsPanel.h` 加成员 `m_IconSize` + 常量 `s_IconSizeMin / Max / s_ListThresholdEps / s_BottomToolbarHeight`、`IsListLayout()` inline、`DrawBottomToolbar` 声明
   - `ProjectAssetsPanel.cpp` 实现 `DrawBottomToolbar`（右端对齐 `SliderFloat`，范围 16~128）
   - `OnGUI` 的右侧 `BeginChild "##ContentArea"` 内部拆成 `##Content` {0, -24} + `##BottomToolbar` {0, 24} 两段
2. **G2**：
   - `Lucky/UI/Theme.h` 加 `GridItemPaddingX / GridItemPaddingY / GridItemNameRows / GridItemSpacing`
   - `Lucky/UI/Widgets.h` 加 `BeginGrid / EndGrid` 声明 + 模板 `BeginRenamableGridItem / EndRenamableGridItem` 声明
   - `Lucky/UI/Widgets.cpp` 实现 GridContext 栈 + 上述函数 + 私有 helper（`DrawTruncatedCenteredText` / `DrawInlineInputAtRect`）
   - 临时写一个 demo 面板验证 §7.2
3. **G3**：
   - 拆 `DrawContentArea` → `DrawContentArea_List` / `DrawContentArea_Grid`
   - 实现 `DrawAssetItem_Grid`（薄壳，调 `UI::BeginRenamableGridItem`）
   - 抽 `ApplyAssetItemInteractions` helper，List 和 Grid 共用
   - Grid 下的 Pending Create 占位接通
   - 验收 §7.3 的勾选项
