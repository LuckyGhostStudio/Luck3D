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
#include "Lucky/Scene/SceneManager.h"

#include "Lucky/Utils/PlatformUtils.h"

#include "Lucky/Serialization/SceneSerializer.h"
#include "Lucky/Serialization/MeshSerializer.h"
#include "Lucky/Asset/ModelLoader.h"
#include "Lucky/Asset/AssetManager.h"
#include "Lucky/Editor/EditorIconManager.h"
#include "Lucky/Editor/AssetInspectorRegistry.h"
#include "Lucky/Editor/AssetInspectors.h"

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

        // 注册所有资产类型的 Inspector（Inspector 面板会据此分发绘制）
        AssetInspectorRegistry::Register(AssetType::Material,  &MaterialInspector::Draw);
        AssetInspectorRegistry::Register(AssetType::Mesh,      &MeshInspector::Draw);
        AssetInspectorRegistry::Register(AssetType::Texture2D, &Texture2DInspector::Draw);
        AssetInspectorRegistry::Register(AssetType::Scene,     &SceneInspector::Draw);
        AssetInspectorRegistry::Register(AssetType::Shader,    &ShaderInspector::Draw);

        // 初始化 SceneManager：作为当前活动场景的唯一真源
        // 面板通过 Subscribe 订阅切换事件，无需再手动 SetScene
        SceneManager::Init();

        m_PanelManager = CreateScope<PanelManager>();
        
        // 面板 ctor 中订阅 SceneManager 场景切换事件，此处传入的 initialScene 仅用于满足既有 ctor 签名
        // 真正的 ActiveScene 由下方 EnsureDefaultScene / OpenScene 通过订阅回调统一同步到各面板
        Ref<Scene> placeholder = CreateRef<Scene>("New Scene");
        m_PanelManager->AddPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL_ID, "Hierarchy", true, placeholder);
        m_PanelManager->AddPanel<SceneViewportPanel>(SCENE_VIEWPORT_PANEL_ID, "Scene", true, placeholder);
        m_PanelManager->AddPanel<InspectorPanel>(INSPECTOR_PANEL_ID, "Inspector", true, placeholder);
        m_PanelManager->AddPanel<ProjectAssetsPanel>(PROJECT_ASSETS_PANEL_ID, "Project", true);
        m_PanelManager->AddPanel<RenderPipelinePanel>(RENDER_PIPELINE_PANEL_ID, "Render Pipeline", true);
        m_PanelManager->AddPanel<PreferencesPanel>(PREFERENCES_PANEL_ID, "Preferences", false);
        m_PanelManager->AddPanel<LightingPanel>(LIGHTING_PANEL_ID, "Lighting", false, placeholder);

        auto commandLineArgs = Application::GetInstance().GetSpecification().CommandLineArgs;
        // 命令行传入的场景优先；否则加载/创建默认场景
        if (commandLineArgs.Count > 1)
        {
            const char* sceneFilePath = commandLineArgs[1];
            SceneManager::OpenScene(std::filesystem::path(sceneFilePath));
        }
        else
        {
            EnsureDefaultScene();
        }
    }

    void EditorLayer::EnsureDefaultScene()
    {
        // 默认场景固定路径：Assets/Scenes/New Scene.luck3d
        // 使用 std::filesystem::path 构造以正确处理路径分隔符和空格
        const std::filesystem::path relPath = std::filesystem::path("Assets") / "Scenes" / "New Scene.luck3d";
        const std::string normalizedPath = relPath.generic_string();
        const std::filesystem::path absPath = std::filesystem::absolute(relPath);

        if (std::filesystem::exists(absPath))
        {
            // 场景文件已存在（例如上次启动创建过）：直接加载，保留用户在其上的所有修改
            // 内部会 ImportAsset 注册 -> UnloadAsset 清缓存 -> GetAsset 反序列化 -> SetActiveScene 广播
            SceneManager::OpenScene(relPath);
            return;
        }

        // 场景文件不存在：首次启动，构造默认场景并落盘
        // 确保上级目录存在，避免 CreateAsset 内 SaveAssetToFile 失败
        std::filesystem::create_directories(absPath.parent_path());

        Ref<Scene> scene = CreateRef<Scene>("New Scene");

        // 默认 Cube
        Entity cubeEntity = scene->CreateEntity("Cube");
        cubeEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Cube);
        cubeEntity.AddComponent<MeshRendererComponent>();

        // 默认 DirectionalLight
        Entity lightEntity = scene->CreateEntity("Directional Light");
        lightEntity.AddComponent<LightComponent>(LightType::Directional);

        // 设置初始方向斜向下
        TransformComponent& transform = lightEntity.GetComponent<TransformComponent>();
        transform.SetRotationEuler(glm::vec3(glm::radians(50.0f), glm::radians(-32.0f), 0.0f));

        // Main Camera：暂缓，等相机组件到位后再补

        // 落盘 + 注册到资产系统 + 放入缓存
        // 内部会调 scene->SetHandle(newHandle)，此后 Save 可通过 scene->GetHandle() 拿到路径
        AssetHandle sceneHandle = AssetManager::CreateAsset(scene, normalizedPath);
        if (!sceneHandle.IsValid())
        {
            LF_CORE_ERROR("EditorLayer::EnsureDefaultScene - Failed to create default scene: '{0}'", normalizedPath);
            // 即便落盘失败也让内存中的场景可用，避免进入编辑器后没有任何场景
            SceneManager::SetActiveScene(scene);
            return;
        }

        // 广播为当前活动场景，触发所有面板通过订阅回调同步
        SceneManager::SetActiveScene(scene);
    }

    void EditorLayer::OnDetach()
    {
        LF_TRACE("EditorLayer::OnDetach");

        // 关闭 SceneManager：释放 ActiveScene 引用和订阅表
        // 注意：必须在 PanelManager 释放之前调用，避免面板 dtor 时订阅表已失效
        SceneManager::Shutdown();

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
                if (ImGui::MenuItem("New Scene"))
                {
                    SceneManager::NewSceneWithDialog();
                }
                
                if (ImGui::MenuItem("Open Scene"))
                {
                    // 弹出文件对话框，选中后交由 SceneManager 打开
                    std::string filepath = FileDialogs::OpenFile("Luck3D Scene(*.luck3d)\0*.luck3d\0");
                    if (!filepath.empty())
                    {
                        SceneManager::OpenScene(std::filesystem::path(filepath));
                    }
                }
                
                ImGui::Separator();
                
                if (ImGui::MenuItem("Save"))
                {
                    SaveScene();
                }
                
                if (ImGui::MenuItem("Save As..."))
                {
                    SceneManager::SaveSceneAs();
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

    void EditorLayer::SaveScene()
    {
        const Ref<Scene>& scene = SceneManager::GetActiveScene();
        if (!scene || !scene->GetHandle().IsValid())
        {
            LF_CORE_WARN("No scene to save (no valid handle).");
            return;
        }
        
        const std::string& filepath = AssetManager::GetAssetFilePath(scene->GetHandle());
        std::string absolutePath = std::filesystem::absolute(filepath).string();
        
        SceneSerializer::Serialize(scene, absolutePath);
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
        const Ref<Scene>& activeScene = SceneManager::GetActiveScene();
        if (!activeScene)
        {
            LF_CORE_ERROR("EditorLayer::ImportModel - No active scene.");
            return;
        }
        Entity entity = activeScene->CreateEntity(meshName);

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
}
