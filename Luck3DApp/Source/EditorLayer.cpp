#include "EditorLayer.h"

#include <imgui/imgui.h>

#include "Panels/SceneHierarchyPanel.h"
#include "Panels/SceneViewportPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ProjectAssetsPanel.h"
#include "Panels/RenderPipelinePanel.h"
#include "Panels/PreferencesPanel.h"
#include "Panels/LightingPanel.h"

#include "Lucky/Renderer/MeshFactory.h"
#include "Lucky/Renderer/Renderer3D.h"

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
#define PROJECT_ASSETS_PANEL_ID "ProjectAssetsPanel"
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

        m_Scene = CreateRef<Scene>("New Scene");
        
        m_PanelManager = CreateScope<PanelManager>();
        
        m_PanelManager->AddPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL_ID, "Hierarchy", true, m_Scene);
        m_PanelManager->AddPanel<SceneViewportPanel>(SCENE_VIEWPORT_PANEL_ID, "Scene", true, m_Scene);
        m_PanelManager->AddPanel<InspectorPanel>(INSPECTOR_PANEL_ID, "Inspector", true, m_Scene);
        m_PanelManager->AddPanel<ProjectAssetsPanel>(PROJECT_ASSETS_PANEL_ID, "Project", true);
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
                
                if (ImGui::MenuItem("Create Material..."))
                {
                    CreateMaterial();
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

                    if (ImGui::MenuItem("Lighting"))
                    {
                        uint32_t panelID = Hash::GenerateFNVHash(LIGHTING_PANEL_ID);
                        PanelData* panelData = m_PanelManager->GetPanelData(panelID);
                        panelData->IsOpen = true;
                    }

                    if (ImGui::MenuItem("Project"))
                    {
                        uint32_t panelID = Hash::GenerateFNVHash(PROJECT_ASSETS_PANEL_ID);
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

                ImGui::Separator();

                if (ImGui::BeginMenu("Layouts"))
                {
                    if (ImGui::MenuItem("Default"))
                    {
                        m_EditorDockSpace.ResetToDefaultLayout();

                        // 确保默认布局中的面板都处于打开状态
                        auto OpenPanel = [this](const char* panelID)
                            {
                                uint32_t id = Hash::GenerateFNVHash(panelID);
                                PanelData* data = m_PanelManager->GetPanelData(id);
                                data->IsOpen = true;
                            };

                        OpenPanel(SCENE_HIERARCHY_PANEL_ID);
                        OpenPanel(SCENE_VIEWPORT_PANEL_ID);
                        OpenPanel(INSPECTOR_PANEL_ID);
                        OpenPanel(PROJECT_ASSETS_PANEL_ID);
                        OpenPanel(RENDER_PIPELINE_PANEL_ID);
                    }

                    ImGui::EndMenu();
                }

                ImGui::Separator();
                
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
            // 为每个导入的材质保存为独立 .lmat 文件并注册到资产系统
            std::filesystem::path materialsDir = relPath.parent_path() / "Materials";
            
            for (size_t i = 0; i < result.Materials.size(); ++i)
            {
                auto& material = result.Materials[i];
                if (!material)
                {
                    continue;
                }
                
                // 构造材质文件名：模型名_材质名.lmat
                std::string matName = material->GetName();
                if (matName.empty())
                {
                    matName = std::format("{0}_Material_{1}", name, i);
                    material->SetName(matName);
                }
                
                // 构造 .lmat 文件路径
                std::filesystem::path matFilePath = materialsDir / (matName + ".lmat");
                std::string matFilePathStr = matFilePath.generic_string();
                std::string absoluteMatPath = std::filesystem::absolute(matFilePath).string();
                
                AssetManager::CreateAsset(material, absoluteMatPath);  // 创建材质资产
            }
            meshRenderer.Materials = result.Materials;
        }
        
        SelectionManager::Select(entity.GetUUID()); // 选中当前实体
        
        LF_CORE_INFO("Imported model: {0} ({1} SubMeshes, {2} Materials)", name, mesh->GetSubMeshes().size(), meshRenderer.Materials.size());
    }

    void EditorLayer::CreateMaterial()
    {
        // 打开保存文件对话框
        std::string filepath = FileDialogs::SaveFile("Luck3D Material (*.lmat)\0*.lmat\0");
        
        if (filepath.empty())
        {
            return;
        }
        
        // 确保扩展名为 .lmat
        std::filesystem::path path(filepath);
        if (path.extension() != ".lmat")
        {
            path.replace_extension(".lmat");
            filepath = path.string();
        }
        
        // 创建默认材质（使用 Standard Shader）
        Ref<ShaderLibrary>& shaderLib = Renderer3D::GetShaderLibrary();
        Ref<Shader> standardShader = shaderLib->Get("Standard");
        
        std::string materialName = path.stem().string();
        Ref<Material> material = CreateRef<Material>(materialName, standardShader);
        
        AssetManager::CreateAsset(material, filepath);  // 创建材质资产
    }
}
