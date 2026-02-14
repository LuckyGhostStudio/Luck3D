#include "EditorLayer.h"

#include <imgui/imgui.h>

#include "Panels/ExamplePanel.h"

namespace Lucky
{
#define SCENE_EXAMPLE_PANEL_ID "ExamplePanel"

    EditorLayer::EditorLayer()
        : Layer("EditorLayer")
    {

    }

    void EditorLayer::OnAttach()
    {
        LF_TRACE("EditorLayer::OnAttach");

        m_PanelManager = CreateScope<PanelManager>();

        m_PanelManager->AddPanel<ExamplePanel>(SCENE_EXAMPLE_PANEL_ID, "Example", true);
    }

    void EditorLayer::OnDetach()
    {
        LF_TRACE("EditorLayer::OnDetach");
    }

    void EditorLayer::OnUpdate()
    {
        
    }

    void EditorLayer::OnImGuiRender()
    {
        // äÖÈ¾ DockSpace
        m_EditorDockSpace.ImGuiRender();

        UI_DrawMenuBar();

        m_PanelManager->OnImGuiRender();

        ImGui::ShowDemoWindow();
    }

    void EditorLayer::OnEvent(Event& event)
    {
        m_PanelManager->OnEvent(event);
    }

    void EditorLayer::UI_DrawMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            // File
            if (ImGui::BeginMenu("File"))
            {
                // ÍË³ö
                if (ImGui::MenuItem("Quit", "Ctrl Q"))
                {
                    Application::GetInstance().Close();    // ÍË³ö³ÌÐò
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                if (ImGui::MenuItem("Example"))
                {
                    uint32_t panelID = Hash::GenerateFNVHash(SCENE_EXAMPLE_PANEL_ID);
                    PanelData* panelData = m_PanelManager->GetPanelData(panelID);
                    panelData->IsOpen = true;
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("About Luck3D"))
                {

                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }
}
