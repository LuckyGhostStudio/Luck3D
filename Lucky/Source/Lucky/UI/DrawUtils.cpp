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