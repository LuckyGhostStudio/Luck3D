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
        
        // Temp 测试
        
        Ref<Mesh> cubeMesh = MeshFactory::CreateCube();
        cubeMesh->SetName("Cube");  // Temp
        
        cubeMesh->UpdateSubMesh(0, { 0, 18, 12, 0 });   // 子网格 0
        cubeMesh->AddSubMesh(18, 18, 12, 1);    // 子网格 1
        
        Entity cubeEntity = m_Scene->CreateEntity("Cube");
        cubeEntity.AddComponent<MeshFilterComponent>(cubeMesh);
        
        // 材质 0
        Ref<Material> testMaterial0 = CreateRef<Material>("Test_Material0", Renderer3D::GetShaderLibrary()->Get("Standard"));
        testMaterial0->SetFloat3("u_AmbientCoeff", glm::vec3(0.2f));
        testMaterial0->SetFloat3("u_DiffuseCoeff", glm::vec3(0.8f));
        testMaterial0->SetFloat3("u_SpecularCoeff", glm::vec3(0.5f));
        testMaterial0->SetFloat("u_Shininess", 32.0f);
        testMaterial0->SetTexture("u_MainTexture", Texture2D::Create("Assets/Textures/Texture_Gloss.png"));
        
        // 材质 1
        Ref<Material> testMaterial1 = CreateRef<Material>("Test_Material1", Renderer3D::GetShaderLibrary()->Get("Standard"));
        testMaterial1->SetFloat3("u_AmbientCoeff", glm::vec3(0.2f));
        testMaterial1->SetFloat3("u_DiffuseCoeff", glm::vec3(1.0f));
        testMaterial1->SetFloat3("u_SpecularCoeff", glm::vec3(0.9f));
        testMaterial1->SetFloat("u_Shininess", 32.0f);
        testMaterial1->SetTexture("u_MainTexture", Texture2D::Create("Assets/Textures/Lucky_Logo.png"));
            
        MeshRendererComponent& meshRenderer = cubeEntity.AddComponent<MeshRendererComponent>();
        meshRenderer.SetMaterial(0, testMaterial0);
        meshRenderer.SetMaterial(1, testMaterial1);
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
        // 渲染 DockSpace
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
                // 退出
                if (ImGui::MenuItem("Quit", "Ctrl Q"))
                {
                    Application::GetInstance().Close();    // 退出程序
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
