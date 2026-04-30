# ImGui 封装 Layer 1：基础设施层详细设计

> **文档类型**：详细设计文档（可直接编码实现）  
> **创建日期**：2025-07  
> **状态**：待实施  
> **所属层级**：Layer 1 — 基础设施  
> **关联文档**：[ImGui_Encapsulation_Strategy.md](./ImGui_Encapsulation_Strategy.md)（策略总览）  
> **代码规范**：[Coding_Style_Guide.md](../Coding_Style_Guide.md)

---

## 一、概述

本文档定义 `Lucky::UI` 封装层的 **基础设施**，包括：

1. **RAII 工具类**（ScopedGuards）：自动管理 ImGui 的 Push/Pop 配对
2. **ID 管理系统**：解决控件 ID 冲突问题
3. **光标辅助函数**：简化光标偏移操作
4. **布局常量**（Theme::Layout）：消除硬编码魔法数字
5. **绘制辅助**（Draw 命名空间）：下划线、焦点高亮等视觉效果

这些是所有上层控件（Layer 2 增强控件、Layer 3 语义化控件）的基础依赖。

---

## 二、文件结构

```
Lucky/Source/Lucky/UI/
├── ScopedGuards.h              // RAII 工具类（纯头文件，无 .cpp）
├── UICore.h / UICore.cpp       // ID 管理、光标辅助
├── Theme.h                     // 布局常量（纯头文件，无 .cpp）
└── DrawUtils.h / DrawUtils.cpp // 绘制辅助
```

---

## 三、ScopedGuards.h — RAII 工具类

### 3.1 设计目标

消除 `ImGui::PushStyleVar` / `ImGui::PopStyleVar` 等配对调用的不匹配风险。利用 C++ RAII 机制，在作用域结束时自动 Pop。

### 3.2 实现方式选择

#### 方案 A：每个类型一个独立类（推荐 ???）

```cpp
class ScopedStyle { ... };
class ScopedColor { ... };
class ScopedID { ... };
class ScopedFont { ... };
class ScopedDisable { ... };
```

**优点**：类型安全，语义清晰，IDE 自动补全友好  
**缺点**：类数量多（5 个）

#### 方案 B：统一模板类

```cpp
template<typename PushFunc, typename PopFunc>
class ScopedGuard { ... };

using ScopedStyle = ScopedGuard<...>;
using ScopedColor = ScopedGuard<...>;
```

**优点**：代码量少  
**缺点**：模板错误信息难读，IDE 补全不友好，不同 Push 函数签名不一致导致模板参数复杂

**结论**：采用 **方案 A**，与 Hazel 一致，代码清晰易维护。

### 3.3 完整代码

```cpp
// ScopedGuards.h
#pragma once

#include <imgui/imgui.h>

namespace Lucky::UI
{
    /// <summary>
    /// RAII 样式变量管理：自动配对 PushStyleVar / PopStyleVar
    /// </summary>
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

        ScopedStyle(const ScopedStyle&) = delete;
        ScopedStyle& operator=(const ScopedStyle&) = delete;
    };

    /// <summary>
    /// RAII 颜色管理：自动配对 PushStyleColor / PopStyleColor
    /// </summary>
    class ScopedColor
    {
    public:
        ScopedColor(ImGuiCol idx, const ImVec4& color)
        {
            ImGui::PushStyleColor(idx, color);
        }

        ScopedColor(ImGuiCol idx, ImU32 color)
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

    /// <summary>
    /// RAII ID 管理：自动配对 PushID / PopID
    /// </summary>
    class ScopedID
    {
    public:
        ScopedID(int id)
        {
            ImGui::PushID(id);
        }

        ScopedID(const char* id)
        {
            ImGui::PushID(id);
        }

        ScopedID(const void* id)
        {
            ImGui::PushID(id);
        }

        ~ScopedID()
        {
            ImGui::PopID();
        }

        ScopedID(const ScopedID&) = delete;
        ScopedID& operator=(const ScopedID&) = delete;
    };

    /// <summary>
    /// RAII 字体管理：自动配对 PushFont / PopFont
    /// </summary>
    class ScopedFont
    {
    public:
        ScopedFont(ImFont* font)
        {
            ImGui::PushFont(font);
        }

        ~ScopedFont()
        {
            ImGui::PopFont();
        }

        ScopedFont(const ScopedFont&) = delete;
        ScopedFont& operator=(const ScopedFont&) = delete;
    };

    /// <summary>
    /// RAII 禁用状态管理：自动配对 BeginDisabled / EndDisabled
    /// 当 disabled = true 时，控件变灰且不可交互
    /// </summary>
    class ScopedDisable
    {
    public:
        /// <param name="disabled">是否禁用（默认 true）</param>
        ScopedDisable(bool disabled = true)
            : m_Disabled(disabled)
        {
            if (m_Disabled)
            {
                ImGui::BeginDisabled(true);
            }
        }

        ~ScopedDisable()
        {
            if (m_Disabled)
            {
                ImGui::EndDisabled();
            }
        }

        ScopedDisable(const ScopedDisable&) = delete;
        ScopedDisable& operator=(const ScopedDisable&) = delete;

    private:
        bool m_Disabled;
    };
}
```

### 3.4 使用示例

```cpp
// 封装前
ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 30));
bool opened = ImGui::TreeNodeEx(...);
ImGui::PopStyleVar();

// 封装后
UI::ScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2(4, 30));
bool opened = ImGui::TreeNodeEx(...);
// 自动 PopStyleVar，即使中间有 return 也不会泄漏
```

### 3.5 关于 ScopedColorStack / ScopedStyleStack 的讨论

Hazel 实现了 `ScopedColourStack` 和 `ScopedStyleStack`，支持一次 Push 多个值：

```cpp
// Hazel 的 ScopedColourStack
ScopedColourStack colors(
    ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f),
    ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f),
    ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)
);
```

#### 是否需要实现？

| 方案 | 优点 | 缺点 | 推荐度 |
|------|------|------|:---:|
| **不实现 Stack 版本** | 简单，用多个 `ScopedColor` 即可 | 多个变量名略繁琐 | ?? 其次 |
| **实现 Stack 版本** | 一行代码 Push 多个值，代码更紧凑 | 模板元编程复杂，编译错误信息难读 | ??? **最优** |

**推荐**：**实现 Stack 版本**。虽然模板稍复杂，但在材质编辑器等需要同时修改多个颜色的场景中非常有用。

#### ScopedColorStack 完整代码

```cpp
/// <summary>
/// RAII 批量颜色管理：一次 Push 多个颜色，析构时统一 Pop
/// 参数必须成对传入：(ImGuiCol, color, ImGuiCol, color, ...)
/// </summary>
class ScopedColorStack
{
public:
    ScopedColorStack(const ScopedColorStack&) = delete;
    ScopedColorStack& operator=(const ScopedColorStack&) = delete;

    template<typename ColorType, typename... OtherColors>
    ScopedColorStack(ImGuiCol firstColorID, ColorType firstColor, OtherColors&&... otherColorPairs)
        : m_Count((sizeof...(otherColorPairs) / 2) + 1)
    {
        static_assert((sizeof...(otherColorPairs) & 1u) == 0,
            "ScopedColorStack 参数必须成对传入：(ImGuiCol, color) 对");

        PushColor(firstColorID, firstColor, std::forward<OtherColors>(otherColorPairs)...);
    }

    ~ScopedColorStack()
    {
        ImGui::PopStyleColor(m_Count);
    }

private:
    int m_Count;

    template<typename ColorType, typename... OtherColors>
    void PushColor(ImGuiCol colorID, ColorType color, OtherColors&&... otherColorPairs)
    {
        if constexpr (sizeof...(otherColorPairs) == 0)
        {
            ImGui::PushStyleColor(colorID, ImColor(color).Value);
        }
        else
        {
            ImGui::PushStyleColor(colorID, ImColor(color).Value);
            PushColor(std::forward<OtherColors>(otherColorPairs)...);
        }
    }
};

/// <summary>
/// RAII 批量样式管理：一次 Push 多个样式变量，析构时统一 Pop
/// 参数必须成对传入：(ImGuiStyleVar, value, ImGuiStyleVar, value, ...)
/// </summary>
class ScopedStyleStack
{
public:
    ScopedStyleStack(const ScopedStyleStack&) = delete;
    ScopedStyleStack& operator=(const ScopedStyleStack&) = delete;

    template<typename ValueType, typename... OtherStylePairs>
    ScopedStyleStack(ImGuiStyleVar firstStyleVar, ValueType firstValue, OtherStylePairs&&... otherStylePairs)
        : m_Count((sizeof...(otherStylePairs) / 2) + 1)
    {
        static_assert((sizeof...(otherStylePairs) & 1u) == 0,
            "ScopedStyleStack 参数必须成对传入：(ImGuiStyleVar, value) 对");

        PushStyle(firstStyleVar, firstValue, std::forward<OtherStylePairs>(otherStylePairs)...);
    }

    ~ScopedStyleStack()
    {
        ImGui::PopStyleVar(m_Count);
    }

private:
    int m_Count;

    template<typename ValueType, typename... OtherStylePairs>
    void PushStyle(ImGuiStyleVar styleVar, ValueType value, OtherStylePairs&&... otherStylePairs)
    {
        if constexpr (sizeof...(otherStylePairs) == 0)
        {
            ImGui::PushStyleVar(styleVar, value);
        }
        else
        {
            ImGui::PushStyleVar(styleVar, value);
            PushStyle(std::forward<OtherStylePairs>(otherStylePairs)...);
        }
    }
};
```

---

## 四、UICore.h / UICore.cpp — ID 管理与光标辅助

### 4.1 ID 管理系统

#### 问题背景

当前 InspectorPanel 的材质编辑器中使用 `std::format("##{0}{1}", prop.Name, textureID)` 手动拼接唯一 ID，这种方式：
- 容易出错（忘记拼接、格式不一致）
- 不统一（每个开发者可能用不同的拼接方式）
- 当同一面板绘制多个同名属性时仍可能冲突

#### 解决方案

采用 Hazel 的全局自增 ID 生成器方案，配合 `PushID`/`PopID` 作用域管理。

#### 实现方式选择

##### 方案 A：文件作用域静态变量（推荐 ???）

```cpp
// UICore.cpp
namespace Lucky::UI
{
    static int s_UIContextID = 0;       // 上下文 ID（PushID/PopID 管理）
    static uint32_t s_Counter = 0;      // 自增计数器
    static char s_IDBuffer[16] = "##";  // ID 缓冲区（"##0", "##1", ...）
}
```

**优点**：简单直接，与 Hazel 一致，性能好  
**缺点**：非线程安全（但 ImGui 本身也不是线程安全的，所以不是问题）

##### 方案 B：线程局部存储（thread_local）

```cpp
static thread_local int s_UIContextID = 0;
static thread_local uint32_t s_Counter = 0;
```

**优点**：线程安全  
**缺点**：ImGui 本身不支持多线程渲染，过度设计

**结论**：采用 **方案 A**，与 ImGui 的单线程模型一致。

### 4.2 完整代码

#### UICore.h

```cpp
// UICore.h
#pragma once

#include <imgui/imgui.h>

namespace Lucky::UI
{
    /// <summary>
    /// 生成唯一 ID（格式："##0", "##1", "##2" ...）
    /// 在 PushID/PopID 作用域内使用，每次调用返回不同的 ID
    /// </summary>
    /// <returns>唯一 ID 字符串（指向内部静态缓冲区，下次调用会覆盖）</returns>
    const char* GenerateID();

    /// <summary>
    /// 进入新的 ID 作用域：Push 一个唯一的上下文 ID，并重置计数器
    /// 必须与 PopID() 配对使用
    /// </summary>
    void PushID();

    /// <summary>
    /// 退出当前 ID 作用域
    /// </summary>
    void PopID();

    /// <summary>
    /// 水平偏移光标
    /// </summary>
    /// <param name="distance">偏移距离（像素）</param>
    void ShiftCursorX(float distance);

    /// <summary>
    /// 垂直偏移光标
    /// </summary>
    /// <param name="distance">偏移距离（像素）</param>
    void ShiftCursorY(float distance);

    /// <summary>
    /// 同时偏移光标的水平和垂直位置
    /// </summary>
    /// <param name="x">水平偏移距离（像素）</param>
    /// <param name="y">垂直偏移距离（像素）</param>
    void ShiftCursor(float x, float y);
}
```

#### UICore.cpp

```cpp
// UICore.cpp
#include "lcpch.h"
#include "UICore.h"

#include <cstdio>

namespace Lucky::UI
{
    static int s_UIContextID = 0;       // 上下文 ID（PushID/PopID 管理）
    static uint32_t s_Counter = 0;      // 自增计数器
    static char s_IDBuffer[16] = "##";  // ID 缓冲区

    const char* GenerateID()
    {
        snprintf(s_IDBuffer + 2, sizeof(s_IDBuffer) - 2, "%u", s_Counter++);
        return s_IDBuffer;
    }

    void PushID()
    {
        ImGui::PushID(s_UIContextID++);
        s_Counter = 0;  // 重置计数器，每个作用域从 0 开始
    }

    void PopID()
    {
        ImGui::PopID();
        s_UIContextID--;
    }

    void ShiftCursorX(float distance)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + distance);
    }

    void ShiftCursorY(float distance)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + distance);
    }

    void ShiftCursor(float x, float y)
    {
        const ImVec2 cursor = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cursor.x + x, cursor.y + y));
    }
}
```

### 4.3 使用示例

```cpp
// 材质编辑器中绘制多个材质的属性
for (Ref<Material>& material : meshRenderer.Materials)
{
    UI::PushID();  // 每个材质一个独立的 ID 作用域
    
    // 即使两个材质都有 "Color" 属性，ID 也不会冲突
    // 因为 GenerateID() 在不同的 PushID 作用域内生成不同的 ID
    UI::PropertyColor("Color", material->GetColor());
    UI::PropertyFloat("Metallic", material->GetMetallic());
    
    UI::PopID();
}
```

---

## 五、Theme.h — 布局常量

### 5.1 设计目标

将当前代码中散落的硬编码数值（如 `ImVec2(4, 30)`、`{ 64, 64 }`、`0.01f` 等）提取为命名常量，方便统一调整。

### 5.2 与 EditorPreferences 的关系

- `EditorPreferences::ColorSettings` → 管理 **运行时可修改的颜色**（通过 Preferences 面板调整）
- `UI::Theme::Layout` → 管理 **编译期布局常量**（间距、偏移、尺寸等），不涉及颜色

### 5.3 完整代码

```cpp
// Theme.h
#pragma once

namespace Lucky::UI::Theme
{
    /// <summary>
    /// 布局常量：控件间距、偏移、尺寸等
    /// </summary>
    namespace Layout
    {
        // ---- PropertyGrid 布局 ----
        constexpr float PropertyLabelOffsetX = 10.0f;       // Label 列水平偏移
        constexpr float PropertyLabelOffsetY = 9.0f;        // Label 列垂直偏移
        constexpr float PropertyValueOffsetY = 4.0f;        // Value 列垂直偏移
        constexpr float PropertyRowSpacing = 8.0f;          // 属性行间距
        constexpr float PropertyFramePaddingX = 4.0f;       // 属性控件内边距 X
        constexpr float PropertyFramePaddingY = 4.0f;       // 属性控件内边距 Y
        constexpr float PropertyLabelWidthRatio = 0.35f;    // Label 列宽占面板宽度的比例
        constexpr float PropertyLabelMinWidth = 80.0f;      // Label 列最小宽度

        // ---- 组件头部 ----
        constexpr float ComponentHeaderPaddingX = 4.0f;     // 组件头部内边距 X
        constexpr float ComponentHeaderPaddingY = 4.0f;     // 组件头部内边距 Y
        constexpr float ComponentSettingsButtonSize = 30.0f; // 组件设置按钮尺寸
        constexpr float ComponentSettingsButtonOffset = 18.0f; // 组件设置按钮右偏移

        // ---- 纹理预览 ----
        constexpr float TexturePreviewSize = 64.0f;         // 纹理预览图尺寸

        // ---- 通用 ----
        constexpr float ItemSpacingX = 8.0f;                // 控件水平间距
        constexpr float ItemSpacingY = 8.0f;                // 控件垂直间距
        constexpr float IndentSpacing = 12.0f;              // 缩进间距
        constexpr float SeparatorSpacingY = 18.0f;          // 分隔线后的垂直间距
    }
}
```

### 5.4 使用示例

```cpp
// 封装前（硬编码）
ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 30));
ImGui::SameLine(contentRegionAvail.x - 18);
if (ImGui::Button("+", { 30, 30 }))

// 封装后（使用常量）
using namespace UI::Theme::Layout;
UI::ScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2(ComponentHeaderPaddingX, ComponentHeaderPaddingY));
ImGui::SameLine(contentRegionAvail.x - ComponentSettingsButtonOffset);
if (ImGui::Button("+", { ComponentSettingsButtonSize, ComponentSettingsButtonSize }))
```

---

## 六、DrawUtils.h / DrawUtils.cpp — 绘制辅助

### 6.1 设计目标

提供统一的绘制辅助函数，用于 Property 控件的视觉增强（下划线分隔、焦点高亮等）。

### 6.2 实现方式选择

#### Draw::Underline 的实现

##### 方案 A：简单线条（推荐 ???）

```cpp
void Underline(bool fullWidth, float offsetX, float offsetY)
{
    const float width = fullWidth ? ImGui::GetWindowWidth() : ImGui::GetContentRegionAvail().x;
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(cursor.x + offsetX, cursor.y + offsetY),
        ImVec2(cursor.x + width, cursor.y + offsetY),
        IM_COL32(40, 40, 40, 255),  // 深灰色
        1.0f
    );
}
```

**优点**：简单直接，性能好  
**缺点**：颜色硬编码

##### 方案 B：使用 EditorPreferences 颜色

```cpp
void Underline(bool fullWidth, float offsetX, float offsetY)
{
    const auto& colors = EditorPreferences::Get().GetColors();
    // 使用 colors.SeparatorColor 或类似颜色
    // ...
}
```

**优点**：颜色可通过 Preferences 面板调整  
**缺点**：DrawUtils 依赖 EditorPreferences，增加了耦合

**结论**：采用 **方案 A**，使用 ImGui 主题颜色（`ImGuiCol_Separator`）而非硬编码，这样既不依赖 EditorPreferences，又能跟随主题变化。

#### DrawItemActivityOutline 的实现

这是 Hazel 的核心视觉增强：根据控件的悬停/激活/非激活状态绘制不同的边框。

##### 方案 A：简化版（推荐 ???）

仅绘制激活和悬停状态的边框，不绘制非激活状态（减少视觉噪音）。

##### 方案 B：完整版（与 Hazel 一致）

支持 `OutlineFlags` 位标志，可精细控制每种状态的边框。

**结论**：先采用 **方案 A**（简化版），后续根据需要升级为方案 B。

### 6.3 完整代码

#### DrawUtils.h

```cpp
// DrawUtils.h
#pragma once

#include <imgui/imgui.h>

namespace Lucky::UI
{
    namespace Draw
    {
        /// <summary>
        /// 绘制下划线分隔线
        /// </summary>
        /// <param name="fullWidth">是否占满窗口宽度（false 则仅占内容区域宽度）</param>
        /// <param name="offsetX">水平偏移</param>
        /// <param name="offsetY">垂直偏移（默认 -1.0f，即在当前行上方 1px）</param>
        void Underline(bool fullWidth = false, float offsetX = 0.0f, float offsetY = -1.0f);
    }

    /// <summary>
    /// 绘制控件活动状态边框（焦点高亮、悬停效果）
    /// 在控件绘制之后立即调用
    /// </summary>
    /// <param name="rounding">边框圆角半径（默认使用 ImGui 全局 FrameRounding）</param>
    void DrawItemActivityOutline(float rounding = 0.0f);
}
```

#### DrawUtils.cpp

```cpp
// DrawUtils.cpp
#include "lcpch.h"
#include "DrawUtils.h"

#include <imgui/imgui_internal.h>

namespace Lucky::UI
{
    namespace Draw
    {
        void Underline(bool fullWidth, float offsetX, float offsetY)
        {
            if (fullWidth)
            {
                if (ImGui::GetCurrentWindow()->DC.CurrentColumns != nullptr)
                {
                    ImGui::PushColumnsBackground();
                }
                else if (ImGui::GetCurrentTable() != nullptr)
                {
                    ImGui::TablePushBackgroundChannel();
                }
            }

            const float width = fullWidth ? ImGui::GetWindowWidth() : ImGui::GetContentRegionAvail().x;
            const ImVec2 cursor = ImGui::GetCursorScreenPos();

            // 使用 ImGui 主题中的分隔线颜色（跟随主题变化）
            ImU32 color = ImGui::GetColorU32(ImGuiCol_Separator);

            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(cursor.x + offsetX, cursor.y + offsetY),
                ImVec2(cursor.x + width, cursor.y + offsetY),
                color,
                1.0f
            );

            if (fullWidth)
            {
                if (ImGui::GetCurrentWindow()->DC.CurrentColumns != nullptr)
                {
                    ImGui::PopColumnsBackground();
                }
                else if (ImGui::GetCurrentTable() != nullptr)
                {
                    ImGui::TablePopBackgroundChannel();
                }
            }
        }
    }

    void DrawItemActivityOutline(float rounding)
    {
        ImGuiContext& g = *GImGui;

        // 禁用状态不绘制边框
        if (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled)
        {
            return;
        }

        if (rounding == 0.0f)
        {
            rounding = g.Style.FrameRounding;
        }

        auto* drawList = ImGui::GetWindowDrawList();
        ImRect rect = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };
        rect.Min.x -= 1.0f;
        rect.Min.y -= 1.0f;
        rect.Max.x += 1.0f;
        rect.Max.y += 1.0f;

        if (ImGui::IsItemActive())
        {
            // 激活状态：高亮边框
            drawList->AddRect(rect.Min, rect.Max, IM_COL32(60, 60, 60, 255), rounding, 0, 1.5f);
        }
        else if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            // 悬停状态：浅色边框
            drawList->AddRect(rect.Min, rect.Max, IM_COL32(60, 60, 60, 255), rounding, 0, 1.5f);
        }
        else
        {
            // 非激活状态：更浅的边框
            drawList->AddRect(rect.Min, rect.Max, IM_COL32(50, 50, 50, 255), rounding, 0, 1.0f);
        }
    }
}
```

### 6.4 关于 `imgui_internal.h` 的依赖

`DrawUtils.cpp` 需要包含 `imgui_internal.h`（用于 `GImGui`、`ImGuiItemFlags_Disabled`、`ImRect` 等）。这是可以接受的，因为：

1. Hazel 也在封装层中使用了 `imgui_internal.h`
2. 仅在 `.cpp` 文件中包含，不会污染头文件
3. `imgui_internal.h` 是 ImGui 官方提供的，API 相对稳定

---

## 七、构建系统集成

### 7.1 Premake 配置

需要在 Lucky 引擎的 Premake 脚本中添加新的 UI 目录：

```lua
-- Lucky/Build-Lucky.lua 中的 files 配置
files
{
    -- ... 现有文件 ...
    "%{prj.name}/Source/Lucky/UI/**.h",
    "%{prj.name}/Source/Lucky/UI/**.cpp",
}
```

### 7.2 预编译头文件

`UICore.cpp` 和 `DrawUtils.cpp` 的第一行必须包含 `#include "lcpch.h"`（项目规范要求）。

### 7.3 头文件包含顺序

```cpp
// .cpp 文件
#include "lcpch.h"          // 1. 预编译头文件
#include "UICore.h"         // 2. 对应的头文件
#include <imgui/imgui.h>    // 3. 第三方库（如果 .h 中未包含）

// .h 文件
#pragma once
#include <imgui/imgui.h>    // 仅包含必要的头文件
```

---

## 八、测试验证

### 8.1 验证 ScopedGuards

在现有代码中将 `PushStyleVar`/`PopStyleVar` 替换为 `ScopedStyle`，验证行为一致：

```cpp
// EditorDockSpace.cpp 中的验证
// 替换前
ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
// ... 代码 ...
ImGui::PopStyleVar(2);

// 替换后
UI::ScopedStyle windowRounding(ImGuiStyleVar_WindowRounding, 0.0f);
UI::ScopedStyle windowBorder(ImGuiStyleVar_WindowBorderSize, 0.0f);
// ... 代码 ...
// 自动 Pop
```

### 8.2 验证 ID 管理

在 InspectorPanel 的材质编辑器中验证 ID 不冲突：

```cpp
// 两个材质都有 "Color" 属性
UI::PushID();
// 材质 1 的 "Color" → ID 为 "##0" in context 0
UI::PopID();

UI::PushID();
// 材质 2 的 "Color" → ID 为 "##0" in context 1（不冲突）
UI::PopID();
```

---

## 九、总结

| 文件 | 行数估算 | 依赖 |
|------|:---:|------|
| `ScopedGuards.h` | ~180 | `imgui.h` |
| `UICore.h` | ~50 | `imgui.h` |
| `UICore.cpp` | ~50 | `lcpch.h`, `UICore.h` |
| `Theme.h` | ~40 | 无 |
| `DrawUtils.h` | ~30 | `imgui.h` |
| `DrawUtils.cpp` | ~80 | `lcpch.h`, `DrawUtils.h`, `imgui_internal.h` |
| **合计** | **~430** | |

**实施顺序**：
1. 创建 `Lucky/Source/Lucky/UI/` 目录
2. 实现 `ScopedGuards.h`（纯头文件，无依赖）
3. 实现 `Theme.h`（纯头文件，无依赖）
4. 实现 `UICore.h` / `UICore.cpp`
5. 实现 `DrawUtils.h` / `DrawUtils.cpp`
6. 更新 Premake 构建脚本
7. 在现有代码中小范围验证
