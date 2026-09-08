#include "EditorDockSpace.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "Lucky/Core/Log.h"

namespace Lucky
{
    EditorDockSpace::EditorDockSpace(bool fullScreen)
        : m_IsFullScreen(fullScreen),
        m_Flags(ImGuiDockNodeFlags_None),
        m_WindowFlags(ImGuiWindowFlags_NoDocking)
    {

    }

    void EditorDockSpace::ImGuiRender()
    {
        if (m_IsFullScreen)
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();

            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            m_WindowFlags |= ImGuiWindowFlags_NoTitleBar;
            m_WindowFlags |= ImGuiWindowFlags_NoCollapse;
            m_WindowFlags |= ImGuiWindowFlags_NoResize;
            m_WindowFlags |= ImGuiWindowFlags_NoMove;
            m_WindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
            m_WindowFlags |= ImGuiWindowFlags_NoNavFocus;
        }

        if (m_Flags)
        {
            m_WindowFlags |= ImGuiWindowFlags_NoBackground;
        }

        static ImVec4 barColor = ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg];
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, barColor);
        ImGui::PushStyleColor(ImGuiCol_Border, barColor);
        // DockSpace 窗口
        ImGui::Begin("DockSpace", nullptr, m_WindowFlags);
        {
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();

            if (m_IsFullScreen)
            {
                ImGui::PopStyleVar(2);
            }

            ImGuiIO& io = ImGui::GetIO();

            if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
            {
                ImGuiID dockspaceID = ImGui::GetID("EditorDockSpace");
                ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), m_Flags | ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton);

                // ---- 布局管理 ----

                // 首次启动检测：节点不存在或无子节点分割，说明没有已保存的布局
                ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceID);
                if (node == nullptr || !node->IsSplitNode())
                {
                    m_LayoutManager.ApplyDefaultLayout(dockspaceID);
                }

                // 请求重置布局
                if (m_ResetLayout)
                {
                    m_LayoutManager.ApplyDefaultLayout(dockspaceID);
                    m_ResetLayout = false;
                }
            }
        }
        ImGui::End();
    }

    void EditorDockSpace::ResetToDefaultLayout()
    {
        m_ResetLayout = true;
    }
}
