#include "lcpch.h"
#include "DragDropVisuals.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Lucky::UI
{
    namespace
    {
        // 通用高亮色（AssetField 矩形、Inside 节点框）：与项目悬停态视觉一致的灰白
        constexpr ImU32 s_HighlightColor = IM_COL32(200, 200, 200, 255);
        // Insert 线主色：DodgerBlue，饱和度高、亮度略低，与"选中态节点蓝背景"能拉开明显区分
        constexpr ImU32 s_InsertLineColor = IM_COL32(30, 144, 255, 255);
        constexpr float s_HighlightThickness = 1.5f;
        // Insert 线专用线宽：略粗于普通高亮，提升识别度
        constexpr float s_InsertLineThickness = 2.0f;
        // 插入线左端点小圆圈的半径
        constexpr float s_InsertDotRadius = 3.5f;
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

    void DragDropVisuals::HighlightTargetInsertLine(float x, bool above)
    {
        ImVec2 itemMin = ImGui::GetItemRectMin();
        ImVec2 itemMax = ImGui::GetItemRectMax();

        // y 位置：当前 item 顶部（above=true，Before）或底部（above=false，After）
        float y = above ? itemMin.y : itemMax.y;

        HighlightTargetInsertLineAt(x, y);
    }

    void DragDropVisuals::HighlightTargetInsertLineAt(float x, float y)
    {
        auto* drawList = ImGui::GetWindowDrawList();

        // 右端点延伸到当前窗口内容区右边界（考虑 padding，避免贴住滚动条）
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        float rightX = window->WorkRect.Max.x;

        // 左端点空心小圆圈：指示插入的"层级锚点"
        drawList->AddCircle(
            ImVec2(x, y),
            s_InsertDotRadius,
            s_InsertLineColor,
            0,
            s_InsertLineThickness);

        // 圆圈右侧起延伸的水平直线（起点略微偏右，避免与圆圈重叠）
        float lineStartX = x + s_InsertDotRadius;
        if (lineStartX < rightX)
        {
            drawList->AddLine(
                ImVec2(lineStartX, y),
                ImVec2(rightX, y),
                s_InsertLineColor,
                s_InsertLineThickness);
        }
    }

    void DragDropVisuals::HighlightTargetInsertDotAt(float x, float y)
    {
        // 仅绘制孤立小圆圈（无水平线段）
        // 视觉参数与 HighlightTargetInsertLineAt 的左端圆圈完全一致，
        // 用于 After 模式 X 层级回退时，在被跨越的祖先节点底部提示"更深层级仍可选"
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddCircle(
            ImVec2(x, y),
            s_InsertDotRadius,
            s_InsertLineColor,
            0,
            s_InsertLineThickness);
    }

    void DragDropVisuals::HighlightTargetNode(float rounding)
    {
        ImVec2 itemMin = ImGui::GetItemRectMin();
        ImVec2 itemMax = ImGui::GetItemRectMax();

        HighlightTargetNodeAt(itemMin.x, itemMin.y, itemMax.x, itemMax.y, rounding);
    }

    void DragDropVisuals::HighlightTargetNodeAt(float minX, float minY, float maxX, float maxY, float rounding)
    {
        ImGuiContext& g = *GImGui;

        if (rounding == 0.0f)
        {
            rounding = g.Style.FrameRounding;
        }

        auto* drawList = ImGui::GetWindowDrawList();

        // Inside 模式：整节点高亮框
        // - 颜色使用与 Insert 线一致的蓝色（s_InsertLineColor），保持拖拽视觉语义统一
        // - 线宽略粗于普通高亮（s_InsertLineThickness），提升识别度
        // - 描边的"中心线"落在 [min, max] 上，线宽 2px 意味着右/底边线的右/下半宽（1px）
        //   会越过 max，超出 WorkRect 右边界。为让描边线整体落在传入矩形内部（视觉外沿 ==
        //   目标 header 外沿），将矩形向内收缩半个线宽
        // - 视觉调整：整体再向左平移半个线宽，让高亮框在水平方向上更贴合 Hierarchy 左边界
        float halfLine = s_InsertLineThickness * 0.5f;
        drawList->AddRect(
            ImVec2(minX,                minY + halfLine),
            ImVec2(maxX - halfLine * 2, maxY - halfLine),
            s_InsertLineColor,
            rounding,
            0,
            s_InsertLineThickness);
    }
}
