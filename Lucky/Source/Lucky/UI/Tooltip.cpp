#include "lcpch.h"
#include "Tooltip.h"

#include "Lucky/Editor/EditorPreferences.h"
#include "Lucky/UI/Theme.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <cstdio>

namespace Lucky::UI
{
    static ImVec4 ToImVec4(const glm::vec4& v)
    {
        return ImVec4(v.r, v.g, v.b, v.a);
    }

    /// <summary>
    /// 真正画 Tooltip 的内部函数。调用前已保证通过了 Hover + 延迟判定。
    /// </summary>
    static void DrawTooltip(const TooltipStyle& style, const char* text)
    {
        // 位置：固定在 Hovered Item 正下方，左对齐 Item 左边界，不跟随鼠标
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();
        ImGui::SetNextWindowPos(ImVec2(itemMin.x, itemMax.y + Theme::Layout::TooltipItemGap));

        const ColorSettings& c = EditorPreferences::Get().GetColors();

        // Push 独立样式（不继承父面板的 WindowPadding = 0 等设置）
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::Layout::TooltipPaddingX, Theme::Layout::TooltipPaddingY));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, style.Rounding);
        // 关键：Tooltip 窗口在 ImGui::Begin 里读的是 PopupBorderSize（Popup/Tooltip 类型统一走这个），
        // 不是 WindowBorderSize。Push 错变量会完全无效。
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, Theme::Layout::TooltipBorderSize);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ToImVec4(c.TooltipBgColor));
        ImGui::PushStyleColor(ImGuiCol_Border, ToImVec4(c.TooltipBorderColor));

        ImGui::BeginTooltip();
        ImGui::TextUnformatted(text);
        ImGui::EndTooltip();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }

    void SetItemTooltipExV(const TooltipStyle& style, const char* fmt, va_list args)
    {
        if (!ImGui::IsItemHovered())
        {
            return;
        }

        // ImGui 1.87 没有 ImGuiHoveredFlags_DelayNormal，使用 context 的 HoveredIdTimer 做延迟判定。
        // HoveredIdTimer 是当前 HoveredId 持续被 Hover 的累计时间（秒）；Hover 换 Item 自动重置。
        ImGuiContext& g = *ImGui::GetCurrentContext();
        if (g.HoveredIdTimer < style.DelaySeconds)
        {
            return;
        }

        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        DrawTooltip(style, buf);
    }

    void SetItemTooltipV(const char* fmt, va_list args)
    {
        TooltipStyle style;
        SetItemTooltipExV(style, fmt, args);
    }

    void SetItemTooltip(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        SetItemTooltipV(fmt, args);
        va_end(args);
    }

    void SetItemTooltipEx(const TooltipStyle& style, const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        SetItemTooltipExV(style, fmt, args);
        va_end(args);
    }
}
