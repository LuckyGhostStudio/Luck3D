#include "lcpch.h"
#include "DragDropVisuals.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Lucky::UI
{
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

        // 与 DrawItemActivityOutline 悬停态一致的灰白色高亮，保持视觉风格统一
        drawList->AddRect(rect.Min, rect.Max, IM_COL32(200, 200, 200, 255), rounding, 0, 1.5f);
    }
}
