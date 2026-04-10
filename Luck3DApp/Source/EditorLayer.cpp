#include "EditorLayer.h"

#include <imgui/imgui.h>

#include "Panels/SceneHierarchyPanel.h"
#include "Panels/SceneViewportPanel.h"
#include "Panels/InspectorPanel.h"

#include "Lucky/Renderer/MeshFactory.h"

#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/SelectionManager.h"

#include "Lucky/Serialization//SceneSerializer.h"

#include "Lucky/Utils/PlatformUtils.h"

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
        
        // Temp 测试 Cube
        Entity cubeEntity = m_Scene->CreateEntity("Cube");
        
        // MeshFilter
        cubeEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Cube);
        
        // MeshRenderer
        cubeEntity.AddComponent<MeshRendererComponent>();
        
        // 测试 DirLight
        Entity lightEntity = m_Scene->CreateEntity("Directional Light");
        
        // DirectionalLight
        lightEntity.AddComponent<DirectionalLightComponent>();
        
        // 设置初始方向斜向下
        TransformComponent& transform = lightEntity.GetComponent<TransformComponent>();
        transform.SetRotationEuler(glm::vec3(glm::radians(50.0f), glm::radians(-32.0f), 0.0f));

        auto commandLineArgs = Application::GetInstance().GetSpecification().CommandLineArgs;
        // 从命令行加载场景
        if (commandLineArgs.Count > 1)
        {
            const char* sceneFilePath = commandLineArgs[1];

            OpenScene(sceneFilePath);
        }
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
                if (ImGui::MenuItem("New"))
                {
                    NewScene();
                }
                
                if (ImGui::MenuItem("Open..."))
                {
                    OpenScene();
                }
                
                if (ImGui::MenuItem("Save"))
                {
                    SaveScene();
                }
                
                if (ImGui::MenuItem("Save As..."))
                {
                    SaveSceneAs();
                }
                
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

    void EditorLayer::NewScene()
    {
        SelectionManager::Deselect();               // 清空选中项 防止创建新场景后当前选中的 UUID 无效
        
        m_Scene = CreateRef<Scene>();               // 创建新场景
        m_SceneFilePath = std::filesystem::path();  // 场景路径

        // 设置所有面板的场景上下文
        m_PanelManager->GetPanel<SceneViewportPanel>(SCENE_VIEWPORT_PANEL_ID)->SetScene(m_Scene);
        m_PanelManager->GetPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL_ID)->SetScene(m_Scene);
        m_PanelManager->GetPanel<InspectorPanel>(INSPECTOR_PANEL_ID)->SetScene(m_Scene);
    }

    void EditorLayer::OpenScene()
    {
        // 打开文件对话框（文件类型名\0 文件类型.luck3d）
        std::string filepath = FileDialogs::OpenFile("Luck3D Scene(*.luck3d)\0*.luck3d\0");

        // 路径不为空
        if (!filepath.empty())
        {
            OpenScene(filepath);    // 打开场景
        }
    }

    void EditorLayer::OpenScene(const std::filesystem::path& filepath)
    {
        // 不是场景文件
        if (filepath.extension().string() != ".luck3d")
        {
            LF_WARN("Can not Load {0} - Not a Scene File.", filepath.filename().string());
            return;
        }

        Ref<Scene> newScene = CreateRef<Scene>();   // 创建新场景
        SceneSerializer serializer(newScene);       // 场景序列化器
        
        // 反序列化：加载文件场景到新场景
        if (serializer.Deserialize(filepath.string()))
        {
            m_Scene = newScene;
            m_SceneFilePath = filepath;     // 当前场景路径

            // 设置所有面板的场景上下文
            m_PanelManager->GetPanel<SceneViewportPanel>(SCENE_VIEWPORT_PANEL_ID)->SetScene(m_Scene);
            m_PanelManager->GetPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL_ID)->SetScene(m_Scene);
            m_PanelManager->GetPanel<InspectorPanel>(INSPECTOR_PANEL_ID)->SetScene(m_Scene);
        }
    }

    void EditorLayer::SaveScene()
    {
        if (!m_SceneFilePath.empty())
        {
            SerializeScene(m_Scene, m_SceneFilePath);   // 序列化场景到当前场景
        }
        else
        {
            SaveSceneAs();  // 场景另存为
        }
    }

    void EditorLayer::SaveSceneAs()
    {
        // 保存文件对话框（文件类型名\0 文件类型.luck3d）
        std::string filepath = FileDialogs::SaveFile("Luck3D Scene(*.luck3d)\0*.luck3d\0");

        // 路径不为空
        if (!filepath.empty())
        {
            SerializeScene(m_Scene, filepath);    // 序列化场景
            m_SceneFilePath = filepath;             // 记录当前场景路径
        }
    }

    void EditorLayer::SerializeScene(Ref<Scene> scene, const std::filesystem::path& filepath)
    {
        SceneSerializer serializer(scene);          // 场景序列化器
        serializer.Serialize(filepath.string());    // 序列化：保存场景
    }
}
