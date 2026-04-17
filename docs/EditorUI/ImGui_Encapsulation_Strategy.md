# ImGui 封装策略设计文档

> **文档类型**：设计规划文档  
> **创建日期**：2025-07  
> **状态**：规划中  
> **关联文档**：[Roadmap_Feature_Development.md](../Roadmap_Feature_Development.md)、[Architecture_Overview.md](../Architecture_Overview.md)

---

## 一、概述

### 1.1 背景

当前项目的编辑器 UI 层直接使用 ImGui 原生接口（`ImGui::` 命名空间），没有任何中间封装层。随着编辑器功能的增长，这种方式带来了以下问题：

- **重复代码**：Inspector 面板中大量 `Label + 控件` 的布局代码高度重复
- **风格不一致**：各面板的控件样式、间距、颜色缺乏统一管理
- **维护困难**：修改全局风格需要逐个文件修改
- **扩展受限**：未来接入脚本系统时，无法将 UI 接口暴露给脚本层

### 1.2 目标

建立一套 **选择性封装** 的 ImGui 中间层（`Lucky::UI` 命名空间），实现：

1. 减少编辑器面板中的重复代码
2. 统一视觉风格，支持主题切换
3. 隔离引擎类型与 ImGui 类型的转换
4. 为未来的脚本绑定和编辑器扩展预留接口

### 1.3 设计原则

- **选择性封装，而非全量封装**：只封装高频、复合、涉及引擎类型转换的控件
- **不追求消灭 `ImGui::`**：查询类 API、底层布局 API 允许直接使用
- **三层架构**：基础设施 → 增强控件 → 语义化控件
- **渐进式迁移**：新代码使用封装层，旧代码按需迁移

---

## 二、当前项目 ImGui 使用现状

### 2.1 使用范围

当前项目中 **所有 ImGui 原生调用都集中在编辑器层**，引擎核心层（渲染、ECS、序列化等）没有任何 ImGui 依赖，这是一个非常好的架构基础。

| 文件 | ImGui 调用次数 | 使用的控件类型 |
|------|:---:|------|
| `InspectorPanel.cpp` | ~50 | `DragFloat/2/3/4`, `DragInt`, `ColorEdit3`, `InputText`, `InputInt`, `TreeNodeEx`, `TreePop`, `BeginCombo`, `Selectable`, `ImageButton`, `Text`, `PushID/PopID`, `SameLine`, `SetCursorPos/Y`, `PushStyleVar/PopStyleVar` |
| `SceneHierarchyPanel.cpp` | ~25 | `TreeNodeEx`, `TreePop`, `IsMouseClicked`, `IsWindowHovered`, `BeginPopupContextWindow`, `BeginMenu`, `MenuItem`, `Separator`, `IsItemClicked`, `BeginPopupContextItem` |
| `SceneViewportPanel.cpp` | ~10 | `PushStyleVar/PopStyleVar`, `GetWindowContentRegionMin/Max`, `GetWindowPos`, `GetContentRegionAvail`, `Image` |
| `EditorLayer.cpp` | ~15 | `BeginMainMenuBar`, `BeginMenu`, `MenuItem`, `EndMenu`, `EndMainMenuBar` |
| `EditorDockSpace.cpp` | ~15 | `GetMainViewport`, `SetNextWindowPos/Size/Viewport`, `PushStyleVar/Color`, `PopStyleVar/Color`, `Begin/End`, `DockSpace`, `GetIO` |
| `EditorPanel.cpp` | 5 | `Begin/End`, `BeginPopupContextItem`, `MenuItem`, `EndPopup` |
| `ImGuiLayer.cpp` | 11 | `CreateContext/DestroyContext`, `GetIO`, `NewFrame`, `Render`, `GetDrawData`, `UpdatePlatformWindows`, `RenderPlatformWindowsDefault`, `GetStyle` |

### 2.2 高频使用模式

当前 InspectorPanel 中最常见的代码模式是 **"Label + 控件"**，每个属性都需要重复以下逻辑：

```cpp
// 当前的重复模式（每个属性都要写一遍）
ImGui::DragFloat("Intensity", &light.Intensity, 0.01f, 0.0f, 10.0f);
ImGui::ColorEdit3("Color", glm::value_ptr(light.Color));
ImGui::DragFloat3("Position", &transform.Translation.x, 0.01f);
```

材质编辑器中更复杂，涉及 ID 管理、纹理类型转换、文件对话框等：

```cpp
// 当前的纹理属性代码（约 20 行重复逻辑）
Ref<Texture2D> texture = std::get<Ref<Texture2D>>(prop.Value);
uint32_t textureID = texture->GetRendererID();
std::string strID = std::format("##{0}{1}", prop.Name, textureID);
ImGui::PushID(strID.c_str());
if (ImGui::ImageButton((ImTextureID)textureID, { 64, 64 }, { 0, 1 }, { 1, 0 }))
{
    std::string filepath = FileDialogs::OpenFile("Texture(*.png;*.jpg)\0*.png;*.jpg\0");
    if (!filepath.empty())
    {
        material->SetTexture(prop.Name, Texture2D::Create(filepath));
    }
}
ImGui::SameLine();
ImGui::Text(prop.Name.c_str());
ImGui::PopID();
```

---

## 三、封装架构设计

### 3.1 三层架构

```
┌─────────────────────────────────────────────────────────────────┐
│  Layer 3: 语义化控件（面向编辑器面板）                              │
│  PropertyFloat / PropertyColor / PropertyTexture / PropertyCombo │
│  ComponentHeader / MaterialEditor                                │
│  → 内置 Label+控件布局，参数精简，面板代码直接调用                    │
├─────────────────────────────────────────────────────────────────┤
│  Layer 2: 增强控件（1:1 映射 ImGui，加视觉增强）                    │
│  UI::DragFloat / UI::ColorEdit3 / UI::ImageButton               │
│  → 参数与 ImGui 原生完全一致，内部加 ActivityOutline 等视觉效果      │
│  → 保证任何 ImGui 能做的事这层都能做                                │
├─────────────────────────────────────────────────────────────────┤
│  Layer 1: 基础设施（所有层共用）                                    │
│  ScopedStyle / ScopedColor / ScopedID / ScopedFont              │
│  Theme::Colors / Theme::Styles / Theme::Fonts                   │
│  GenerateID() / PushID() / PopID()                              │
│  BeginPropertyGrid() / EndPropertyGrid()                        │
└─────────────────────────────────────────────────────────────────┘
```

**各层职责：**

- **Layer 1（基础设施）**：RAII 工具类 + 主题颜色/样式常量 + ID 管理 + 布局辅助。所有编辑器代码都使用这一层。
- **Layer 2（增强控件）**：和 ImGui 原生 **完全相同的参数签名**，只是在内部加了统一的视觉增强（如焦点高亮、悬停效果）。这层 **不砍参数**，保证任何 ImGui 能做的事这层都能做。当 Layer 3 无法满足特殊需求时，降级到这层。
- **Layer 3（语义化控件）**：面向 Inspector 的高级控件，**大幅精简参数**，内置 Label+控件两列布局。编辑器面板 90% 的代码使用这层。

**这样设计的好处：**

- 编辑器面板 90% 的代码用 Layer 3，代码极简
- 遇到特殊需求时，降级到 Layer 2，不需要回退到 `ImGui::` 原生
- Layer 2 的参数和 ImGui 完全一致，不存在"暴露多少参数"的纠结
- 基础设施层被所有代码共享，投入产出比最高

### 3.2 目录结构规划

```
Lucky/Source/Lucky/UI/
├── UICore.h / UICore.cpp           // Layer 1: ID 管理、布局辅助、基础工具
├── ScopedGuards.h                  // Layer 1: RAII 工具类（ScopedStyle/Color/ID/Font）
├── Theme.h / Theme.cpp             // Layer 1: 主题颜色、样式常量、主题切换
├── DrawUtils.h / DrawUtils.cpp     // Layer 1: 绘制辅助（Underline、Separator 等）
├── Controls.h / Controls.cpp       // Layer 2: 增强控件（UI::DragFloat 等）
├── PropertyGrid.h / PropertyGrid.cpp // Layer 3: Property 系列语义化控件
└── Widgets.h / Widgets.cpp         // Layer 3: 高级复合控件（Vec3Editor 等）
```

### 3.3 命名空间

```cpp
namespace Lucky::UI {
    // Layer 1
    void PushID();
    void PopID();
    const char* GenerateID();
    void BeginPropertyGrid();
    void EndPropertyGrid();
    
    // Layer 2
    bool DragFloat(const char* label, float* v, float v_speed = 1.0f, ...);
    bool ColorEdit3(const char* label, float col[3], ...);
    bool ImageButton(const Ref<Texture2D>& texture, const ImVec2& size, ...);
    
    // Layer 3
    bool PropertyFloat(const char* label, float& value, float delta = 0.1f, ...);
    bool PropertyColor(const char* label, glm::vec3& value);
    bool PropertyTexture(const char* label, Ref<Texture2D>& texture);
    bool PropertyCombo(const char* label, int& selected, const char** options, int count);
    
    // 主题
    namespace Theme {
        namespace Colors { ... }
        namespace Styles { ... }
    }
}
```

---

## 四、核心难点分析与解决方案

### 4.1 封装粒度 — 到底暴露多少参数

**问题**：ImGui 原生 `DragFloat` 有 7 个参数（label, v, speed, min, max, format, flags），封装时应该暴露多少？

**Hazel 的做法**：在 Property 层只暴露常用参数（label, value, delta, min, max, helpText），砍掉了 `format` 和 `flags`。但这导致需要整数格式 `"%.0f"` 或对数拖拽 `ImGuiSliderFlags_Logarithmic` 时无法实现。

**本项目的解决方案 — 三层策略**：

```cpp
// Layer 2: 完整参数，和 ImGui 原生一致（不存在"暴露多少"的纠结）
bool UI::DragFloat(const char* label, float* v, 
                   float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, 
                   const char* format = "%.3f", ImGuiSliderFlags flags = 0);

// Layer 3: 精简参数，面向编辑器（大幅简化）
bool UI::PropertyFloat(const char* label, float& value, 
                       float delta = 0.1f, float min = 0.0f, float max = 0.0f);
```

- 编辑器面板 90% 的场景用 `PropertyFloat`（精简参数）
- 特殊需求降级到 `UI::DragFloat`（完整参数）
- 极端情况直接用 `ImGui::DragFloat`（原生）

### 4.2 Label + 控件的自适应布局

**问题**：ImGui 原生的 Label 在控件右侧，而 Unity 风格是 Label 在左、控件在右，且需要响应面板宽度变化。

**方案对比**：

| 方案 | 优点 | 缺点 |
|------|------|------|
| `ImGui::Columns(2)` | 实现简单，Hazel 采用此方案 | 旧 API，嵌套有 bug，列宽不能自动响应 |
| `ImGui::Table` | 新 API，支持嵌套，列宽可自动调整 | 稍复杂 |
| 手动 `SameLine` + `SetCursorPosX` | 完全可控 | 代码繁琐，每个控件都要手动计算 |

**本项目推荐方案：`ImGui::Table`**

```cpp
void BeginPropertyGrid()
{
    PushID();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
    
    ImGui::BeginTable("##PropertyGrid", 2, 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp);
    
    // Label 列：动态计算宽度，至少 80px，最多占面板宽度的 35%
    float panelWidth = ImGui::GetContentRegionAvail().x;
    float labelWidth = glm::max(panelWidth * 0.35f, 80.0f);
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, labelWidth);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
}

void EndPropertyGrid()
{
    ImGui::EndTable();
    ImGui::PopStyleVar(2);
    PopID();
}
```

每个 Property 控件内部的统一模式：

```cpp
bool PropertyFloat(const char* label, float& value, float delta, float min, float max)
{
    bool modified = false;
    
    ImGui::TableNextRow();
    
    // 第一列：Label
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::Text(label);
    
    // 第二列：控件
    ImGui::TableSetColumnIndex(1);
    ImGui::PushItemWidth(-1);  // 填满剩余宽度
    modified = UI::DragFloat(GenerateID(), &value, delta, min, max);
    ImGui::PopItemWidth();
    
    return modified;
}
```

### 4.3 ID 冲突问题

**问题**：当同一面板中绘制两个材质，每个材质都有 `"Color"` 属性时，如果用 label 作为 ID，两个 `"Color"` 会 ID 冲突，导致只有一个能正常交互。

**当前项目中已有此问题**：InspectorPanel 的材质编辑器中使用 `std::format("##{0}{1}", prop.Name, textureID)` 手动拼接唯一 ID，这种方式容易出错且不统一。

**解决方案：全局自增 ID 生成器**

```cpp
// UICore.h
namespace Lucky::UI {
    static int s_UIContextID = 0;
    static uint32_t s_Counter = 0;
    static char s_IDBuffer[16 + 2 + 1] = "##";
    
    // 生成唯一 ID（"##0", "##1", "##2" ...）
    const char* GenerateID()
    {
        snprintf(s_IDBuffer + 2, 16, "%u", s_Counter++);
        return s_IDBuffer;
    }
    
    // 在每个组件块开始时调用，重置计数器
    void PushID()
    {
        ImGui::PushID(s_UIContextID++);
        s_Counter = 0;
    }
    
    void PopID()
    {
        ImGui::PopID();
        s_UIContextID--;
    }
}
```

这样即使 label 相同，底层 ID 也不会冲突。Property 控件内部使用 `GenerateID()` 而非 label 作为 ImGui 控件的 ID，label 仅用于显示。

### 4.4 返回值语义不一致

**问题**：ImGui 的 `DragFloat` 返回 `true` 表示"值被修改了"，但 `BeginCombo` 返回 `true` 表示"下拉框打开了"。封装时容易混淆。

**解决方案**：所有 Property 系列控件统一返回 `bool modified`，表示"值是否被用户修改"。

```cpp
// PropertyCombo 内部处理语义差异
bool PropertyCombo(const char* label, int& selected, const char** options, int count)
{
    bool modified = false;
    
    // ... Label 列 ...
    
    // BeginCombo 返回的是"是否打开"，不是"是否修改"
    if (ImGui::BeginCombo(GenerateID(), options[selected]))
    {
        for (int i = 0; i < count; i++)
        {
            bool isSelected = (i == selected);
            if (ImGui::Selectable(options[i], isSelected))
            {
                if (i != selected)
                {
                    selected = i;
                    modified = true;  // 真正的"值被修改"
                }
            }
        }
        ImGui::EndCombo();
    }
    
    return modified;
}
```

### 4.5 Undo/Redo 集成点

**问题**：当前项目还没有 Undo/Redo 系统，但封装层是最佳的集成点。

**预留设计**：在 Property 控件内部预留 hook 点，未来自动获得 Undo/Redo 能力。

```cpp
bool PropertyFloat(const char* label, float& value, float delta, float min, float max)
{
    float oldValue = value;
    bool modified = /* ... 绘制控件 ... */;
    
    // TODO: 未来集成 Undo/Redo
    // if (modified && UndoSystem::IsRecording())
    // {
    //     UndoSystem::Record(label, oldValue, value);
    // }
    
    return modified;
}
```

如果不在封装层预留，未来加 Undo/Redo 时需要在每个 `ImGui::DragFloat` 调用处手动加记录逻辑，工作量巨大。

### 4.6 引擎类型转换（纹理等）

**问题**：当前 `ImageButton` 需要传 `(ImTextureID)textureID`，这是 OpenGL 的 `GLuint` 强转。

**解决方案**：封装层接受引擎类型 `Ref<Texture2D>`，内部处理转换。

```cpp
bool ImageButton(const Ref<Texture2D>& texture, const ImVec2& size, 
                 const ImVec2& uv0 = {0, 0}, const ImVec2& uv1 = {1, 1})
{
    uint32_t texID = texture ? texture->GetRendererID() : 0;
    return ImGui::ImageButton((ImTextureID)(intptr_t)texID, size, uv0, uv1);
}
```

好处：
- 调用者不需要知道 `ImTextureID` 是什么
- 未来切换到 Vulkan/DX12 时，只需要改封装层内部

### 4.7 拖拽（Drag & Drop）集成

**问题**：ImGui 的拖拽 API 比较底层，当纹理槽需要支持"从资产浏览器拖拽纹理"时，逻辑会和控件封装紧密耦合。

**解决方案**：在 `PropertyTexture` 内部集成拖拽接收。

```cpp
bool PropertyTexture(const char* label, Ref<Texture2D>& texture)
{
    // ... 绘制纹理预览和点击选择逻辑 ...
    
    // 拖拽接收（未来资产浏览器实现后启用）
    // if (ImGui::BeginDragDropTarget())
    // {
    //     if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_TEXTURE"))
    //     {
    //         // 从 payload 中获取纹理路径，创建纹理
    //     }
    //     ImGui::EndDragDropTarget();
    // }
    
    return modified;
}
```

### 4.8 样式一致性的隐性耦合

**问题**：封装控件内部的偏移量（如 `ShiftCursor(10.0f, 9.0f)`）是硬编码的"魔法数字"，如果全局字体大小改变，所有偏移值都要手动调整。

**解决方案**：将这些数值提取为主题常量。

```cpp
namespace Theme {
    namespace Layout {
        constexpr float PropertyLabelOffsetX = 10.0f;
        constexpr float PropertyLabelOffsetY = 9.0f;
        constexpr float PropertyValueOffsetY = 4.0f;
        constexpr float PropertyRowSpacing = 8.0f;
        constexpr float ComponentHeaderPadding = 4.0f;
        constexpr float TexturePreviewSize = 64.0f;
    }
}
```

---

## 五、RAII 工具类设计

### 5.1 ScopedStyle

自动管理 `ImGui::PushStyleVar` / `ImGui::PopStyleVar`，消除不匹配风险。

```cpp
class ScopedStyle
{
public:
    ScopedStyle(ImGuiStyleVar idx, float val)
    {
        ImGui::PushStyleVar(idx, val);
    }
    
    ScopedStyle(ImGuiStyleVar idx, const ImVec2& val)
    {
        ImGui::PushStyleVar(idx, val);
    }
    
    ~ScopedStyle()
    {
        ImGui::PopStyleVar();
    }
    
    // 禁止拷贝和移动
    ScopedStyle(const ScopedStyle&) = delete;
    ScopedStyle& operator=(const ScopedStyle&) = delete;
};
```

### 5.2 ScopedColor

```cpp
class ScopedColor
{
public:
    ScopedColor(ImGuiCol idx, const ImVec4& color)
    {
        ImGui::PushStyleColor(idx, color);
    }
    
    ScopedColor(ImGuiCol idx, uint32_t color)
    {
        ImGui::PushStyleColor(idx, color);
    }
    
    ~ScopedColor()
    {
        ImGui::PopStyleColor();
    }
    
    ScopedColor(const ScopedColor&) = delete;
    ScopedColor& operator=(const ScopedColor&) = delete;
};
```

### 5.3 ScopedID

```cpp
class ScopedID
{
public:
    ScopedID(int id) { ImGui::PushID(id); }
    ScopedID(const char* id) { ImGui::PushID(id); }
    ScopedID(const void* id) { ImGui::PushID(id); }
    ~ScopedID() { ImGui::PopID(); }
    
    ScopedID(const ScopedID&) = delete;
    ScopedID& operator=(const ScopedID&) = delete;
};
```

### 5.4 ScopedFont

```cpp
class ScopedFont
{
public:
    ScopedFont(ImFont* font) { ImGui::PushFont(font); }
    ~ScopedFont() { ImGui::PopFont(); }
    
    ScopedFont(const ScopedFont&) = delete;
    ScopedFont& operator=(const ScopedFont&) = delete;
};
```

### 5.5 ScopedDisable

```cpp
class ScopedDisable
{
public:
    ScopedDisable(bool disabled = true)
    {
        if (disabled)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }
        m_Disabled = disabled;
    }
    
    ~ScopedDisable()
    {
        if (m_Disabled)
        {
            ImGui::PopStyleVar();
            ImGui::PopItemFlag();
        }
    }
    
    ScopedDisable(const ScopedDisable&) = delete;
    ScopedDisable& operator=(const ScopedDisable&) = delete;
    
private:
    bool m_Disabled;
};
```

---

## 六、主题系统设计

### 6.1 颜色常量

```cpp
namespace Lucky::UI::Theme {
    namespace Colors {
        // ---- 基础色 ----
        constexpr ImVec4 Accent       = { 0.26f, 0.59f, 0.98f, 1.00f };  // 强调色（蓝色）
        constexpr ImVec4 Highlight    = { 0.26f, 0.59f, 0.98f, 0.40f };  // 高亮色
        constexpr ImVec4 Error        = { 0.90f, 0.25f, 0.25f, 1.00f };  // 错误色（红色）
        constexpr ImVec4 Warning      = { 0.95f, 0.75f, 0.20f, 1.00f };  // 警告色（黄色）
        
        // ---- 背景色 ----
        constexpr ImVec4 Background       = { 0.18f, 0.18f, 0.18f, 1.00f };  // 主背景
        constexpr ImVec4 BackgroundDark   = { 0.14f, 0.14f, 0.14f, 1.00f };  // 深色背景
        constexpr ImVec4 BackgroundPopup  = { 0.20f, 0.20f, 0.20f, 1.00f };  // 弹窗背景
        
        // ---- 文字色 ----
        constexpr ImVec4 Text         = { 1.00f, 1.00f, 1.00f, 1.00f };  // 主文字
        constexpr ImVec4 TextDimmed   = { 0.50f, 0.50f, 0.50f, 1.00f };  // 次要文字
        constexpr ImVec4 TextDisabled = { 0.36f, 0.36f, 0.36f, 1.00f };  // 禁用文字
        
        // ---- 控件色 ----
        constexpr ImVec4 FrameBg        = { 0.22f, 0.22f, 0.22f, 1.00f };  // 输入框背景
        constexpr ImVec4 FrameBgHovered = { 0.28f, 0.28f, 0.28f, 1.00f };  // 输入框悬停
        constexpr ImVec4 FrameBgActive  = { 0.32f, 0.32f, 0.32f, 1.00f };  // 输入框激活
        
        // ---- 组件头色 ----
        constexpr ImVec4 Header        = { 0.22f, 0.22f, 0.22f, 1.00f };
        constexpr ImVec4 HeaderHovered = { 0.28f, 0.28f, 0.28f, 1.00f };
        constexpr ImVec4 HeaderActive  = { 0.32f, 0.32f, 0.32f, 1.00f };
        
        // ---- Vec3 编辑器轴色 ----
        constexpr ImVec4 AxisX = { 0.80f, 0.15f, 0.15f, 1.00f };  // X 轴红色
        constexpr ImVec4 AxisY = { 0.20f, 0.70f, 0.20f, 1.00f };  // Y 轴绿色
        constexpr ImVec4 AxisZ = { 0.15f, 0.35f, 0.80f, 1.00f };  // Z 轴蓝色
    }
}
```

### 6.2 主题切换（未来扩展）

```cpp
enum class ThemePreset
{
    Dark,       // 默认深色主题
    Light,      // 浅色主题
    Custom      // 自定义主题（从配置文件加载）
};

void ApplyTheme(ThemePreset preset);
void LoadThemeFromFile(const std::string& filepath);
void SaveThemeToFile(const std::string& filepath);
```

---

## 七、封装范围决策

### 7.1 推荐封装的控件

| 优先级 | 控件 | 理由 |
|:---:|------|------|
| **P0** | `PropertyFloat` / `PropertyFloat2` / `PropertyFloat3` / `PropertyFloat4` | InspectorPanel 中重复代码最多 |
| **P0** | `PropertyColor` (vec3/vec4) | 颜色编辑需要统一风格 |
| **P0** | `PropertyTexture` | 涉及引擎类型 `Ref<Texture2D>` 的转换 |
| **P0** | `ScopedStyle` / `ScopedColor` / `ScopedID` | RAII 风格管理，消除 Push/Pop 不匹配风险 |
| **P1** | `PropertyCombo` / `PropertyEnum` | Shader 选择等下拉框 |
| **P1** | `PropertyInt` / `PropertyString` | 统一风格 |
| **P1** | `ComponentHeader` | 组件折叠头（替代当前 `DrawComponent` 中的 TreeNode 逻辑） |
| **P1** | `BeginPropertyGrid` / `EndPropertyGrid` | 属性面板两列布局 |
| **P2** | `Vec3Editor`（带 XYZ 彩色按钮） | Transform 编辑器专用，视觉效果好 |
| **P2** | `UI::Image` / `UI::ImageButton` | 接受 `Ref<Texture2D>` 而非 `ImTextureID` |
| **P2** | `Theme::Colors` 颜色常量集合 | 统一全局风格 |

### 7.2 不建议封装的

| 类别 | 示例 | 理由 |
|------|------|------|
| 查询类 API | `IsMouseClicked`, `GetCursorPos`, `IsWindowHovered` | 封装后只是无意义的转发 |
| 底层布局 API | `SameLine`, `SetCursorPos`, `Spacing`, `Indent` | 使用场景差异大，封装收益低 |
| 窗口管理 API | `Begin/End` | 已在 `EditorPanel` 基类中封装 |
| 菜单 API | `BeginMenu`, `MenuItem` | 菜单结构各不相同，封装收益低 |
| ImGui 生命周期 | `CreateContext`, `NewFrame`, `Render` | 仅在 `ImGuiLayer` 中使用一次 |

---

## 八、Hazel 参考实现总结

Hazel 项目在 `Hazel::UI` 命名空间下建立了完整的封装体系（约 20 个文件，300KB 代码），其核心设计值得借鉴：

### 8.1 值得借鉴的

| 设计 | 说明 | 本项目是否采用 |
|------|------|:---:|
| RAII Scoped 工具类 | `ScopedStyle`, `ScopedColour`, `ScopedFont`, `ScopedID` | ? 采用 |
| 全局 ID 生成器 | `GenerateID()` + `PushID()`/`PopID()` 作用域管理 | ? 采用 |
| Property 系统 | `Property(label, value, ...)` 统一的属性控件接口 | ? 采用 |
| 主题颜色常量 | `Colors::Theme::accent`, `background`, `text` 等 | ? 采用 |
| `BeginPropertyGrid` 两列布局 | 使用 `ImGui::Columns(2)` 实现 Label+控件布局 | ?? 改用 `ImGui::Table` |
| 引擎类型 Image 封装 | `UI::Image(Ref<Texture2D>, ...)` | ? 采用 |

### 8.2 需要改进的

| Hazel 的做法 | 问题 | 本项目的改进 |
|------|------|------|
| 使用 `ImGui::Columns(2)` | 旧 API，嵌套有 bug | 改用 `ImGui::Table` |
| Property 层砍掉 `format` 和 `flags` 参数 | 特殊需求无法满足 | 三层架构，Layer 2 保留完整参数 |
| 所有封装放在一个 71KB 的 `ImGui.h` 中 | 文件过大，编译慢 | 按职责拆分为多个文件 |
| 偏移量硬编码（`ShiftCursor(10.0f, 9.0f)`） | 字体大小改变时需要手动调整 | 提取为 `Theme::Layout` 常量 |

---

## 九、封装后的代码对比

### 9.1 InspectorPanel 中的灯光组件（封装前 vs 封装后）

**封装前（当前代码）：**

```cpp
DrawComponent<DirectionalLightComponent>("Directional Light", entity, [](DirectionalLightComponent& light)
{
    ImGui::ColorEdit3("Color", glm::value_ptr(light.Color));
    ImGui::DragFloat("Intensity", &light.Intensity, 0.01f, 0.0f, 10.0f);
});
```

**封装后（使用 Property 系统）：**

```cpp
DrawComponent<DirectionalLightComponent>("Directional Light", entity, [](DirectionalLightComponent& light)
{
    UI::PropertyColor("Color", light.Color);
    UI::PropertyFloat("Intensity", light.Intensity, 0.01f, 0.0f, 10.0f);
});
```

差异不大，但 Property 版本自动处理了：Label 在左侧、控件在右侧的两列布局、统一的行间距和下划线、自动 ID 生成（不会冲突）。

### 9.2 材质编辑器中的纹理属性（封装前 vs 封装后）

**封装前（当前代码，约 20 行）：**

```cpp
case ShaderUniformType::Sampler2D:
{
    Ref<Texture2D> texture = std::get<Ref<Texture2D>>(prop.Value);
    if (!texture)
        texture = Renderer3D::GetDefaultTexture(TextureDefault::Black);
    
    uint32_t textureID = texture->GetRendererID();
    std::string strID = std::format("##{0}{1}", prop.Name, textureID);
    
    ImGui::PushID(strID.c_str());
    if (ImGui::ImageButton((ImTextureID)textureID, { 64, 64 }, { 0, 1 }, { 1, 0 }))
    {
        std::string filepath = FileDialogs::OpenFile("Texture(*.png;*.jpg)\0*.png;*.jpg\0");
        if (!filepath.empty())
            material->SetTexture(prop.Name, Texture2D::Create(filepath));
    }
    ImGui::SameLine();
    ImGui::Text(prop.Name.c_str());
    ImGui::PopID();
    break;
}
```

**封装后（约 5 行）：**

```cpp
case ShaderUniformType::Sampler2D:
{
    Ref<Texture2D> texture = std::get<Ref<Texture2D>>(prop.Value);
    if (UI::PropertyTexture(prop.Name.c_str(), texture))
    {
        material->SetTexture(prop.Name, texture);
    }
    break;
}
```

代码量减少 75%，且 ID 冲突、类型转换、默认纹理处理等逻辑全部内聚在 `PropertyTexture` 中。

---

## 十、分阶段实施路线

### Phase 1 — 基础设施（建议近期实施）

**目标**：建立封装层的基础框架，不改变现有面板代码。

**工作内容**：
- 创建 `Lucky/Source/Lucky/UI/` 目录
- 实现 RAII 工具类：`ScopedStyle`、`ScopedColor`、`ScopedID`、`ScopedFont`、`ScopedDisable`
- 实现 `UICore`：`GenerateID()`、`PushID()`/`PopID()`
- 实现 `Theme::Colors` 颜色常量集合（从当前 `ImGuiLayer::SetDefaultStyles()` 中提取）
- 实现 `Theme::Layout` 布局常量

**预估工作量**：~200-300 行代码

### Phase 2 — 核心控件封装（建议中期实施）

**目标**：封装高频控件，迁移 InspectorPanel。

**工作内容**：
- 实现 `BeginPropertyGrid()` / `EndPropertyGrid()`（基于 `ImGui::Table`）
- 封装 Property 系列控件：`PropertyFloat/2/3/4`、`PropertyInt`、`PropertyString`、`PropertyColor`、`PropertyTexture`、`PropertyCombo`
- 封装 `ComponentHeader`（替代 `DrawComponent` 模板中的 TreeNode 逻辑）
- 封装 `UI::Image` / `UI::ImageButton`（接受 `Ref<Texture2D>`）
- 迁移 InspectorPanel 使用新控件

**预估工作量**：~500-800 行代码

### Phase 3 — 主题与偏好系统（建议中后期实施）

**目标**：支持主题切换和偏好设置。

**工作内容**：
- 实现 `ThemePreset` 枚举和 `ApplyTheme()` 函数
- 实现 Dark/Light 两套主题
- 实现偏好设置面板（类似 Unity Edit > Preferences）
- 主题序列化到配置文件（JSON/YAML）

**预估工作量**：~300-500 行代码

### Phase 4 — 高级控件与脚本绑定（远期目标）

**目标**：实现高级复合控件，为脚本绑定做准备。

**工作内容**：
- 实现 `Vec3Editor`（带 XYZ 彩色按钮的向量编辑器）
- 实现 `AssetField`（资产引用字段，支持拖拽）
- 将 Phase 2 的控件接口注册到脚本引擎
- 实现 `CustomEditor` 基类（类似 Unity 的 `Editor` 类）

**预估工作量**：取决于脚本系统的实现

---

## 十一、总结

| 设想 | 可行性 | 必要性 | 建议 |
|------|:---:|:---:|------|
| 完全消除 ImGui 原生代码 | ? 低 | ? 低 | 采用选择性封装，不追求 100% |
| 每种控件封装 | ?? 部分 | ?? 部分 | 只封装高频、复合、涉及引擎类型的控件 |
| 全局样式/主题设置 | ? 高 | ? 高 | **最推荐优先实施**，投入产出比最高 |
| 偏好设置面板 | ? 高 | ?? 中 | 可在主题系统之后实施 |
| 脚本调用封装接口 | ? 高 | ? 高 | 远期目标，当前只需保证接口设计干净 |

**核心原则**：封装的目标不是"消灭 `ImGui::`"，而是 **"减少重复代码、统一视觉风格、为未来扩展预留接口"**。
