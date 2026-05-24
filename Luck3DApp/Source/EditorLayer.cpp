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

#include "Lucky/Serialization/SceneSerializer.h"
#include "Lucky/Serialization/MeshSerializer.h"
#include "Lucky/Asset/ModelLoader.h"
#include "Lucky/Asset/AssetManager.h"
#include "Lucky/Editor/EditorIconManager.h"

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

        // 初始化图标管理器（在创建面板之前）
        EditorIconManager::Init();

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

        EditorIconManager::Shutdown();
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
                    CreateScene();
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

    void EditorLayer::CreateScene()
    {
        // 通过文件对话框指定保存路径
        std::string filepath = FileDialogs::SaveFile("Luck3D Scene(*.luck3d)\0*.luck3d\0");
        if (filepath.empty()) return;
        
        // 确保扩展名为 .luck3d
        std::filesystem::path path(filepath);
        if (path.extension() != ".luck3d")
        {
            path.replace_extension(".luck3d");
        }
        
        SelectionManager::Deselect();               // 清空选中项
        
        // 创建新的空场景
        Ref<Scene> scene = CreateRef<Scene>();
        scene->SetName(path.stem().string());
        
        // 计算相对路径
        std::filesystem::path relPath = std::filesystem::relative(path);
        std::string normalizedPath = relPath.generic_string();
        
        // 通过 CreateAsset 创建资产文件并注册到 Registry
        AssetHandle sceneHandle = AssetManager::CreateAsset(scene, normalizedPath);
        if (!sceneHandle.IsValid())
        {
            LF_CORE_ERROR("Failed to create scene asset: '{0}'", normalizedPath);
            return;
        }
        
        m_Scene = scene;

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
        
        SelectionManager::Deselect();               // 清空选中项

        // 注册到资产系统（如果尚未注册）
        std::filesystem::path relPath = std::filesystem::relative(filepath);
        std::string normalizedPath = relPath.generic_string();
        
        AssetHandle sceneHandle = AssetManager::ImportAsset(normalizedPath, AssetType::Scene);
        if (!sceneHandle.IsValid())
        {
            LF_CORE_ERROR("Failed to import scene: '{0}'", normalizedPath);
            return;
        }
        
        // 通过 AssetManager 加载场景
        // 场景不使用缓存（每次打开都是新实例）
        AssetManager::UnloadAsset(sceneHandle);  // 清除旧缓存
        Ref<Scene> scene = AssetManager::GetAsset<Scene>(sceneHandle);
        if (!scene)
        {
            LF_CORE_ERROR("Failed to load scene asset: '{0}'", normalizedPath);
            return;
        }
        
        m_Scene = scene;

        // 设置所有面板的场景上下文
        m_PanelManager->GetPanel<SceneViewportPanel>(SCENE_VIEWPORT_PANEL_ID)->SetScene(m_Scene);
        m_PanelManager->GetPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL_ID)->SetScene(m_Scene);
        m_PanelManager->GetPanel<InspectorPanel>(INSPECTOR_PANEL_ID)->SetScene(m_Scene);
        m_PanelManager->GetPanel<LightingPanel>(LIGHTING_PANEL_ID)->SetScene(m_Scene);
    }

    void EditorLayer::SaveScene()
    {
        if (!m_Scene->GetHandle().IsValid())
        {
            LF_CORE_WARN("No scene to save (no valid handle).");
            return;
        }
        
        const std::string& filepath = AssetManager::GetAssetFilePath(m_Scene->GetHandle());
        std::string absolutePath = std::filesystem::absolute(filepath).string();
        
        SceneSerializer::Serialize(m_Scene, absolutePath);
    }

    void EditorLayer::SerializeScene(Ref<Scene> scene, const std::filesystem::path& filepath)
    {
        SceneSerializer::Serialize(scene, filepath.string());
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
        // 1. 通过 ModelLoader 解析外部模型文件
        ModelLoadResult result = ModelLoader::Load(filepath.string());
        if (!result.Success)
        {
            LF_CORE_ERROR("Failed to load model: '{0}' - {1}", filepath.string(), result.ErrorMessage);
            return;
        }

        // 2. 确定 .lmesh 输出路径（在 Assets 目录中）
        std::string meshName = filepath.stem().string();
        std::filesystem::path lmeshRelPath = std::filesystem::path("Assets/Meshes") / (meshName + ".lmesh");
        std::string absoluteLmeshPath = std::filesystem::absolute(lmeshRelPath).string();

        // 3. 序列化 Mesh 到 .lmesh 文件
        result.MeshData->SetName(meshName);
        if (!MeshSerializer::Serialize(result.MeshData, absoluteLmeshPath))
        {
            LF_CORE_ERROR("Failed to serialize mesh to .lmesh: '{0}'", absoluteLmeshPath);
            return;
        }

        // 4. 注册 .lmesh 文件到资产系统
        std::string normalizedLmeshPath = lmeshRelPath.generic_string();
        AssetHandle meshHandle = AssetManager::ImportAsset(normalizedLmeshPath, AssetType::Mesh);
        if (!meshHandle.IsValid())
        {
            LF_CORE_ERROR("Failed to register .lmesh asset: '{0}'", normalizedLmeshPath);
            return;
        }

        // 5. 设置 Handle 并缓存
        result.MeshData->SetHandle(meshHandle);

        // 6. 创建实体
        Entity entity = m_Scene->CreateEntity(meshName);

        // 添加 MeshFilter
        auto& meshFilterComponent = entity.AddComponent<MeshFilterComponent>();
        meshFilterComponent.Mesh = result.MeshData;

        // 7. 材质处理
        auto& meshRenderer = entity.AddComponent<MeshRendererComponent>();
        if (!result.Materials.empty())
        {
            // 为每个导入的材质保存为独立 .lmat 文件并注册到资产系统
            std::filesystem::path materialsDir = "Assets/Materials";

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
                    matName = std::format("{0}_Material_{1}", meshName, i);
                    material->SetName(matName);
                }

                // 构造 .lmat 文件路径（相对路径）
                std::filesystem::path matFilePath = materialsDir / (matName + ".lmat");
                std::string matFilePathStr = matFilePath.generic_string();

                AssetManager::CreateAsset(material, matFilePathStr);  // 创建材质资产
            }
            meshRenderer.Materials = result.Materials;
        }

        SelectionManager::Select(entity.GetUUID()); // 选中当前实体

        LF_CORE_INFO("Imported model: {0} ({1} SubMeshes, {2} Materials)", meshName, result.MeshData->GetSubMeshes().size(), meshRenderer.Materials.size());
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
        }
        
        // 转为相对路径（CreateAsset 要求相对路径）
        std::filesystem::path relPath = std::filesystem::relative(path);
        std::string normalizedPath = relPath.generic_string();
        
        // 创建默认材质（使用 Standard Shader）
        Ref<ShaderLibrary>& shaderLib = Renderer3D::GetShaderLibrary();
        Ref<Shader> standardShader = shaderLib->Get("Standard");
        
        std::string materialName = relPath.stem().string();
        Ref<Material> material = CreateRef<Material>(materialName, standardShader);
        
        AssetManager::CreateAsset(material, normalizedPath);  // 创建材质资产
    }
}
