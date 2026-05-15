#include "EditorLayer.h"

#include <imgui/imgui.h>

#include "Panels/SceneHierarchyPanel.h"
#include "Panels/SceneViewportPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/RenderPipelinePanel.h"
#include "Panels/PreferencesPanel.h"
#include "Panels/LightingPanel.h"

#include "Lucky/Renderer/MeshFactory.h"

#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/SelectionManager.h"

#include "Lucky/Utils/PlatformUtils.h"

#include "Lucky/Serialization//SceneSerializer.h"
#include "Lucky/Asset/MeshImporter.h"
#include "Lucky/Asset/AssetManager.h"

#include <filesystem>

namespace Lucky
{
#define SCENE_HIERARCHY_PANEL_ID "SceneHierarchyPanel"
#define SCENE_VIEWPORT_PANEL_ID "SceneViewportPanel"
#define INSPECTOR_PANEL_ID "InspectorPanel"
#define RENDER_PIPELINE_PANEL_ID "RenderPipelinePanel"
    
#define PREFERENCES_PANEL_ID "PreferencesPanel"
#define LIGHTING_PANEL_ID "LightingPanel"
    
    EditorLayer::EditorLayer()
        : Layer("EditorLayer")
    {

    }

    void EditorLayer::OnAttach()
    {
        LF_TRACE("EditorLayer::OnAttach");

        // 初始化资产系统
        AssetManager::Init();

        m_Scene = CreateRef<Scene>("New Scene");
        
        m_PanelManager = CreateScope<PanelManager>();
        
        m_PanelManager->AddPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL_ID, "Hierarchy", true, m_Scene);
        m_PanelManager->AddPanel<SceneViewportPanel>(SCENE_VIEWPORT_PANEL_ID, "Scene", true, m_Scene);
        m_PanelManager->AddPanel<InspectorPanel>(INSPECTOR_PANEL_ID, "Inspector", true, m_Scene);
        m_PanelManager->AddPanel<RenderPipelinePanel>(RENDER_PIPELINE_PANEL_ID, "Render Pipeline", true);
        m_PanelManager->AddPanel<PreferencesPanel>(PREFERENCES_PANEL_ID, "Preferences", false);
        m_PanelManager->AddPanel<LightingPanel>(LIGHTING_PANEL_ID, "Lighting", false, m_Scene);
        
        // Temp 测试 Cube
        Entity cubeEntity = m_Scene->CreateEntity("Cube");
        
        // MeshFilter
        cubeEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Cube);
        
        // MeshRenderer
        cubeEntity.AddComponent<MeshRendererComponent>();
        
        // 测试 DirLight
        Entity lightEntity = m_Scene->CreateEntity("Directional Light");
        
        // DirectionalLight
        lightEntity.AddComponent<LightComponent>(LightType::Directional);
        
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

        // 关闭资产系统
        AssetManager::Shutdown();
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
                
                ImGui::Separator();
                
                if (ImGui::MenuItem("Save"))
                {
                    SaveScene();
                }
                
                if (ImGui::MenuItem("Save As..."))
                {
                    SaveSceneAs();
                }
                
                ImGui::Separator();
                
                if (ImGui::MenuItem("Import Model..."))
                {
                    ImportModel();
                }
                
                ImGui::Separator();
                
                // 退出
                if (ImGui::MenuItem("Quit", "Ctrl Q"))
                {
                    Application::GetInstance().Close();    // 退出程序
                }

                ImGui::EndMenu();
            }

            // Edit 菜单（新增）
            if (ImGui::BeginMenu("Edit"))
            {
                // 偏好设置
                if (ImGui::MenuItem("Preferences..."))
                {
                    uint32_t panelID = Hash::GenerateFNVHash(PREFERENCES_PANEL_ID);
                    PanelData* panelData = m_PanelManager->GetPanelData(panelID);
                    panelData->IsOpen = true;
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Window"))
            {
                if (ImGui::BeginMenu("Panels"))
                {
                    if (ImGui::MenuItem("Hierarchy"))
                    {
                        uint32_t panelID = Hash::GenerateFNVHash(SCENE_HIERARCHY_PANEL_ID);
                        PanelData* panelData = m_PanelManager->GetPanelData(panelID);
                        panelData->IsOpen = true;
                    }
                
                    if (ImGui::MenuItem("Inspector"))
                    {
                        uint32_t panelID = Hash::GenerateFNVHash(INSPECTOR_PANEL_ID);
                        PanelData* panelData = m_PanelManager->GetPanelData(panelID);
                        panelData->IsOpen = true;
                    }
                    
                    if (ImGui::MenuItem("Scene"))
                    {
                        uint32_t panelID = Hash::GenerateFNVHash(SCENE_VIEWPORT_PANEL_ID);
                        PanelData* panelData = m_PanelManager->GetPanelData(panelID);
                        panelData->IsOpen = true;
                    }
                    
                    ImGui::EndMenu();
                }
                
                if (ImGui::BeginMenu("Rendering"))
                {
                    if (ImGui::MenuItem("Render Pipeline"))
                    {
                        uint32_t panelID = Hash::GenerateFNVHash(RENDER_PIPELINE_PANEL_ID);
                        PanelData* panelData = m_PanelManager->GetPanelData(panelID);
                        panelData->IsOpen = true;
                    }
                    
                    if (ImGui::MenuItem("Lighting"))
                    {
                        uint32_t panelID = Hash::GenerateFNVHash(LIGHTING_PANEL_ID);
                        PanelData* panelData = m_PanelManager->GetPanelData(panelID);
                        panelData->IsOpen = true;
                    }
                    
                    ImGui::EndMenu();
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
        m_PanelManager->GetPanel<LightingPanel>(LIGHTING_PANEL_ID)->SetScene(m_Scene);
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
        
        SelectionManager::Deselect();               // 清空选中项 防止创建新场景后当前选中的 UUID 无效

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
            m_PanelManager->GetPanel<LightingPanel>(LIGHTING_PANEL_ID)->SetScene(m_Scene);
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

    void EditorLayer::ImportModel()
    {
        std::string filepath = FileDialogs::OpenFile("3D Model (*.obj;*.fbx;*.gltf;*.glb)\0*.obj;*.fbx;*.gltf;*.glb\0""All Files (*.*)\0*.*\0");
        
        if (!filepath.empty())
        {
            ImportModel(filepath);
        }
    }

    void EditorLayer::ImportModel(const std::filesystem::path& filepath)
    {
        // TODO 检查文件类型
        // TODO 导入设置面板
        
        // 通过 AssetManager 导入模型资产
        std::filesystem::path relPath = std::filesystem::relative(filepath);
        std::string normalizedPath = relPath.generic_string();
        
        AssetHandle meshHandle = AssetManager::ImportAsset(normalizedPath, AssetType::Mesh);
        if (!meshHandle.IsValid())
        {
            LF_CORE_ERROR("Failed to import model: '{0}'", normalizedPath);
            return;
        }
        
        // 通过 AssetManager 加载 Mesh
        Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(meshHandle);
        if (!mesh)
        {
            LF_CORE_ERROR("Failed to load mesh asset: '{0}'", normalizedPath);
            return;
        }
        
        // 创建实体
        std::string name = filepath.stem().string();
        Entity entity = m_Scene->CreateEntity(name);
        
        // 添加 MeshFilter
        auto& meshFilterComponent = entity.AddComponent<MeshFilterComponent>();
        meshFilterComponent.Mesh = mesh;
        
        // 添加 MeshRenderer
        // 导入模型时同时获取材质（通过 MeshImporter 的结果）
        // 注意：当前 MeshAssetImporter 只返回 Mesh，材质需要单独处理
        // 暂时使用 MeshImporter 直接获取材质列表
        MeshImportResult result = MeshImporter::Import(filepath.string());
        auto& meshRenderer = entity.AddComponent<MeshRendererComponent>();
        if (result.Success)
        {
            // 为每个导入的材质注册到资产系统并设置 Handle
            for (auto& material : result.Materials)
            {
                if (material)
                {
                    // 材质继承 Asset，设置一个临时 Handle（内存中的材质，尚无独立 .mat 文件）
                    material->SetHandle(AssetHandle::Generate());
                }
            }
            meshRenderer.Materials = result.Materials;
        }
        
        SelectionManager::Select(entity.GetUUID()); // 选中当前实体
        
        // 保存 Registry
        AssetManager::SaveRegistry();
        
        LF_CORE_INFO("Imported model: {0} ({1} SubMeshes, {2} Materials)", name, mesh->GetSubMeshes().size(), meshRenderer.Materials.size());
    }
}
