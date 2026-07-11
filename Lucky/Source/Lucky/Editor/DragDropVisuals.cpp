#include "lcpch.h"
#include "DragDropVisuals.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Lucky::UI
{
    namespace
    {
        // 与 DrawItemActivityOutline 悬停态一致的灰白色高亮，保持项目内视觉风格统一
        constexpr ImU32 s_HighlightColor = IM_COL32(200, 200, 200, 255);
        constexpr float s_HighlightThickness = 1.5f;
    }

    void DragDropVisuals::HighlightTargetRect(float rounding)
    {
        ImGuiContext& g = *GImGui;

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

        drawList->AddRect(rect.Min, rect.Max, s_HighlightColor, rounding, 0, s_HighlightThickness);
    }

    void DragDropVisuals::HighlightTargetInsertLine(bool above)
    {
        auto* drawList = ImGui::GetWindowDrawList();
        ImVec2 itemMin = ImGui::GetItemRectMin();
        ImVec2 itemMax = ImGui::GetItemRectMax();

        // 在当前 item 顶部（above=true，Before）或底部（above=false，After）绘制一条水平线
        float y = above ? itemMin.y : itemMax.y;
        drawList->AddLine(
            ImVec2(itemMin.x, y),
            ImVec2(itemMax.x, y),
            s_HighlightColor,
            s_HighlightThickness);
    }

    void DragDropVisuals::HighlightTargetNode(float rounding)
    {
        ImGuiContext& g = *GImGui;

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

        // Inside 模式：整节点高亮框，与 HighlightTargetRect 视觉一致
        drawList->AddRect(rect.Min, rect.Max, s_HighlightColor, rounding, 0, s_HighlightThickness);
    }
}
