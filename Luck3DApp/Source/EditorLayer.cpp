#include "EditorLayer.h"

#include <imgui/imgui.h>

#include "Panels/ExamplePanel.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Lucky
{
#define SCENE_EXAMPLE_PANEL_ID "ExamplePanel"

    EditorLayer::EditorLayer()
        : Layer("EditorLayer"),
        m_EditorCamera(30.0f, 1280.0f / 720.0f, 0.01f, 1000.0f)
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

    void EditorLayer::OnUpdate(DeltaTime dt)
    {
        if (m_Size.x > 0.0f && m_Size.y > 0.0f)
        {
            m_EditorCamera.SetViewportSize(m_Size.x, m_Size.y); // 重置编辑器相机视口大小
        }

        m_EditorCamera.OnUpdate(dt);    // 更新编辑器相机

        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
        RenderCommand::Clear();

        Renderer3D::BeginScene(m_EditorCamera);
        {
            glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };  // 位置
            glm::vec3 m_Scale = { 1.0f, 1.0f, 1.0f };     // 缩放

            glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position) * glm::scale(glm::mat4(1.0f), m_Scale);

            Renderer3D::DrawMesh(transform, m_SquareColor);
        }
        Renderer3D::EndScene();
    }

    void EditorLayer::OnImGuiRender()
    {
        //// 渲染 DockSpace
        //m_EditorDockSpace.ImGuiRender();

        //UI_DrawMenuBar();

        //m_PanelManager->OnImGuiRender();

        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();  // 当前面板大小
        m_Size = { viewportPanelSize.x, viewportPanelSize.y };      // 视口大小
    }

    void EditorLayer::OnEvent(Event& event)
    {
        m_PanelManager->OnEvent(event);

        m_EditorCamera.OnEvent(event);
    }

    void EditorLayer::UI_DrawMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            // File
            if (ImGui::BeginMenu("File"))
            {
                // 退出
                if (ImGui::MenuItem("Quit", "Ctrl Q"))
                {
                    Application::GetInstance().Close();    // 退出程序
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
