#include "EditorLayoutManager.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Lucky
{
    void EditorLayoutManager::ApplyDefaultLayout(uint32_t dockspaceID)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        // 清除旧布局
        ImGui::DockBuilderRemoveNode(dockspaceID);

        // 创建新的 DockSpace 节点
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->Size);

        //  ┌──────────────┬──────────────────────┬──────────────┐
        //  │              │                      │  Inspector   │
        //  │  Hierarchy   │    Scene (Viewport)  │     |        │
        //  │    (30%)     │       (40%)          │  Render      │
        //  │              │                      │  Pipeline    │
        //  │              │                      │    (30%)     │
        //  ├──────────────┴──────────────────────┤              │
        //  │                                     │              │
        //  │          Project                    │              │
        //  │           (40%)                     │              │
        //  └─────────────────────────────────────┴──────────────┘

        ImGuiID dockMain = dockspaceID;

        // Step 1: 右侧分割出 Inspector（30%）
        ImGuiID dockRight;
        ImGuiID dockLeft;
        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.3f, &dockRight, &dockLeft);

        // Step 2: 左侧区域底部分割出 Project（40% 高度）
        ImGuiID dockBottom;
        ImGuiID dockTop;
        ImGui::DockBuilderSplitNode(dockLeft, ImGuiDir_Down, 0.4f, &dockBottom, &dockTop);

        // Step 3: 上方区域左侧分割出 Hierarchy（30%/(30%+40%) ≈ 0.43）
        ImGuiID dockHierarchy;
        ImGuiID dockViewport;
        ImGui::DockBuilderSplitNode(dockTop, ImGuiDir_Left, 0.43f, &dockHierarchy, &dockViewport);

        // 将窗口停靠到对应节点
        ImGui::DockBuilderDockWindow("Hierarchy", dockHierarchy);
        ImGui::DockBuilderDockWindow("Scene", dockViewport);
        ImGui::DockBuilderDockWindow("Project", dockBottom);
        ImGui::DockBuilderDockWindow("Render Pipeline", dockRight); // 与 Inspector 共享 Tab
        ImGui::DockBuilderDockWindow("Inspector", dockRight);

        // 完成布局构建
        ImGui::DockBuilderFinish(dockspaceID);
    }
}
