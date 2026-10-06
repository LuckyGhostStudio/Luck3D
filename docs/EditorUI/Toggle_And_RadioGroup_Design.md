# Toggle 控件 & RadioGroup 分组控件 — 详细设计

> **文档类型**：详细设计文档（可直接编码实现）
> **创建日期**：2026-10-06
> **状态**：待实施
> **所属层级**：Layer 3 — 通用控件
> **关联文档**：
> - [ImGui_Layer3_CommonWidgets_And_EditorPanel.md](./ImGui_Layer3_CommonWidgets_And_EditorPanel.md)（通用控件总览）
> - [EditorIcon_Phase2_EditorIconManager.md](./EditorIcon_Phase2_EditorIconManager.md)（图标管理器）
> - [EditorPreferences_And_ThemeColor_System.md](./EditorPreferences_And_ThemeColor_System.md)（主题色系统）
> **代码规范**：[Coding_Style_Guide.md](../Coding_Style_Guide.md)

---

## 一、目标与背景

### 1.1 要解决的问题

当前仓库里"双态按钮"（Toggle）有 **3 份重复实现**：

| 位置 | 做法 | 外观 |
|---|---|---|
| `EditorToolbar.cpp:32` `DrawToggleIconButton` | 独立局部函数，只服务 Play / Pause | 纯图标 |
| `SceneViewportPanel.cpp:208-281` Grid / CSM 按钮 | **复制粘贴两遍**，每份约 50 行手写 `PushStyleColor` + `ImGui::Button` | 纯文本 |
| 没有 | 需要新增 Scene Gizmo 工具栏（Select / Translate / Rotate / Scale 四按钮互斥） | 纯图标，Radio 单选 |

同时**"按钮组"抽象完全缺失**，加 Gizmo 工具栏现在只能继续手写互斥逻辑。

### 1.2 本次要交付的

1. **Toggle 原语（展示层）**：封装"二态按钮"的 3 种外观变体（纯图标 / 纯文本 / 图标+文本）
2. **RadioGroup 组（行为层）**：封装"单选 + 必选一个"的组行为，内部复用 Toggle 原语
3. **接入**：
   - Scene 视口工具栏新增 Gizmo 四按钮组（Selection / Translate / Rotate / Scale）
   - Scene 视口的 Grid / CSM 迁移到 `ToggleTextButton`
   - EditorToolbar 的 Play / Pause 迁移到 `ToggleIconButton`

### 1.3 本次**不做**的

- **Segmented Control 分段圆角**（Unity Pivot/Center 工具栏那种"三连按钮"视觉）：API 预留 `connected` 开关，本次 `connected = false` 实现只画独立按钮。原因：ImGui 的 `FrameRounding` 是四角一起生效，要实现分段圆角必须用 `InvisibleButton + DrawList` 自绘背景，代码量翻倍、本次性价比低。等有刚需再做。
- **多选组（CheckboxGroup）**：当前多选场景就是独立 Toggle 并排（Grid + CSM），不需要"组"抽象。
- **原生 Radio / Checkbox 组（圆点、方框）**：ImGui 原生 `RadioButton` / `Checkbox` 够用，等真需要封装再做。
- **禁用态（disabled）**：Gizmo / Grid / CSM / Play / Pause 现在都不需要。API 预留参数但本次不实现样式。

---

## 二、设计原则

### 2.1 两层正交分解

把"按钮组"拆成**展示层**和**行为层**两个独立维度，禁止把二者揉在一起：

| 维度 | 候选 |
|---|---|
| **展示层**（单个按钮长什么样） | 纯图标 / 纯文本 / 图标+文本 /（未来）圆点 / 方框 |
| **行为层**（组怎么管状态） | 单 Toggle（无组）/ Radio 单选必选 /（未来）Checkbox 多选 / 单选可取消 |

每种展示可以和每种行为自由组合（Gizmo 组 = 图标 + Radio；Console 日志过滤组 = 图标+文本 + Radio）。

### 2.2 Immediate Mode，和 ImGui 对齐

- 所有 API 保持 ImGui 的即时模式风格（每帧调用）
- 单个 Toggle 返回 `bool`（本次是否被点击）
- Begin/End 配对
- **不**引入"控件对象"或"配置结构体 Builder"等 retained-mode 东西

### 2.3 状态外绑

- 单 Toggle 绑定 `bool& value`
- Radio 组绑定 `int& selectedIndex`
- 组内部**不持有**状态 —— 所有状态在调用方字段里，键盘快捷键改字段后组会自动跟随

### 2.4 ID 稳定性由基类保证

- `EditorPanel::OnImGuiRender` 已在每帧每个面板起始调 `UI::ResetIDCounter()`，面板内 `UI::GenerateID()` 返回的 ID 跨帧稳定
- 组内部 `ImGui::PushID(strID)` 建立独立 ID 子作用域，Item 之间用 `ImGui::PushID(itemIndex)` 区分

---

## 三、文件结构

```
Lucky/Source/Lucky/UI/
├── Toggles.h              // 新增：Toggle 原语 + RadioGroup
├── Toggles.cpp            // 新增：实现
├── Theme.h                // 修改：新增 Colors::Toggle 子命名空间（样式常量）
└── Widgets.h/.cpp         // 不动

Lucky/Source/Lucky/Editor/
├── EditorIconManager.h    // 修改：新增 4 个 Gizmo 工具栏图标接口
└── EditorIconManager.cpp  // 修改：Init 中加载 4 张新图标

Luck3DApp/Source/
├── EditorToolbar.cpp      // 修改：Play / Pause 迁移到 UI::ToggleIconButton
└── Panels/
    ├── SceneViewportPanel.h   // 修改：m_GizmoType 默认值改为 -1（Selection）
    └── SceneViewportPanel.cpp // 修改：工具栏重写 Gizmo 组 + Grid/CSM 迁移
```

---

## 四、第 1 层：Toggle 原语（展示层）

### 4.1 三个函数签名

```cpp
namespace Lucky::UI
{
    /// <summary>
    /// Toggle 按钮样式（可选）。为 nullptr 时使用 Theme::Colors::Toggle 的默认值。
    /// 调用方几乎不需要传，仅在需要自定义高亮色时覆盖。
    /// </summary>
    struct ToggleStyle
    {
        ImVec4 BgNormal;              // 未选中：常态背景
        ImVec4 BgHovered;             // 未选中：Hover 背景
        ImVec4 BgActive;              // 未选中：按下背景
        ImVec4 BgSelected;            // 选中：常态背景
        ImVec4 BgSelectedHovered;     // 选中：Hover 背景
        ImVec4 BgSelectedActive;      // 选中：按下背景
    };

    /// <summary>
    /// 纯图标 Toggle 按钮
    /// </summary>
    /// <param name="strID">ImGui ID（如 "##Play"），用于避免同帧内多个按钮 ID 冲突</param>
    /// <param name="icon">图标纹理（内部用 ImageButtonFlipped 绘制，自动处理 OpenGL Y 翻转）</param>
    /// <param name="value">双向绑定的选中状态；被点击时内部翻转</param>
    /// <param name="size">按钮总尺寸（图标撑满该尺寸，内边距为 0）</param>
    /// <param name="tooltip">鼠标悬停时显示的提示文本，nullptr 则不显示</param>
    /// <param name="style">样式覆盖，nullptr 使用主题默认</param>
    /// <returns>本次是否被点击（点击时 value 已被切换）</returns>
    bool ToggleIconButton(const char* strID,
                          const Ref<Texture2D>& icon,
                          bool& value,
                          const ImVec2& size,
                          const char* tooltip = nullptr,
                          const ToggleStyle* style = nullptr);

    /// <summary>
    /// 纯文本 Toggle 按钮
    /// </summary>
    /// <param name="label">按钮文本（会同时作为 ImGui 内部 ID 的可见部分；
    ///                     如需隐藏 ID 可用 "Grid##GridToggle" 这种形式）</param>
    /// <param name="value">双向绑定的选中状态</param>
    /// <param name="size">按钮尺寸；{0, 0} 时 ImGui 自动按文本尺寸测算</param>
    bool ToggleTextButton(const char* label,
                          bool& value,
                          const ImVec2& size = { 0.0f, 0.0f },
                          const char* tooltip = nullptr,
                          const ToggleStyle* style = nullptr);

    /// <summary>
    /// 图标 + 文本 Toggle 按钮（图标在左、文本在右）
    /// </summary>
    /// <param name="strID">ImGui ID</param>
    /// <param name="icon">左侧图标</param>
    /// <param name="label">右侧文本</param>
    /// <param name="value">双向绑定的选中状态</param>
    /// <param name="size">按钮尺寸；{0, 0} 自动测算</param>
    bool ToggleIconTextButton(const char* strID,
                              const Ref<Texture2D>& icon,
                              const char* label,
                              bool& value,
                              const ImVec2& size = { 0.0f, 0.0f },
                              const char* tooltip = nullptr,
                              const ToggleStyle* style = nullptr);
}
```

### 4.2 行为契约

- **点击即切换**：被点击时内部执行 `value = !value`，然后返回 `true`
- **选中态/未选中态**只影响背景色，图标 tint 始终为白（和现有 `EditorToolbar::DrawToggleIconButton` 一致）
- **Tooltip** 仅在非 nullptr 且鼠标悬停时显示（`ImGui::IsItemHovered() && ImGui::SetTooltip(tooltip)`）
- **FrameBorderSize 必须显式 Push 为 0.0f**，否则项目全局 Theme 的 `FrameBorderSize = 1.0f` 会在按钮边缘画一圈半透明 Border，和深色底色混合后视觉上"吞掉"最外 1px，使按钮比实际尺寸小一圈（见 `EditorToolbar.cpp:48-49` 注释）

### 4.3 内部实现要点

**纯图标按钮**：

```cpp
bool ToggleIconButton(const char* strID, const Ref<Texture2D>& icon, bool& value,
                      const ImVec2& size, const char* tooltip, const ToggleStyle* style)
{
    const ToggleStyle& s = style ? *style : GetDefaultToggleStyle();

    ImGui::PushID(strID);

    if (value)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, s.BgSelected);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, s.BgSelectedHovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, s.BgSelectedActive);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, s.BgNormal);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, s.BgHovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, s.BgActive);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

    bool clicked = UI::ImageButtonFlipped(icon, size, 0);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if (tooltip && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::PopID();

    if (clicked)
    {
        value = !value;
    }
    return clicked;
}
```

**纯文本按钮**：差异仅在于：
- 不需要 `ImageButtonFlipped`，用 `ImGui::Button(label, size)`
- 不需要显式关闭 `FrameBorderSize`（文本按钮不吃这个坑）
- `strID` 由 `label` 本身承担（ImGui 用 label 做 ID），所以不需要独立 `PushID`

**图标+文本按钮**：ImGui 没有原生"图标+文本"按钮控件，可选以下实现路径：

#### 方案 A（推荐）：InvisibleButton + 手动布局

```cpp
// 伪代码
ImGui::PushID(strID);
ImVec2 cursorStart = ImGui::GetCursorScreenPos();
ImVec2 realSize = (size.x > 0 && size.y > 0) ? size : ComputeIconTextSize(icon, label);
bool clicked = ImGui::InvisibleButton("##btn", realSize);

// 画背景矩形（根据 value 和 Hover/Active 状态选色）
ImVec4 bgColor = PickToggleBgColor(value, ImGui::IsItemHovered(), ImGui::IsItemActive(), s);
ImGui::GetWindowDrawList()->AddRectFilled(cursorStart, cursorStart + realSize,
                                          ImGui::ColorConvertFloat4ToU32(bgColor),
                                          Theme::Layout::FrameRounding);
// 画图标（左）+ 文本（右），用 AddImage + AddText
...
```

- **优点**：完全可控，图标和文本的间距、对齐都能自己定；分段圆角将来升级也走这条路
- **缺点**：代码量比纯 `ImGui::Button` 多 20 行；要处理 Hover / Active 判定；不能复用 ImGui 的 FramePadding 主题设置

#### 方案 B：ImageButtonFlipped + SameLine + Text

- **优点**：实现简单，10 行搞定
- **缺点**：是**两个独立控件**而非一个按钮；点击图标和点击文本是两次独立事件；Tooltip 只能挂在图标或文本上一个

#### 方案 C（**不推荐**）：ImageButton 塞一个"图标 + 文字拼合位图"

- 需要文字渲染器预烘培纹理，远超本次范围

**结论**：采用 **方案 A（InvisibleButton + 手动布局）**。它是正确实现图标+文本按钮的唯一通用方式，而且和分段圆角的未来扩展路径一致。

### 4.4 和现有 `EditorToolbar::DrawToggleIconButton` 的关系

- `DrawToggleIconButton` 是本次新接口的**局部先行版**，功能完全被 `UI::ToggleIconButton` 覆盖
- 本次接入时**删除** `EditorToolbar::DrawToggleIconButton`，Play / Pause 的调用点改为 `UI::ToggleIconButton`
- `EditorToolbar.cpp` 匿名命名空间里的 6 个颜色常量（`kBtnBg*`）**迁移到** `Theme::Colors::Toggle`，不再在 EditorToolbar 内重复定义

---

## 五、Theme 扩展

在 `Lucky/Source/Lucky/UI/Theme.h` 新增 `Colors::Toggle` 子命名空间（如果 Theme.h 已有 `Colors` 命名空间就并入；如果没有，新建一个）：

```cpp
namespace Lucky::UI::Theme::Colors::Toggle
{
    // ---- 未选中态 ----
    constexpr ImVec4 BgNormal        = ImVec4(0.220f, 0.220f, 0.220f, 1.00f);   // #383838
    constexpr ImVec4 BgHovered       = ImVec4(0.280f, 0.280f, 0.280f, 1.00f);   // #474747
    constexpr ImVec4 BgActive        = ImVec4(0.280f, 0.280f, 0.280f, 1.00f);   // 同 Hovered

    // ---- 选中态 ----
    constexpr ImVec4 BgSelected        = ImVec4(0.275f, 0.377f, 0.486f, 1.00f); // #46607C 蓝
    constexpr ImVec4 BgSelectedHovered = ImVec4(0.200f, 0.302f, 0.502f, 1.00f); // 稍亮
    constexpr ImVec4 BgSelectedActive  = ImVec4(0.200f, 0.302f, 0.502f, 1.00f); // 同 Hovered
}
```

**配色来源**：
- 未选中态颜色复用 `EditorToolbar.cpp:17-19` 的 `kBtnBg*Normal`
- 选中态蓝色复用 `SceneViewportPanel.cpp:234` Grid 按钮的 `{0.275f, 0.377f, 0.486f, 1.0f}`
- `EditorToolbar.cpp:21-23` 的 `kBtnBg*Highlight` 和 Grid 的蓝色略有差异（0.200/0.302/0.452 vs 0.275/0.377/0.486），本次**统一到 Grid 的更亮版本**，视觉更接近 Unity Selected 态

`Toggles.cpp` 内部 `GetDefaultToggleStyle()` 组装这些常量：

```cpp
static const ToggleStyle& GetDefaultToggleStyle()
{
    using namespace Lucky::UI::Theme::Colors::Toggle;
    static const ToggleStyle s_Default = {
        BgNormal, BgHovered, BgActive,
        BgSelected, BgSelectedHovered, BgSelectedActive,
    };
    return s_Default;
}
```

---

## 六、第 2 层：RadioGroup（行为层）

### 6.1 两种调用风格

**风格 A — Begin/End 迭代风格**（主要实现）：

```cpp
UI::BeginRadioGroup("##GizmoOps", m_GizmoType);
{
    UI::RadioIconItem(-1, iconSelection,    buttonSize, "Selection (Q)");
    UI::RadioIconItem( 0, iconTranslation,  buttonSize, "Translate (G)");
    UI::RadioIconItem( 1, iconRotation,     buttonSize, "Rotate (R)");
    UI::RadioIconItem( 2, iconScale,        buttonSize, "Scale (S)");
}
UI::EndRadioGroup();
```

**风格 B — 数据驱动一次性调用**（可选便捷接口，内部调用风格 A）：

```cpp
struct RadioIconItemDesc
{
    int Value;                       // 对应 selectedIndex 的值
    const Ref<Texture2D>* Icon;      // 图标（指针防止拷贝 Ref 的开销）
    const char* Tooltip;
};

UI::RadioIconGroup(const char* strID, int& selectedIndex,
                   const RadioIconItemDesc* items, int count,
                   const ImVec2& itemSize);
```

### 6.2 API 签名

```cpp
namespace Lucky::UI
{
    /// <summary>
    /// RadioGroup 样式（可选）
    /// </summary>
    struct RadioGroupStyle
    {
        float ItemSpacing = 2.0f;    // Item 之间水平间距（像素）
        bool Connected = false;      // 分段圆角：true 时所有 Item 无间距、相邻圆角相消
                                     // （本次 Connected = true 分支不实现，按 false 走）
        ToggleStyle Items;           // 单个 Item 的样式（直接复用 Toggle 的样式）
    };

    /// <summary>
    /// 开始一个 Radio 组（互斥、必选一个）。
    /// 配对的 EndRadioGroup() 必须调用。
    /// 期间所有 RadioXxxItem 调用的 ItemValue 若等于 selectedIndex 则绘制为"选中"状态；
    /// 点击某个 Item 时 selectedIndex = 该 Item 的 ItemValue。
    /// 点击已选中项不做任何事（保证"至少选一个"约束）。
    /// </summary>
    /// <param name="strID">ImGui ID 子作用域</param>
    /// <param name="selectedIndex">双向绑定的当前选中值</param>
    /// <param name="style">样式覆盖，nullptr 使用默认（ItemSpacing=2, Connected=false）</param>
    void BeginRadioGroup(const char* strID, int& selectedIndex,
                         const RadioGroupStyle* style = nullptr);

    /// <summary>
    /// 向当前 Radio 组追加一个"纯图标"Item
    /// </summary>
    /// <param name="itemValue">该 Item 代表的值（选中时写入 selectedIndex）</param>
    /// <param name="icon">图标</param>
    /// <param name="size">按钮尺寸</param>
    /// <param name="tooltip">悬停提示</param>
    void RadioIconItem(int itemValue, const Ref<Texture2D>& icon,
                       const ImVec2& size, const char* tooltip = nullptr);

    /// <summary>
    /// 向当前 Radio 组追加一个"纯文本"Item
    /// </summary>
    void RadioTextItem(int itemValue, const char* label,
                       const ImVec2& size = { 0.0f, 0.0f },
                       const char* tooltip = nullptr);

    /// <summary>
    /// 向当前 Radio 组追加一个"图标+文本"Item
    /// </summary>
    void RadioIconTextItem(int itemValue, const Ref<Texture2D>& icon, const char* label,
                           const ImVec2& size = { 0.0f, 0.0f },
                           const char* tooltip = nullptr);

    /// <summary>
    /// 结束 Radio 组
    /// </summary>
    void EndRadioGroup();

    // ---- 便捷接口（可选，内部走 Begin/End）----
    void RadioIconGroup(const char* strID, int& selectedIndex,
                        const RadioIconItemDesc* items, int count,
                        const ImVec2& itemSize,
                        const RadioGroupStyle* style = nullptr);
}
```

### 6.3 内部状态管理

Radio 组需要跨 `BeginRadioGroup / RadioXxxItem / EndRadioGroup` 共享状态（当前选中值的指针、样式、第一个 Item 是否已画 —— 用于决定要不要 `SameLine`、item 之间 spacing）。

#### 方案 A（推荐）：static 栈

```cpp
// Toggles.cpp 内部
namespace
{
    struct RadioGroupContext
    {
        int* SelectedIndex = nullptr;       // 绑定的外部变量
        RadioGroupStyle Style;              // 本组样式
        int ItemCount = 0;                  // 已画 Item 数（用于判断首个 vs 后续）
    };

    static std::vector<RadioGroupContext> s_RadioGroupStack;
}
```

- `BeginRadioGroup` 把新 context 推入栈顶
- `RadioXxxItem` 读 `s_RadioGroupStack.back()`，判断 `ItemCount == 0` 决定是否 `SameLine`；画完 `++ItemCount`
- `EndRadioGroup` 弹出栈顶

**优点**：
- 和 `UI::PushID / PopID` 的 counter 栈管理一致
- 支持嵌套（虽然 Radio 组嵌套场景罕见）
- 实现简单

**缺点**：
- 全局 static 需要注意线程安全（本项目 UI 单线程，无影响）
- 调用方忘记 `EndRadioGroup` 会导致栈残留 —— 加 Debug 期 Assert 兜底

#### 方案 B：ImGui Storage

把 context 挂到 `ImGui::GetStateStorage()`，用 strID hash 做 key。

- **优点**：和 ImGui 的 retained-mode 存储风格一致，不需要全局变量
- **缺点**：需要 hash key、取出时类型转换麻烦；对嵌套场景支持更弱

**结论**：采用**方案 A**（static 栈）。和项目其他 UI 工具（UICore 的 PushID / PopID 栈）保持风格一致。

### 6.4 Item 内部实现

```cpp
void RadioIconItem(int itemValue, const Ref<Texture2D>& icon, const ImVec2& size, const char* tooltip)
{
    LF_CORE_ASSERT(!s_RadioGroupStack.empty(), "RadioIconItem called outside BeginRadioGroup");

    RadioGroupContext& ctx = s_RadioGroupStack.back();

    // Item 间排布：首个 Item 不 SameLine，后续 Item 都 SameLine
    if (ctx.ItemCount > 0)
    {
        ImGui::SameLine(0.0f, ctx.Style.ItemSpacing);
    }

    // 判断当前 Item 是否被选中
    bool isSelected = (*ctx.SelectedIndex == itemValue);

    // 调用 Toggle 原语绘制。注意 value 用一个 **临时** bool，
    // 因为我们不希望 Toggle 翻转 selectedIndex 的其他值 ——
    // Toggle 的 "clicked 时 value = !value" 对 Radio 场景无意义，我们接管 clicked。
    bool displayValue = isSelected;
    bool clicked = ToggleIconButton(/*strID=*/ GenerateItemID(itemValue), icon, displayValue,
                                    size, tooltip, &ctx.Style.Items);

    if (clicked && !isSelected)
    {
        *ctx.SelectedIndex = itemValue;     // 切换选中值
    }
    // clicked && isSelected 时：Toggle 会把 displayValue 翻成 false，但我们不管，
    // 下一帧 displayValue 又从 isSelected 重新算 —— 结果保持选中。
    // 这就是"至少选一个"的实现：点中已选中项不改外部 selectedIndex。

    ++ctx.ItemCount;
}
```

**关键点**：
- 第一个 Item 不 `SameLine`，让组的位置由调用方（父窗口的 cursor）决定
- 用临时 `displayValue` 隔离 Toggle 的内部切换行为，自己接管"写回 selectedIndex"
- 点中已选中项时 `clicked && !isSelected = false`，什么都不做 —— 自动满足必选约束
- `GenerateItemID(itemValue)` 用 itemValue 做 ID 字符串（如 `"##item_-1"` / `"##item_0"`），保证组内 Item 之间 ID 不冲突

### 6.5 "必选"约束的两种实现方式

#### 方案 A（推荐，上文已用）：点中已选中项时什么都不做

- **优点**：实现最简单，一行判断；调用方不用关心
- **缺点**：用户点击时视觉上没反馈（但这就是 Unity Gizmo 工具栏的行为）

#### 方案 B：允许点击，但 EndRadioGroup 内检测"selectedIndex 不在合法范围" → 强制 fallback 到第一个 Item

- **优点**：允许更灵活的交互（可以在外部通过别的方式改成 "无选中"，组内会 fallback）
- **缺点**：需要维护"合法 itemValue 列表"，复杂度高

**结论**：采用**方案 A**。这是 Unity Gizmo 工具栏的标准行为。

---

## 七、EditorIconManager 扩展

需要新增 4 个 Gizmo 工具栏图标接口。线线已经把图标文件放到 `Luck3DApp/Resources/Icons/Toolbar/` 下，命名约定（**务必核对实际文件名**）：

- `Toolbar/Selection.png`
- `Toolbar/Translation.png`
- `Toolbar/Rotation.png`
- `Toolbar/Scale.png`

### 7.1 EditorIconManager.h 新增接口

```cpp
// ======== 工具条图标 ========（在 GetPauseIcon 之后追加）
static const Ref<Texture2D>& GetSelectionIcon();
static const Ref<Texture2D>& GetTranslationIcon();
static const Ref<Texture2D>& GetRotationIcon();
static const Ref<Texture2D>& GetScaleIcon();
```

### 7.2 EditorIconManager.cpp

- `EditorIconData` struct 内部新增 4 个 `Ref<Texture2D>` 字段
- `Init()` 中加载：

```cpp
s_IconData.SelectionIcon    = LoadIcon("Toolbar/Selection.png");
s_IconData.TranslationIcon  = LoadIcon("Toolbar/Translation.png");
s_IconData.RotationIcon     = LoadIcon("Toolbar/Rotation.png");
s_IconData.ScaleIcon        = LoadIcon("Toolbar/Scale.png");
```

- 4 个 `Get*Icon()` 实现：直接 `return s_IconData.XxxIcon`，和 `GetPlayIcon` 等已有实现对齐

---

## 八、Scene 视口 Gizmo 工具栏接入

### 8.1 SceneViewportPanel.h 改动

- `m_GizmoType` 保留 `int` 类型（线线决定不抽 enum）
- **默认值**：无需改动 —— `int m_GizmoType = -1;` 本来就是，`-1` 现在的语义是"Selection 态"

### 8.2 SceneViewportPanel.cpp 工具栏重写

**位置**：替换 `SceneViewportPanel.cpp:208-281` 的手写 Grid / CSM 按钮代码（含它们的大量 `ScopedColor`）。

**新的工具栏结构**（伪代码）：

```cpp
void SceneViewportPanel::OnGUI()
{
    constexpr float toolBarHeight = 34.0f;
    constexpr float buttonSize = 28.0f;
    const ImVec2 iconBtnSize(buttonSize, buttonSize);

    {
        UI::ScopedColor bgColor(ImGuiCol_ChildBg, { 0.235f, 0.235f, 0.235f, 1.0f });
        ImGui::BeginChild("ToolBar", { 0, toolBarHeight }, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            UI::ShiftCursor(4.0f, 4.0f);

            // ---- Gizmo Mode 下拉框（Local / World） ----
            ImGui::SetNextItemWidth(90.0f);
            const char* gizmoModes[] = { "Local", "World" };
            UI::DropdownList(m_GizmoMode, gizmoModes, IM_ARRAYSIZE(gizmoModes));

            ImGui::SameLine();
            UI::ShiftCursorX(8.0f);

            // ---- Gizmo 操作组（Selection / Translate / Rotate / Scale） ----
            UI::BeginRadioGroup("##GizmoOps", m_GizmoType);
            {
                UI::RadioIconItem(-1, EditorIconManager::GetSelectionIcon(),
                                  iconBtnSize, "Selection");
                UI::RadioIconItem(ImGuizmo::OPERATION::TRANSLATE,
                                  EditorIconManager::GetTranslationIcon(),
                                  iconBtnSize, "Translate (G)");
                UI::RadioIconItem(ImGuizmo::OPERATION::ROTATE,
                                  EditorIconManager::GetRotationIcon(),
                                  iconBtnSize, "Rotate (R)");
                UI::RadioIconItem(ImGuizmo::OPERATION::SCALE,
                                  EditorIconManager::GetScaleIcon(),
                                  iconBtnSize, "Scale (S)");
            }
            UI::EndRadioGroup();

            ImGui::SameLine();
            UI::ShiftCursorX(8.0f);

            // ---- Grid 开关 ----
            UI::ToggleTextButton("Grid", m_ShowGrid, { 0.0f, buttonSize }, nullptr);

            ImGui::SameLine();
            UI::ShiftCursorX(2.0f);

            // ---- CSM 调试开关 ----
            auto debugPass = m_SceneRenderer->GetPipeline().GetPass<DebugVisualizePass>();
            bool csmChecked = debugPass && debugPass->GetMode() == DebugVisualizeMode::CSMCascades;
            bool newCsmChecked = csmChecked;
            if (UI::ToggleTextButton("CSM", newCsmChecked, { 0.0f, buttonSize },
                                      "Toggle CSM Cascade Visualization\n"
                                      "Red=C0  Green=C1  Blue=C2  Yellow=C3"))
            {
                if (debugPass)
                {
                    debugPass->SetMode(newCsmChecked ? DebugVisualizeMode::CSMCascades
                                                     : DebugVisualizeMode::None);
                }
            }
        }
        ImGui::EndChild();
    }

    // ... Viewport 子窗口不变 ...
}
```

**要点**：
- `ImGuizmo::OPERATION::TRANSLATE` 等已有枚举值分别是 0 / 1 / 2，和 Radio 组绑定的 `int m_GizmoType` 一致，直接用枚举常量即可（可读性比裸写 0/1/2 好）
- 4 个 Gizmo 按钮之间无显式 `SameLine`，由 Radio 组内部自动布局
- Grid / CSM 不属于 Gizmo 组，仍是独立 ToggleTextButton，可同时开启
- Grid 和 CSM 之间保留原来的 2px spacing

### 8.3 快捷键

按线线决定：**保留 G/R/S，Selection 用 W**。

`SceneViewportPanel::OnKeyPressed` 内增补 `case Key::W` 分支：

```cpp
switch (e.GetKeyCode())
{
    case Key::W:
        m_GizmoType = -1;                                   // Selection（不画 Gizmo）
        break;
    case Key::G:
        m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;       // 平移
        break;
    case Key::R:
        m_GizmoType = ImGuizmo::OPERATION::ROTATE;          // 旋转
        break;
    case Key::S:
        m_GizmoType = ImGuizmo::OPERATION::SCALE;           // 缩放
        break;
}
```

**要点**：快捷键只写 `m_GizmoType` 这个字段，Radio 组绑的是同一字段，下一帧自动同步 UI。**完全无需** Radio 组暴露 "SetSelected" API。

### 8.4 `UI_DrawGizmos` 中 `-1` 分支的处理

```cpp
// 现有代码（UI_DrawGizmos 中）：
if (selectionID != 0 && m_GizmoType != -1) { ... ImGuizmo::Manipulate(...); ... }
```

- `-1` 对应 Selection 态 → 不画 Manipulate 手柄
- 这段代码**不用改**，`-1` 的现有语义正好对应"Selection 不显示 Gizmo"

---

## 九、EditorToolbar 迁移

### 9.1 删掉的东西

- `EditorToolbar.cpp` 匿名命名空间里的 `kBtnBg*` 6 个颜色常量（迁移到 Theme）
- 匿名命名空间里的 `DrawToggleIconButton` 局部函数

### 9.2 Play / Pause 调用点

**改动前**（`EditorToolbar.cpp:109-135`）：

```cpp
ImGui::PushID("##Play");
if (DrawToggleIconButton(EditorIconManager::GetPlayIcon(), isPlaying, playSize))
{
    ...
}
ImGui::PopID();
```

**改动后**：

```cpp
if (UI::ToggleIconButton("##Play", EditorIconManager::GetPlayIcon(),
                          isPlaying, playSize, /*tooltip=*/ "Play / Stop"))
{
    // 注意：UI::ToggleIconButton 已经把 isPlaying 翻转了，但我们这里 isPlaying
    // 是从 SceneManager 读的本地副本，翻转后的值不会回写。要么：
    //   (a) 用一个本地临时 bool，然后根据 clicked 调用 SceneManager::OnScenePlay/Stop
    //   (b) 不理会 isPlaying 的翻转，仍根据原来的 isPlaying 值分发
}
```

**注意**：Play / Pause 的 `isPlaying` **不是**一个真的"状态字段" —— 它是从 `SceneManager::GetActiveScene()->GetState()` 推导的。Toggle 翻转这个局部副本没有意义。

#### 方案 A（推荐）：传入临时 bool，忽略翻转

```cpp
bool playingLocal = isPlaying;
if (UI::ToggleIconButton("##Play", EditorIconManager::GetPlayIcon(),
                          playingLocal, playSize, "Play / Stop"))
{
    if (!isPlaying) { SceneManager::OnScenePlay(); ... }
    else            { SceneManager::OnSceneStop(); ... }
}
```

- **优点**：Toggle 翻转本地副本，不影响 SceneManager；判断逻辑用原始 `isPlaying` 清晰
- **缺点**：无

#### 方案 B：Toggle 加一个 `bool readOnly` 参数

- **优点**：显式表达"状态由外部管理"
- **缺点**：API 复杂度上升，而且 ImGui 本身没有这种控件约定

**结论**：采用**方案 A**。Pause 按钮同理。

---

## 十、测试与验收标准

### 10.1 Toggle 原语自测

- [ ] 鼠标 Hover 时按钮背景色切换（根据 value 选 Normal/Hovered 或 Selected/Hovered）
- [ ] 鼠标按下时背景色切换到 Active / SelectedActive
- [ ] 点击时 `value` 翻转，返回值为 true
- [ ] 未点击时返回值为 false，`value` 不变
- [ ] 传入 `tooltip != nullptr` 时 Hover 2 秒显示 tooltip
- [ ] 多个 Toggle 并排时 ID 不冲突（因为 strID 唯一 + 面板基类已 ResetIDCounter）

### 10.2 RadioGroup 自测

- [ ] 组内只有一个 Item 处于"选中"视觉状态
- [ ] 点击未选中 Item → 该 Item 变选中、原选中 Item 变未选中
- [ ] 点击已选中 Item → 组状态不变
- [ ] 通过代码直接修改 `selectedIndex`（模拟键盘快捷键）→ 下一帧 UI 跟随
- [ ] Item 之间水平排布，间距 = `style.ItemSpacing`（默认 2px）
- [ ] 第一个 Item 不 `SameLine`，位置由父窗口 cursor 决定

### 10.3 Scene 视口 Gizmo 工具栏

- [ ] 四个按钮依次显示 Selection / Translation / Rotation / Scale 图标
- [ ] 初始状态下 Selection 按钮高亮（因为 `m_GizmoType = -1`）
- [ ] 点 Translate 按钮 → m_GizmoType 变 0，视口显示 Translate 手柄
- [ ] 点 Rotate 按钮 → m_GizmoType 变 1，视口显示 Rotate 手柄
- [ ] 点 Scale 按钮 → m_GizmoType 变 2，视口显示 Scale 手柄
- [ ] 点 Selection 按钮 → m_GizmoType 变 -1，视口不显示任何手柄
- [ ] 键盘 W → Selection；G → Translate；R → Rotate；S → Scale（Scene 视口聚焦时）
- [ ] 悬停按钮显示 Tooltip
- [ ] Grid / CSM 按钮功能和之前一致，视觉和 Gizmo 按钮保持一致风格

### 10.4 EditorToolbar

- [ ] Play / Pause 按钮功能和之前一致（点击 Play 进入运行态、再点退出）
- [ ] 视觉上和之前一致（可能颜色略有变化，因为 Theme 色统一到 Grid 的更亮版本）
- [ ] 不再有 `DrawToggleIconButton` 局部函数

---

## 十一、工作量估算

| 文件 | 行数估计 | 说明 |
|---|---|---|
| `Lucky/Source/Lucky/UI/Toggles.h` | 90 | 3 个 Toggle + Begin/End + 3 个 Item + 1 个便捷接口 + 2 个 style 结构体 |
| `Lucky/Source/Lucky/UI/Toggles.cpp` | 200 | 实现，含 static 栈、默认 style 工厂、Push/Pop 样式 |
| `Lucky/Source/Lucky/UI/Theme.h` | +20 | 新增 `Colors::Toggle` 子命名空间 |
| `Lucky/Source/Lucky/Editor/EditorIconManager.h` | +10 | 4 个 `Get*Icon()` 声明 |
| `Lucky/Source/Lucky/Editor/EditorIconManager.cpp` | +15 | 4 个字段、4 个 LoadIcon、4 个 Get 实现 |
| `Luck3DApp/Source/EditorToolbar.cpp` | -40 / +15 | 删除 Draw 函数和颜色常量；Play/Pause 调用点改写 |
| `Luck3DApp/Source/Panels/SceneViewportPanel.cpp` | -50 / +40 | 删除 Grid/CSM 手写代码；新增 Gizmo 组、Grid/CSM 新接口；OnKeyPressed 加 W |

**合计新增/修改约 300 行，删除约 90 行**。

---

## 十二、实施顺序建议

按下面顺序，每一步完成即可独立验证，不会卡住：

1. **Theme.h**：新增 `Colors::Toggle`（不影响任何现有代码）
2. **EditorIconManager**：加载 4 张图标 + 4 个 Get 接口（可被 Scene 工具栏之外的代码立即使用）
3. **Toggles.h / Toggles.cpp**：新接口实现（单独编译通过，无调用方也不出错）
4. **EditorToolbar 迁移**：Play / Pause 用新接口（验证 ToggleIconButton 走通，Theme 颜色正确）
5. **Scene 视口 Grid / CSM 迁移**：验证 ToggleTextButton 走通
6. **Scene 视口新增 Gizmo 工具栏 + OnKeyPressed 加 W**：验证 RadioGroup 走通，完成本次全部功能

---

## 十三、风险与注意事项

### 13.1 已知风险

| 风险 | 应对 |
|---|---|
| ToggleIconTextButton 用 InvisibleButton + 自绘，Hover / Active 判定要和 Theme 的 FramePadding 对齐 | 内部计算尺寸时参考 `AssetField` 的做法（Widgets.cpp 内已有 InvisibleButton + DrawList 的参考实现） |
| static 栈在面板切换 / Scene 重载时若未成对 Begin/End 会脏数据 | Debug 期 `LF_CORE_ASSERT` 兜底；每次 `EndRadioGroup` 后清理 context |
| 新增图标文件名大小写不一致导致 Linux/Mac 加载失败 | 按 `EditorIconManager::LoadIcon` 已有做法，日志会 WARN，容易发现 |
| `isPlaying` 等外部管理的状态被 Toggle 翻转干扰 | 用本地临时 bool（见 §9.2 方案 A） |

### 13.2 Code 规范提醒

严格按 `Coding_Style_Guide.md` 执行：

- 文件头 `#pragma once`
- 命名空间 / 类 / 结构体 / 函数 PascalCase；私有成员 `m_` 前缀；static `s_` 前缀；局部变量 camelCase
- 4 空格缩进，Allman 大括号，`if / for / while` 强制 `{}`
- 公有接口 `/// <summary>` XML 文档注释；内部用 `//`
- 构造函数用成员初始化列表；默认构造 `= default`；重写方法加 `override`
- 智能指针用 `Ref<T>` / `Scope<T>`，禁用裸 `std::shared_ptr` / `unique_ptr`
- 禁止在源码注释里带日期戳或"踩坑"字样 —— 只讲"这段代码做什么、为什么"（历史信息沉淀到 git log / .workbuddy/memory）

---

## 十四、本设计未覆盖、留待未来

- **Segmented Control（分段圆角）**：API 预留 `Connected = true` 分支，本次按 `false` 走。实现路径：ToggleIconButton 内部支持传 "画哪几个角圆角" 的 flag（左两角 / 四角 / 右两角 / 零角），RadioGroup 根据 Item 位置分发
- **禁用态（disabled）**：Toggle 加 `bool disabled = false` 参数；选中/未选中背景色通过 `Theme::Layout::DisabledAlpha` 降透明
- **带数字徽章的 ToggleIconText**（Unity Console Error/Warn/Info 的数量角标）：ToggleIconTextButton 的扩展，不动核心 API
- **原生 Radio（圆点）/ Checkbox 组**：新增 `RadioButtonGroup(int& selected, const char* const* labels, int count)` 封装，内部走 `ImGui::RadioButton` + Group 行为
- **多选组（CheckboxGroup）**：绑 `uint32_t& bitset`，Item 内切换对应位
