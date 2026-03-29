#include "EditorLayer.h"

#include <imgui/imgui.h>

#include "Panels/SceneHierarchyPanel.h"
#include "Panels/SceneViewportPanel.h"
#include "Panels/InspectorPanel.h"

#include "Lucky/Renderer/MeshFactory.h"

#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/MeshFilterComponent.h"
#include "Lucky/Scene/Components/MeshRendererComponent.h"

namespace Lucky
{
#define SCENE_HIERARCHY_PANEL_ID "SceneHierarchyPanel"
#define SCENE_VIEWPORT_PANEL_ID "SceneViewportPanel"
#define INSPECTOR_PANEL_ID "InspectorPanel"
    
    EditorLayer::EditorLayer()
        : Layer("EditorLayer")
    {

    }

    void EditorLayer::OnAttach()
    {
        LF_TRACE("EditorLayer::OnAttach");

        m_Scene = CreateRef<Scene>("New Scene");
        
        m_PanelManager = CreateScope<PanelManager>();

        m_PanelManager->AddPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL_ID, "Hierarchy", true, m_Scene);
        m_PanelManager->AddPanel<SceneViewportPanel>(SCENE_VIEWPORT_PANEL_ID, "Scene", true, m_Scene);
        m_PanelManager->AddPanel<InspectorPanel>(INSPECTOR_PANEL_ID, "Inspector", true, m_Scene);
        
        Ref<Mesh> cubeMesh = MeshFactory::CreateCube();
        cubeMesh->SetName("Cube");  // Temp
        Entity cubeEntity = m_Scene->CreateEntity("Cube");
        cubeEntity.AddComponent<MeshFilterComponent>(cubeMesh);
        
        // Temp ≤‚ ‘
        Ref<Material> defaultMaterial = CreateRef<Material>("Default Material", Renderer3D::GetShaderLibrary()->Get("TextureShader"));
        
        defaultMaterial->SetFloat3("u_AmbientCoeff", glm::vec3(0.2f));
        defaultMaterial->SetFloat3("u_DiffuseCoeff", glm::vec3(0.8f));
        defaultMaterial->SetFloat3("u_SpecularCoeff", glm::vec3(0.5f));
        defaultMaterial->SetFloat("u_Shininess", 32.0f);
        defaultMaterial->SetInt("u_TextureIndex", 0);
            
        MeshRendererComponent& meshRenderer = cubeEntity.AddComponent<MeshRendererComponent>();
        meshRenderer.SetMaterial(0, defaultMaterial);
    }

    void EditorLayer::OnDetach()
    {
        LF_TRACE("EditorLayer::OnDetach");
    }

    void EditorLayer::OnUpdate(DeltaTime dt)
    {
        m_PanelManager->OnUpdate(dt);
    }

    void EditorLayer::OnImGuiRender()
    {
        // ‰÷»æ DockSpace
        m_EditorDockSpace.ImGuiRender();

        UI_DrawMenuBar();

        m_PanelManager->OnImGuiRender();
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
                // ÕÀ≥ˆ
                if (ImGui::MenuItem("Quit", "Ctrl Q"))
                {
                    Application::GetInstance().Close();    // ÕÀ≥ˆ≥Ã–Ú
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                if (ImGui::MenuItem("Hierarchy"))
                {
                    uint32_t panelID = Hash::GenerateFNVHash(SCENE_HIERARCHY_PANEL_ID);
                    PanelData* panelData = m_PanelManager->GetPanelData(panelID);
                    panelData->IsOpen = true;
                }
                
                if (ImGui::MenuItem("Scene"))
                {
                    uint32_t panelID = Hash::GenerateFNVHash(SCENE_VIEWPORT_PANEL_ID);
                    PanelData* panelData = m_PanelManager->GetPanelData(panelID);
                    panelData->IsOpen = true;
                }
                
                if (ImGui::MenuItem("Inspector"))
                {
                    uint32_t panelID = Hash::GenerateFNVHash(INSPECTOR_PANEL_ID);
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
