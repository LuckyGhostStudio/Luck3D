# ImGui 封装 Layer 2 & 3：Property 控件层详细设计

> **文档类型**：详细设计文档（可直接编码实现）  
> **创建日期**：2025-07  
> **状态**：待实施  
> **所属层级**：Layer 2（增强控件）+ Layer 3（Property 语义化控件）  
> **关联文档**：  
> - [ImGui_Encapsulation_Strategy.md](./ImGui_Encapsulation_Strategy.md)（策略总览）  
> - [ImGui_Layer1_Infrastructure.md](./ImGui_Layer1_Infrastructure.md)（基础设施层）  
> **代码规范**：[Coding_Style_Guide.md](../Coding_Style_Guide.md)

---

## 一、概述

本文档定义 `Lucky::UI` 封装层的 **Property 控件**，包括：

1. **Layer 2 增强控件**（Controls）：1:1 映射 ImGui 原生控件，加视觉增强（`DrawItemActivityOutline`）
2. **PropertyGrid 布局系统**：`BeginPropertyGrid` / `EndPropertyGrid`，实现 Label+控件两列布局
3. **Layer 3 Property 语义化控件**：面向 Inspector 的高级控件，内置两列布局，参数精简

这些控件直接对应当前 `InspectorPanel.cpp` 中的所有 UI 使用场景。

---

## 二、文件结构

```
Lucky/Source/Lucky/UI/
├── Controls.h / Controls.cpp           // Layer 2: 增强控件
└── PropertyGrid.h / PropertyGrid.cpp   // Layer 3: PropertyGrid 布局 + Property 系列控件
```

---

## 三、Layer 2 增强控件 — Controls.h / Controls.cpp

### 3.1 设计目标

- 参数签名与 ImGui 原生 **完全一致**（不砍参数）
- 内部自动调用 `DrawItemActivityOutline()` 添加视觉增强
- 当 Layer 3 的 Property 控件无法满足特殊需求时，降级到这层

### 3.2 实现方式选择

#### 方案 A：inline 函数在头文件中（推荐 ???）

```cpp
// Controls.h
inline bool DragFloat(const char* label, float* v, float v_speed = 1.0f, ...)
{
    bool changed = ImGui::DragFloat(label, v, v_speed, ...);
    DrawItemActivityOutline();
    return changed;
}
```

**优点**：编译器可内联优化，零开销抽象；调用者无需链接额外 .cpp  
**缺点**：头文件稍大；修改实现需要重新编译所有包含者

#### 方案 B：声明在 .h，实现在 .cpp

```cpp
// Controls.h
bool DragFloat(const char* label, float* v, float v_speed = 1.0f, ...);

// Controls.cpp
bool DragFloat(const char* label, float* v, float v_speed, ...)
{
    bool changed = ImGui::DragFloat(label, v, v_speed, ...);
    DrawItemActivityOutline();
    return changed;
}
```

**优点**：修改实现不需要重新编译包含者  
**缺点**：函数调用开销（虽然极小）

**结论**：采用 **方案 A**（inline），因为这些函数体极短（2-3 行），内联是最佳选择。与 Hazel 一致。

### 3.3 完整代码

#### Controls.h

```cpp
// Controls.h
#pragma once

#include "Lucky/UI/DrawUtils.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

namespace Lucky::UI
{
    // ========================================================================
    // Layer 2: 增强控件
    // 参数签名与 ImGui 原生完全一致，内部加 DrawItemActivityOutline 视觉增强
    // ========================================================================

    // ---- Drag 系列 ----

    inline bool DragFloat(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);
        DrawItemActivityOutline();
        return changed;
    }

    inline bool DragFloat2(const char* label, float v[2], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::DragFloat2(label, v, v_speed, v_min, v_max, format, flags);
        DrawItemActivityOutline();
        return changed;
    }

    inline bool DragFloat3(const char* label, float v[3], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::DragFloat3(label, v, v_speed, v_min, v_max, format, flags);
        DrawItemActivityOutline();
        return changed;
    }

    inline bool DragFloat4(const char* label, float v[4], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::DragFloat4(label, v, v_speed, v_min, v_max, format, flags);
        DrawItemActivityOutline();
        return changed;
    }

    inline bool DragInt(const char* label, int* v, float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::DragInt(label, v, v_speed, v_min, v_max, format, flags);
        DrawItemActivityOutline();
        return changed;
    }

    // ---- Slider 系列 ----

    inline bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
        DrawItemActivityOutline();
        return changed;
    }

    inline bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::SliderInt(label, v, v_min, v_max, format, flags);
        DrawItemActivityOutline();
        return changed;
    }

    // ---- Input 系列 ----

    inline bool InputText(const char* label, char* buf, size_t bufSize, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* userData = nullptr)
    {
        bool changed = ImGui::InputText(label, buf, bufSize, flags, callback, userData);
        DrawItemActivityOutline();
        return changed;
    }

    inline bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* userData = nullptr)
    {
        bool changed = ImGui::InputText(label, str, flags, callback, userData);
        DrawItemActivityOutline();
        return changed;
    }

    inline bool InputInt(const char* label, int* v, int step = 1, int stepFast = 100, ImGuiInputTextFlags flags = 0)
    {
        bool changed = ImGui::InputInt(label, v, step, stepFast, flags);
        DrawItemActivityOutline();
        return changed;
    }

    // ---- Color 系列 ----

    inline bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0)
    {
        bool changed = ImGui::ColorEdit3(label, col, flags);
        DrawItemActivityOutline();
        return changed;
    }

    inline bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0)
    {
        bool changed = ImGui::ColorEdit4(label, col, flags);
        DrawItemActivityOutline();
        return changed;
    }

    // ---- Combo ----

    inline bool BeginCombo(const char* label, const char* previewValue, ImGuiComboFlags flags = 0)
    {
        bool opened = ImGui::BeginCombo(label, previewValue, flags);
        DrawItemActivityOutline();
        return opened;
    }

    inline void EndCombo()
    {
        ImGui::EndCombo();
    }

    // ---- Checkbox ----

    inline bool Checkbox(const char* label, bool* v)
    {
        bool changed = ImGui::Checkbox(label, v);
        DrawItemActivityOutline();
        return changed;
    }
}
```

### 3.4 使用示例

```cpp
// Layer 2 的使用场景：需要完整参数控制
// 例如：Shadow Bias 需要 "%.4f" 格式和极小的拖拽速度
UI::DragFloat("Shadow Bias", &light.ShadowBias, 0.0001f, 0.0f, 0.05f, "%.4f");

// 例如：需要对数拖拽
UI::DragFloat("Range", &light.Range, 0.1f, 0.1f, 1000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
```

---

## 四、PropertyGrid 布局系统

### 4.1 设计目标

实现 Unity 风格的 Label 在左、控件在右的两列布局，所有 Property 控件共享此布局。

### 4.2 实现方式选择

#### 方案 A：基于 ImGui::Table（推荐 ???）

```cpp
void BeginPropertyGrid()
{
    PushID();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
    ImGui::BeginTable("##PropertyGrid", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp);
    // 设置列宽...
}
```

**优点**：
- 新 API，ImGui 官方推荐
- 支持嵌套（Table 内可以再嵌套 Table）
- 支持 `ImGuiTableFlags_Resizable`，用户可拖拽调整 Label/Value 列宽
- 列宽可自动响应面板宽度变化

**缺点**：
- 比 Columns 稍复杂
- 每个 Property 控件内部需要 `TableNextRow` + `TableSetColumnIndex`

#### 方案 B：基于 ImGui::Columns（Hazel 的做法）

```cpp
void BeginPropertyGrid(uint32_t columns)
{
    PushID();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
    ImGui::Columns(columns);
}
```

**优点**：
- 实现简单
- Hazel 已验证可行

**缺点**：
- `ImGui::Columns` 是旧 API，ImGui 官方标记为 "legacy"
- 嵌套有已知 bug
- 列宽不能自动响应面板宽度变化
- 不支持 `Resizable` 标志

#### 方案 C：手动 SameLine + SetCursorPosX

**优点**：完全可控  
**缺点**：代码繁琐，每个控件都要手动计算位置

**结论**：采用 **方案 A**（`ImGui::Table`），这是最现代、最灵活的方案。

### 4.3 PropertyGrid 完整代码

#### PropertyGrid.h（布局部分）

```cpp
// PropertyGrid.h
#pragma once

#include "Lucky/UI/UICore.h"
#include "Lucky/UI/Controls.h"
#include "Lucky/UI/DrawUtils.h"
#include "Lucky/UI/Theme.h"

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Texture.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui/imgui.h>

#include <string>

namespace Lucky::UI
{
    /// <summary>
    /// 开始 PropertyGrid 两列布局（Label 列 + Value 列）
    /// 必须与 EndPropertyGrid() 配对使用
    /// 在 BeginPropertyGrid 和 EndPropertyGrid 之间调用 Property 系列函数
    /// </summary>
    void BeginPropertyGrid();

    /// <summary>
    /// 结束 PropertyGrid 两列布局
    /// </summary>
    void EndPropertyGrid();

    // ========================================================================
    // Layer 3: Property 语义化控件
    // 内置 Label+控件两列布局，参数精简，面板代码直接调用
    // 所有 Property 函数必须在 BeginPropertyGrid / EndPropertyGrid 之间调用
    // 返回值统一为 bool modified（值是否被用户修改）
    // ========================================================================

    // ---- Float 系列 ----

    /// <summary>
    /// 浮点数属性（单个 float）
    /// </summary>
    /// <param name="label">属性名（显示在左侧 Label 列）</param>
    /// <param name="value">浮点数值引用</param>
    /// <param name="delta">拖拽速度（默认 0.1）</param>
    /// <param name="min">最小值（默认 0.0，min == max 时无限制）</param>
    /// <param name="max">最大值（默认 0.0）</param>
    /// <returns>值是否被修改</returns>
    bool PropertyFloat(const char* label, float& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f);

    /// <summary>
    /// 二维浮点数属性（glm::vec2）
    /// </summary>
    bool PropertyFloat2(const char* label, glm::vec2& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f);

    /// <summary>
    /// 三维浮点数属性（glm::vec3）
    /// </summary>
    bool PropertyFloat3(const char* label, glm::vec3& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f);

    /// <summary>
    /// 四维浮点数属性（glm::vec4）
    /// </summary>
    bool PropertyFloat4(const char* label, glm::vec4& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f);

    // ---- Int 系列 ----

    /// <summary>
    /// 整数拖拽属性
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="value">整数值引用</param>
    /// <param name="delta">拖拽速度（默认 1.0）</param>
    /// <param name="min">最小值</param>
    /// <param name="max">最大值</param>
    /// <returns>值是否被修改</returns>
    bool PropertyInt(const char* label, int& value, float delta = 1.0f, int min = 0, int max = 0);

    // ---- Color 系列 ----

    /// <summary>
    /// RGB 颜色属性（glm::vec3）
    /// </summary>
    bool PropertyColor(const char* label, glm::vec3& value);

    /// <summary>
    /// RGBA 颜色属性（glm::vec4）
    /// </summary>
    bool PropertyColor4(const char* label, glm::vec4& value);

    // ---- String 系列 ----

    /// <summary>
    /// 文本输入属性
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="value">字符串缓冲区</param>
    /// <param name="bufSize">缓冲区大小</param>
    /// <returns>值是否被修改</returns>
    bool PropertyString(const char* label, char* value, size_t bufSize);

    /// <summary>
    /// 只读文本属性
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="value">显示的文本</param>
    void PropertyReadOnlyString(const char* label, const char* value);

    // ---- Combo 下拉框 ----

    /// <summary>
    /// 下拉选择框属性
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="selected">当前选中索引引用</param>
    /// <param name="options">选项字符串数组</param>
    /// <param name="count">选项数量</param>
    /// <returns>选中项是否被修改</returns>
    bool PropertyCombo(const char* label, int& selected, const char* const* options, int count);

    // ---- Checkbox ----

    /// <summary>
    /// 复选框属性
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="value">布尔值引用</param>
    /// <returns>值是否被修改</returns>
    bool PropertyCheckbox(const char* label, bool& value);

    // ---- Texture ----

    /// <summary>
    /// 纹理属性（带预览图和选择按钮）
    /// 内部处理：默认纹理、ID 管理、引擎类型转换、文件对话框
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="texture">纹理引用（会被修改为用户选择的新纹理）</param>
    /// <returns>纹理是否被修改</returns>
    bool PropertyTexture(const char* label, Ref<Texture2D>& texture);
}
```

#### PropertyGrid.cpp

```cpp
// PropertyGrid.cpp
#include "lcpch.h"
#include "PropertyGrid.h"

#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Utils/PlatformUtils.h"

namespace Lucky::UI
{
    // ========================================================================
    // PropertyGrid 布局
    // ========================================================================

    void BeginPropertyGrid()
    {
        PushID();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::Layout::ItemSpacingX, Theme::Layout::ItemSpacingY));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::Layout::PropertyFramePaddingX, Theme::Layout::PropertyFramePaddingY));

        ImGui::BeginTable("##PropertyGrid", 2,
            ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp);

        // Label 列：固定宽度，至少 80px，最多占面板宽度的 35%
        float panelWidth = ImGui::GetContentRegionAvail().x;
        float labelWidth = std::max(panelWidth * Theme::Layout::PropertyLabelWidthRatio, Theme::Layout::PropertyLabelMinWidth);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, labelWidth);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    }

    void EndPropertyGrid()
    {
        ImGui::EndTable();
        ImGui::PopStyleVar(2);  // ItemSpacing, FramePadding
        PopID();
    }

    // ========================================================================
    // 内部辅助：Property 行的统一模式
    // ========================================================================

    /// <summary>
    /// 绘制 Property 行的 Label 列
    /// </summary>
    static void PropertyLabel(const char* label)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ShiftCursor(Theme::Layout::PropertyLabelOffsetX, 0.0f);
        ImGui::TextUnformatted(label);
    }

    /// <summary>
    /// 开始绘制 Property 行的 Value 列
    /// </summary>
    static void PropertyValueBegin()
    {
        ImGui::TableSetColumnIndex(1);
        ShiftCursorY(Theme::Layout::PropertyValueOffsetY);
        ImGui::PushItemWidth(-1);  // 填满剩余宽度
    }

    /// <summary>
    /// 结束绘制 Property 行的 Value 列
    /// </summary>
    static void PropertyValueEnd()
    {
        ImGui::PopItemWidth();
        Draw::Underline();
    }

    // ========================================================================
    // Layer 3: Property 语义化控件实现
    // ========================================================================

    // ---- Float 系列 ----

    bool PropertyFloat(const char* label, float& value, float delta, float min, float max)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = UI::DragFloat(GenerateID(), &value, delta, min, max);

        PropertyValueEnd();
        return modified;
    }

    bool PropertyFloat2(const char* label, glm::vec2& value, float delta, float min, float max)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = UI::DragFloat2(GenerateID(), glm::value_ptr(value), delta, min, max);

        PropertyValueEnd();
        return modified;
    }

    bool PropertyFloat3(const char* label, glm::vec3& value, float delta, float min, float max)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = UI::DragFloat3(GenerateID(), glm::value_ptr(value), delta, min, max);

        PropertyValueEnd();
        return modified;
    }

    bool PropertyFloat4(const char* label, glm::vec4& value, float delta, float min, float max)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = UI::DragFloat4(GenerateID(), glm::value_ptr(value), delta, min, max);

        PropertyValueEnd();
        return modified;
    }

    // ---- Int 系列 ----

    bool PropertyInt(const char* label, int& value, float delta, int min, int max)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = UI::DragInt(GenerateID(), &value, delta, min, max);

        PropertyValueEnd();
        return modified;
    }

    // ---- Color 系列 ----

    bool PropertyColor(const char* label, glm::vec3& value)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = UI::ColorEdit3(GenerateID(), glm::value_ptr(value));

        PropertyValueEnd();
        return modified;
    }

    bool PropertyColor4(const char* label, glm::vec4& value)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = UI::ColorEdit4(GenerateID(), glm::value_ptr(value));

        PropertyValueEnd();
        return modified;
    }

    // ---- String 系列 ----

    bool PropertyString(const char* label, char* value, size_t bufSize)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = UI::InputText(GenerateID(), value, bufSize);

        PropertyValueEnd();
        return modified;
    }

    void PropertyReadOnlyString(const char* label, const char* value)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        ScopedDisable disabled(true);
        // 使用 const_cast 是安全的，因为 ReadOnly 标志阻止了修改
        UI::InputText(GenerateID(), const_cast<char*>(value), strlen(value) + 1, ImGuiInputTextFlags_ReadOnly);

        PropertyValueEnd();
    }

    // ---- Combo 下拉框 ----

    bool PropertyCombo(const char* label, int& selected, const char* const* options, int count)
    {
        bool modified = false;

        PropertyLabel(label);
        PropertyValueBegin();

        // BeginCombo 返回的是"是否打开"，不是"是否修改"
        // 需要手动跟踪选中项的变化
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
                        modified = true;
                    }
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        DrawItemActivityOutline();

        PropertyValueEnd();
        return modified;
    }

    // ---- Checkbox ----

    bool PropertyCheckbox(const char* label, bool& value)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = UI::Checkbox(GenerateID(), &value);

        PropertyValueEnd();
        return modified;
    }

    // ---- Texture ----

    bool PropertyTexture(const char* label, Ref<Texture2D>& texture)
    {
        bool modified = false;

        PropertyLabel(label);

        ImGui::TableSetColumnIndex(1);

        // 获取纹理 ID（如果纹理为空，使用默认黑色纹理）
        Ref<Texture2D> displayTexture = texture;
        if (!displayTexture)
        {
            displayTexture = Renderer3D::GetDefaultTexture(TextureDefault::Black);
        }

        uint32_t textureRendererID = displayTexture->GetRendererID();
        float previewSize = Theme::Layout::TexturePreviewSize;

        // 纹理预览按钮
        if (ImGui::ImageButton(
            GenerateID(),
            reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(textureRendererID)),
            ImVec2(previewSize, previewSize),
            ImVec2(0, 1),   // UV 左下
            ImVec2(1, 0)))  // UV 右上（OpenGL Y 翻转）
        {
            // 点击打开文件对话框选择纹理
            std::string filepath = FileDialogs::OpenFile("Texture(*.png;*.jpg;*.tga;*.bmp)\0*.png;*.jpg;*.tga;*.bmp\0");
            if (!filepath.empty())
            {
                texture = Texture2D::Create(filepath);
                modified = true;
            }
        }

        // TODO: 拖拽接收（未来资产浏览器实现后启用）
        // if (ImGui::BeginDragDropTarget())
        // {
        //     if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_TEXTURE"))
        //     {
        //         // 从 payload 中获取纹理路径，创建纹理
        //     }
        //     ImGui::EndDragDropTarget();
        // }

        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        Draw::Underline();
        return modified;
    }
}
```

---

## 五、核心设计决策详解

### 5.1 PropertyGrid 使用 Table vs Columns

| 特性 | `ImGui::Table` (方案 A) | `ImGui::Columns` (方案 B) |
|------|:---:|:---:|
| API 状态 | ? 当前推荐 | ?? Legacy（旧版） |
| 嵌套支持 | ? 完美支持 | ? 已知 bug |
| 用户可拖拽调整列宽 | ? `ImGuiTableFlags_Resizable` | ? 不支持 |
| 列宽自动响应面板宽度 | ? `SizingStretchProp` | ? 固定比例 |
| 实现复杂度 | 中等 | 简单 |
| Hazel 采用 | ? | ? |

**结论**：采用 `ImGui::Table`，虽然 Hazel 用的是 `Columns`，但 `Table` 是更好的选择。

### 5.2 Property 控件的返回值语义

**统一规则**：所有 Property 函数返回 `bool modified`，表示"值是否被用户修改"。

| 控件 | ImGui 原生返回值含义 | Property 封装后返回值含义 |
|------|------|------|
| `DragFloat` | 值被修改 | 值被修改 ? 一致 |
| `ColorEdit3` | 值被修改 | 值被修改 ? 一致 |
| `InputText` | 值被修改 | 值被修改 ? 一致 |
| `BeginCombo` | 下拉框是否打开 | **选中项是否被修改** ?? 语义转换 |
| `Checkbox` | 值被修改 | 值被修改 ? 一致 |
| `ImageButton` | 按钮是否被点击 | **纹理是否被修改** ?? 语义转换 |

`PropertyCombo` 和 `PropertyTexture` 内部处理了语义转换，调用者无需关心底层差异。

### 5.3 PropertyTexture 的 ImageButton ID 问题

**问题**：ImGui 的 `ImageButton` 在新版本中需要一个字符串 ID 作为第一个参数（`ImGui::ImageButton(const char* str_id, ...)`），旧版本使用纹理 ID 作为隐式 ID。

**解决方案**：使用 `GenerateID()` 生成唯一 ID，避免冲突。

### 5.4 Undo/Redo 预留点

每个 Property 控件内部都有明确的"修改前"和"修改后"时间点，未来集成 Undo/Redo 时只需在这些点插入记录逻辑：

```cpp
bool PropertyFloat(const char* label, float& value, float delta, float min, float max)
{
    // float oldValue = value;  // TODO: Undo/Redo 记录修改前的值
    
    PropertyLabel(label);
    PropertyValueBegin();
    bool modified = UI::DragFloat(GenerateID(), &value, delta, min, max);
    PropertyValueEnd();
    
    // if (modified && UndoSystem::IsRecording())
    // {
    //     UndoSystem::Record(label, oldValue, value);
    // }
    
    return modified;
}
```

---

## 六、迁移指南：InspectorPanel 迁移示例

### 6.1 Transform 组件

**迁移前：**

```cpp
DrawComponent<TransformComponent>("Transform", entity, [](TransformComponent& transform)
{
    ImGui::DragFloat3("Position", &transform.Translation.x, 0.01f);
    
    glm::vec3 rotationEuler = glm::degrees(transform.GetRotationEuler());
    if (ImGui::DragFloat3("Rotation", &rotationEuler.x, 1.0f))
    {
        transform.SetRotationEuler(glm::radians(rotationEuler));
    }

    ImGui::DragFloat3("Scale", &transform.Scale.x, 0.01f);
});
```

**迁移后：**

```cpp
DrawComponent<TransformComponent>("Transform", entity, [](TransformComponent& transform)
{
    UI::BeginPropertyGrid();
    
    UI::PropertyFloat3("Position", transform.Translation, 0.01f);
    
    glm::vec3 rotationEuler = glm::degrees(transform.GetRotationEuler());
    if (UI::PropertyFloat3("Rotation", rotationEuler, 1.0f))
    {
        transform.SetRotationEuler(glm::radians(rotationEuler));
    }

    UI::PropertyFloat3("Scale", transform.Scale, 0.01f);
    
    UI::EndPropertyGrid();
});
```

### 6.2 Light 组件

**迁移前：**

```cpp
const char* lightTypes[] = { "Directional", "Point", "Spot" };
int currentType = static_cast<int>(light.Type);
if (ImGui::Combo("Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
{
    LightType newType = static_cast<LightType>(currentType);
    if (newType != light.Type)
    {
        light.Type = newType;
        if (newType == LightType::Directional)
            light.Shadows = ShadowType::Hard;
        else
            light.Shadows = ShadowType::None;
    }
}
ImGui::Separator();
ImGui::ColorEdit3("Color", glm::value_ptr(light.Color));
ImGui::DragFloat("Intensity", &light.Intensity, 0.01f, 0.0f, 100.0f);
```

**迁移后：**

```cpp
UI::BeginPropertyGrid();

const char* lightTypes[] = { "Directional", "Point", "Spot" };
int currentType = static_cast<int>(light.Type);
if (UI::PropertyCombo("Type", currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
{
    LightType newType = static_cast<LightType>(currentType);
    if (newType != light.Type)
    {
        light.Type = newType;
        light.Shadows = (newType == LightType::Directional) ? ShadowType::Hard : ShadowType::None;
    }
}

UI::PropertyColor("Color", light.Color);
UI::PropertyFloat("Intensity", light.Intensity, 0.01f, 0.0f, 100.0f);

UI::EndPropertyGrid();
```

### 6.3 材质编辑器中的纹理属性

**迁移前（约 20 行）：**

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

**迁移后（约 5 行）：**

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

### 6.4 PostProcessVolume 组件

**迁移前：**

```cpp
ImGui::Checkbox("Is Global", &volume.IsGlobal);
ImGui::DragFloat("Priority", &volume.Priority, 0.1f);
ImGui::Separator();
if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
{
    ImGui::Checkbox("Bloom Enabled", &volume.BloomEnabled);
    if (volume.BloomEnabled)
    {
        ImGui::DragFloat("Threshold", &volume.BloomThreshold, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Bloom Intensity", &volume.BloomIntensity, 0.01f, 0.0f, 10.0f);
        ImGui::DragInt("Iterations", &volume.BloomIterations, 1, 1, 10);
    }
}
```

**迁移后：**

```cpp
UI::BeginPropertyGrid();
UI::PropertyCheckbox("Is Global", volume.IsGlobal);
UI::PropertyFloat("Priority", volume.Priority, 0.1f);
UI::EndPropertyGrid();

ImGui::Separator();

if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
{
    UI::BeginPropertyGrid();
    UI::PropertyCheckbox("Bloom Enabled", volume.BloomEnabled);
    if (volume.BloomEnabled)
    {
        UI::PropertyFloat("Threshold", volume.BloomThreshold, 0.01f, 0.0f, 10.0f);
        UI::PropertyFloat("Bloom Intensity", volume.BloomIntensity, 0.01f, 0.0f, 10.0f);
        UI::PropertyInt("Iterations", volume.BloomIterations, 1, 1, 10);
    }
    UI::EndPropertyGrid();
}
```

> **注意**：`CollapsingHeader` 和 `Separator` 不在 PropertyGrid 内部，因为它们不是 Label+控件格式。每个 CollapsingHeader 内部可以有自己的 `BeginPropertyGrid`/`EndPropertyGrid`。

### 6.5 特殊需求降级到 Layer 2

当 Property 控件的精简参数无法满足需求时，降级到 Layer 2：

```cpp
// Shadow Bias 需要 "%.4f" 格式 → 降级到 Layer 2
// 在 PropertyGrid 内部手动绘制
ImGui::TableNextRow();
ImGui::TableSetColumnIndex(0);
ImGui::AlignTextToFramePadding();
UI::ShiftCursor(Theme::Layout::PropertyLabelOffsetX, 0.0f);
ImGui::TextUnformatted("Shadow Bias");
ImGui::TableSetColumnIndex(1);
UI::ShiftCursorY(Theme::Layout::PropertyValueOffsetY);
ImGui::PushItemWidth(-1);
UI::DragFloat(UI::GenerateID(), &light.ShadowBias, 0.0001f, 0.0f, 0.05f, "%.4f");
ImGui::PopItemWidth();
Draw::Underline();
```

或者，可以为 `PropertyFloat` 添加一个带完整参数的重载（可选方案）：

```cpp
/// <summary>
/// 浮点数属性（完整参数版本，支持自定义格式和标志）
/// </summary>
bool PropertyFloat(const char* label, float& value, float delta, float min, float max,
                   const char* format, ImGuiSliderFlags flags = 0);
```

**推荐**：先不添加完整参数重载，等实际遇到多个需要完整参数的场景时再添加。当前只有 `ShadowBias` 一处需要 `"%.4f"` 格式。

---

## 七、总结

| 文件 | 行数估算 | 依赖 |
|------|:---:|------|
| `Controls.h` | ~120 | `DrawUtils.h`, `imgui.h`, `imgui_stdlib.h` |
| `PropertyGrid.h` | ~120 | `UICore.h`, `Controls.h`, `DrawUtils.h`, `Theme.h`, `Base.h`, `Texture.h`, `glm` |
| `PropertyGrid.cpp` | ~250 | `lcpch.h`, `PropertyGrid.h`, `Renderer3D.h`, `PlatformUtils.h` |
| **合计** | **~490** | |

**实施顺序**：
1. 确保 Layer 1 基础设施已实现
2. 实现 `Controls.h`（纯头文件）
3. 实现 `PropertyGrid.h` / `PropertyGrid.cpp`
4. 在 InspectorPanel 中小范围验证（先迁移 Transform 组件）
5. 逐步迁移其他组件
