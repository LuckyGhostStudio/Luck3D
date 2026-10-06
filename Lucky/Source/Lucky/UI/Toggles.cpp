#include "lcpch.h"
#include "Toggles.h"

#include "Lucky/Editor/EditorPreferences.h"
#include "Lucky/Renderer/Texture.h"
#include "Lucky/UI/Theme.h"
#include "Lucky/UI/Widgets.h"

#include <imgui/imgui.h>

#include <vector>

namespace Lucky::UI
{
    // ========================================================================
    // 默认样式
    // ========================================================================

    /// <summary>
    /// 把 glm::vec4 转成 ImVec4（EditorPreferences 的颜色字段用 glm::vec4 存储）
    /// </summary>
    static ImVec4 ToImVec4(const glm::vec4& v)
    {
        return ImVec4(v.r, v.g, v.b, v.a);
    }

    /// <summary>
    /// 构造"通用 Toggle 样式"（EditorToolbar 的 Play/Pause 等用）。
    /// 规则：未选中走 ToggleBg* 三色；选中态三色全部等于 SelectionBlueColor
    /// （选中后鼠标悬停 / 按下不再变色，Unity 一致行为）。
    /// 每次调用都重新从 EditorPreferences 读，支持运行时改配色下一帧生效。
    /// </summary>
    static ToggleStyle GetDefaultToggleStyle()
    {
        const ColorSettings& c = EditorPreferences::Get().GetColors();
        const ImVec4 selBlue = ToImVec4(c.SelectionBlueColor);
        return ToggleStyle{
            ToImVec4(c.ToggleBgNormal),
            ToImVec4(c.ToggleBgHovered),
            ToImVec4(c.ToggleBgActive),
            selBlue,
            selBlue,
            selBlue,
        };
    }

    ToggleStyle MakeToolbarToggleStyle()
    {
        const ColorSettings& c = EditorPreferences::Get().GetColors();
        const ImVec4 selBlue = ToImVec4(c.SelectionBlueColor);
        return ToggleStyle{
            ToImVec4(c.ToolbarToggleBgNormal),      // 未选中：灰
            ToImVec4(c.ToolbarToggleBgHovered),     // 未选中：悬浮稍亮
            selBlue,                                // 未选中：按下用选中蓝（即将选中的预览反馈）
            selBlue,                                // 选中：三态全蓝
            selBlue,
            selBlue,
        };
    }

    static RadioGroupStyle GetDefaultRadioGroupStyle()
    {
        RadioGroupStyle style;
        style.ItemSpacing = 2.0f;
        style.Connected = false;
        style.Items = GetDefaultToggleStyle();
        return style;
    }

    /// <summary>
    /// 把 ToggleStyle 的 6 个背景色 Push 到 ImGui 栈顶（配合 ImGui::Button / ImageButton 使用）。
    /// 调用方 Pop 3 个 Color 即可（Button / ButtonHovered / ButtonActive）。
    /// </summary>
    static void PushToggleButtonColors(bool selected, const ToggleStyle& style)
    {
        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, style.BgSelected);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.BgSelectedHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.BgSelectedActive);
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, style.BgNormal);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.BgHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.BgActive);
        }
    }

    /// <summary>
    /// 根据 value / hovered / active 从 ToggleStyle 挑一个背景色（ToggleIconTextButton 自绘用）
    /// </summary>
    static ImVec4 PickToggleBgColor(bool selected, bool hovered, bool active, const ToggleStyle& style)
    {
        if (selected)
        {
            if (active) { return style.BgSelectedActive; }
            if (hovered) { return style.BgSelectedHovered; }
            return style.BgSelected;
        }
        if (active) { return style.BgActive; }
        if (hovered) { return style.BgHovered; }
        return style.BgNormal;
    }

    // ========================================================================
    // 第 1 层：Toggle 原语
    // ========================================================================

    bool ToggleIconButton(const char* strID, const Ref<Texture2D>& icon, bool& value, const ImVec2& size, const char* tooltip, const ToggleStyle* style)
    {
        ToggleStyle s = style ? *style : GetDefaultToggleStyle();

        ImGui::PushID(strID);

        ImVec2 cursorStart = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton("##btn", size);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // 画按钮背景矩形（根据 selected/hovered/active 挑色）
        ImVec4 bgColor = PickToggleBgColor(value, hovered, active, s);
        ImVec2 rectMax = ImVec2(cursorStart.x + size.x, cursorStart.y + size.y);
        drawList->AddRectFilled(cursorStart, rectMax, ImGui::ColorConvertFloat4ToU32(bgColor), Theme::Layout::FrameRounding);

        // 画图标：直接撑满按钮区，和原生 ImageButton + framePadding=0 行为一致。
        // 图标 PNG 的比例由调用方的 size 匹配（如 Play.png 57x32 对应 50x28 按钮），避免变形。
        if (icon)
        {
            ImTextureID texID = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(icon->GetRendererID()));
            // UV 翻转：OpenGL 纹理原点在左下
            drawList->AddImage(texID, cursorStart, rectMax, ImVec2(0, 1), ImVec2(1, 0), IM_COL32_WHITE);
        }

        if (tooltip && hovered)
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

    bool ToggleTextButton(const char* label, bool& value, const ImVec2& size, const char* tooltip, const ToggleStyle* style)
    {
        ToggleStyle s = style ? *style : GetDefaultToggleStyle();

        PushToggleButtonColors(value, s);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

        bool clicked = ImGui::Button(label, size);

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (tooltip && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", tooltip);
        }

        if (clicked)
        {
            value = !value;
        }
        return clicked;
    }

    bool ToggleIconTextButton(const char* strID, const Ref<Texture2D>& icon, const char* label, bool& value, const ImVec2& size, const char* tooltip, const ToggleStyle* style)
    {
        ToggleStyle s = style ? *style : GetDefaultToggleStyle();

        constexpr float iconToTextGap = 6.0f;
        constexpr float horizontalPadding = 8.0f;
        constexpr float verticalPadding = 4.0f;

        // 计算按钮尺寸：显式传入则用传入值；否则按 "图标 + 间隙 + 文本 + 两侧内边距" 自动测算
        const float textLineHeight = ImGui::GetTextLineHeight();
        const float iconSize = textLineHeight;      // 图标高度和一行文本一致
        const ImVec2 textSize = ImGui::CalcTextSize(label);

        ImVec2 realSize = size;
        if (realSize.x <= 0.0f)
        {
            realSize.x = horizontalPadding + iconSize + iconToTextGap + textSize.x + horizontalPadding;
        }
        if (realSize.y <= 0.0f)
        {
            realSize.y = textLineHeight + verticalPadding * 2.0f;
        }

        ImGui::PushID(strID);

        ImVec2 cursorStart = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton("##btn", realSize);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        // 绘制背景矩形
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec4 bgColor = PickToggleBgColor(value, hovered, active, s);
        ImVec2 rectMax = ImVec2(cursorStart.x + realSize.x, cursorStart.y + realSize.y);
        drawList->AddRectFilled(cursorStart, rectMax, ImGui::ColorConvertFloat4ToU32(bgColor), Theme::Layout::FrameRounding);

        // 绘制图标（左侧，垂直居中）
        if (icon)
        {
            ImTextureID texID = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(icon->GetRendererID()));
            float iconY = cursorStart.y + (realSize.y - iconSize) * 0.5f;
            ImVec2 iconMin = ImVec2(cursorStart.x + horizontalPadding, iconY);
            ImVec2 iconMax = ImVec2(iconMin.x + iconSize, iconMin.y + iconSize);
            // UV 翻转：OpenGL 纹理原点在左下
            drawList->AddImage(texID, iconMin, iconMax, ImVec2(0, 1), ImVec2(1, 0), IM_COL32_WHITE);
        }

        // 绘制文本（图标右侧，垂直居中）
        float textY = cursorStart.y + (realSize.y - textLineHeight) * 0.5f;
        ImVec2 textPos = ImVec2(cursorStart.x + horizontalPadding + iconSize + iconToTextGap, textY);
        drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), label);

        if (tooltip && hovered)
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

    // ========================================================================
    // 第 2 层：RadioGroup
    // ========================================================================

    namespace
    {
        struct RadioGroupContext
        {
            int* SelectedIndex = nullptr;       // 绑定的外部变量
            RadioGroupStyle Style;              // 本组样式
            int ItemCount = 0;                  // 已画 Item 数（判断首个 vs 后续，决定是否 SameLine）
        };

        static std::vector<RadioGroupContext> s_RadioGroupStack;
    }

    /// <summary>
    /// 画一个 Item 前的通用处理：首个 Item 不 SameLine，后续 Item 按 ItemSpacing SameLine。
    /// 返回当前组上下文的引用。
    /// </summary>
    static RadioGroupContext& PrepareRadioItem()
    {
        LF_CORE_ASSERT(!s_RadioGroupStack.empty(), "RadioXxxItem called outside BeginRadioGroup");
        RadioGroupContext& ctx = s_RadioGroupStack.back();
        if (ctx.ItemCount > 0)
        {
            ImGui::SameLine(0.0f, ctx.Style.ItemSpacing);
        }
        return ctx;
    }

    void BeginRadioGroup(const char* strID, int& selectedIndex, const RadioGroupStyle* style)
    {
        RadioGroupContext ctx;
        ctx.SelectedIndex = &selectedIndex;
        ctx.Style = style ? *style : GetDefaultRadioGroupStyle();
        ctx.ItemCount = 0;
        s_RadioGroupStack.push_back(ctx);

        // 组内所有 Item 的 ID 都挂在这个子作用域下，避免跨组 ID 冲突
        ImGui::PushID(strID);
    }

    void EndRadioGroup()
    {
        LF_CORE_ASSERT(!s_RadioGroupStack.empty(), "EndRadioGroup without matching BeginRadioGroup");
        ImGui::PopID();
        s_RadioGroupStack.pop_back();
    }

    void RadioIconItem(int itemValue, const Ref<Texture2D>& icon, const ImVec2& size, const char* tooltip)
    {
        RadioGroupContext& ctx = PrepareRadioItem();
        bool isSelected = (*ctx.SelectedIndex == itemValue);

        // 用 itemValue 做 ID，确保组内多个 Item ID 不冲突
        char itemID[32];
        snprintf(itemID, sizeof(itemID), "##item_%d", itemValue);

        // Toggle 内部会把临时 displayValue 翻转，但下一帧又会从 isSelected 重新算，无副作用。
        // 组这里接管"写入 selectedIndex"的权力。
        bool displayValue = isSelected;
        bool clicked = ToggleIconButton(itemID, icon, displayValue, size, tooltip, &ctx.Style.Items);

        if (clicked && !isSelected)
        {
            *ctx.SelectedIndex = itemValue;
        }
        ++ctx.ItemCount;
    }

    void RadioTextItem(int itemValue, const char* label, const ImVec2& size, const char* tooltip)
    {
        RadioGroupContext& ctx = PrepareRadioItem();
        bool isSelected = (*ctx.SelectedIndex == itemValue);

        // 文本按钮用 label 做 ID，组内可能有多个相同 label（罕见），加 itemValue 后缀确保唯一
        char fullLabel[128];
        snprintf(fullLabel, sizeof(fullLabel), "%s##radio_%d", label, itemValue);

        bool displayValue = isSelected;
        bool clicked = ToggleTextButton(fullLabel, displayValue, size, tooltip, &ctx.Style.Items);

        if (clicked && !isSelected)
        {
            *ctx.SelectedIndex = itemValue;
        }
        ++ctx.ItemCount;
    }

    void RadioIconTextItem(int itemValue, const Ref<Texture2D>& icon, const char* label, const ImVec2& size, const char* tooltip)
    {
        RadioGroupContext& ctx = PrepareRadioItem();
        bool isSelected = (*ctx.SelectedIndex == itemValue);

        char itemID[32];
        snprintf(itemID, sizeof(itemID), "##item_%d", itemValue);

        bool displayValue = isSelected;
        bool clicked = ToggleIconTextButton(itemID, icon, label, displayValue, size, tooltip, &ctx.Style.Items);

        if (clicked && !isSelected)
        {
            *ctx.SelectedIndex = itemValue;
        }
        ++ctx.ItemCount;
    }

    void RadioIconGroup(const char* strID, int& selectedIndex, const RadioIconItemDesc* items, int count, const ImVec2& itemSize, const RadioGroupStyle* style)
    {
        BeginRadioGroup(strID, selectedIndex, style);
        for (int i = 0; i < count; ++i)
        {
            const RadioIconItemDesc& desc = items[i];
            RadioIconItem(desc.Value, desc.Icon ? *desc.Icon : Ref<Texture2D>(), itemSize, desc.Tooltip);
        }
        EndRadioGroup();
    }
}
