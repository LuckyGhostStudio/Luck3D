#include "lcpch.h"
#include "DrawUtils.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Lucky::UI
{
    namespace Draw
    {
        void Underline(bool fullWidth, float lineWidth, float offsetX, float offsetY)
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
                lineWidth
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
        
        void ItemTopShadow()
        {
            auto* drawList = ImGui::GetWindowDrawList();
            ImRect rect = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };
            
            // 上边框深色阴影线
            ImRect shadowLineRect = {
                { rect.Min.x + 4.0f, rect.Min.y + 0.5f },
                { rect.Max.x - 4.0f, rect.Min.y + 0.5f }
            };
            drawList->AddLine(shadowLineRect.Min, shadowLineRect.Max, IM_COL32(0, 0, 0, 255), 2.0f);
        }
        
        void ItemBottomShadow()
        {
            auto* drawList = ImGui::GetWindowDrawList();
            ImRect rect = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };
            
            // 下边框深色阴影线
            ImRect shadowLineRect = {
                { rect.Min.x + 4.0f, rect.Max.y - 0.5f },
                { rect.Max.x - 4.0f, rect.Max.y - 0.5f }
            };
            drawList->AddLine(shadowLineRect.Min, shadowLineRect.Max, IM_COL32(0, 0, 0, 255), 2.0f);
        }
        
        void HorizontalLine(float alpha, float offsetX, float offsetY)
        {
            const float width = ImGui::GetContentRegionAvail().x;
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            
            auto* drawList = ImGui::GetWindowDrawList();
            ImRect rect = {
                ImVec2(cursor.x + offsetX, cursor.y + offsetY),
                ImVec2(cursor.x + width, cursor.y + offsetY)
            };
            
            drawList->AddRect(rect.Min, rect.Max, IM_COL32(25, 25, 25, 255 * alpha), 0.0f, 0);
        }
        
        void VerticalLine(float alpha, float offsetX, float offsetY)
        {
            const float height = ImGui::GetContentRegionAvail().y;
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            
            auto* drawList = ImGui::GetWindowDrawList();
            ImRect rect = {
                ImVec2(cursor.x + offsetX, cursor.y),
                ImVec2(cursor.x, cursor.y + height)
            };
            
            drawList->AddRect(rect.Min, rect.Max, IM_COL32(25, 25, 25, 255 * alpha), 0.0f, 0);
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
            // 激活状态：蓝色高亮边框
            drawList->AddRect(rect.Min, rect.Max, IM_COL32(62, 170, 255, 255), rounding, 0, 1.5f);
        }
        else if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            // 悬停状态：灰白高亮边框
            drawList->AddRect(rect.Min, rect.Max, IM_COL32(200, 200, 200, 255), rounding, 0, 1.5f);
        }
    }
}