#include "lcpch.h"
#include "SceneManager.h"

#include "Scene.h"
#include "SelectionManager.h"

#include "Lucky/Asset/AssetManager.h"
#include "Lucky/Utils/PlatformUtils.h"

#include <unordered_map>

namespace Lucky
{
    namespace
    {
        // ---- 模块局部状态 ----

        Ref<Scene> s_ActiveScene;
        std::unordered_map<SceneManager::SubscriptionHandle, SceneManager::SceneChangedCallback> s_Subscribers;
        SceneManager::SubscriptionHandle s_NextHandle = 1;   // 0 保留作为无效值

        /// <summary>
        /// 通知所有订阅者场景已切换
        /// 复制订阅表快照后再遍历，避免回调中修改订阅表导致迭代器失效
        /// </summary>
        void Notify(const Ref<Scene>& scene)
        {
            std::vector<SceneManager::SceneChangedCallback> snapshot;
            snapshot.reserve(s_Subscribers.size());
            for (auto& [id, cb] : s_Subscribers)
            {
                if (cb)
                {
                    snapshot.push_back(cb);
                }
            }

            for (auto& cb : snapshot)
            {
                cb(scene);
            }
        }
    }

    // ---- 生命周期 ----

    void SceneManager::Init()
    {
        s_ActiveScene.reset();
        s_Subscribers.clear();
        s_NextHandle = 1;
    }

    void SceneManager::Shutdown()
    {
        s_Subscribers.clear();
        s_ActiveScene.reset();
    }

    // ---- ActiveScene 访问 ----

    const Ref<Scene>& SceneManager::GetActiveScene()
    {
        return s_ActiveScene;
    }

    void SceneManager::SetActiveScene(const Ref<Scene>& scene)
    {
        // 切换活动场景时清空选中项：旧场景中的 Entity UUID 在新场景中已不再有效
        SelectionManager::Deselect();

        s_ActiveScene = scene;
        Notify(s_ActiveScene);
    }

    // ---- 场景打开 / 新建 ----

    bool SceneManager::OpenScene(AssetHandle handle)
    {
        if (!handle.IsValid())
        {
            LF_CORE_ERROR("SceneManager::OpenScene - Invalid asset handle.");
            return false;
        }

        if (AssetManager::GetAssetType(handle) != AssetType::Scene)
        {
            LF_CORE_WARN("SceneManager::OpenScene - Asset handle {0} is not a Scene.", static_cast<uint64_t>(handle));
            return false;
        }

        // 场景不使用缓存（每次打开都是新实例）
        AssetManager::UnloadAsset(handle);
        Ref<Scene> scene = AssetManager::GetAsset<Scene>(handle);
        if (!scene)
        {
            LF_CORE_ERROR("SceneManager::OpenScene - Failed to load scene asset (handle {0}).", static_cast<uint64_t>(handle));
            return false;
        }

        SetActiveScene(scene);
        LF_CORE_INFO("SceneManager::OpenScene - Opened scene '{0}' (handle {1}).", scene->GetName(), static_cast<uint64_t>(handle));
        return true;
    }

    bool SceneManager::OpenScene(const std::filesystem::path& filepath)
    {
        if (filepath.extension().string() != ".luck3d")
        {
            LF_CORE_WARN("SceneManager::OpenScene - Not a scene file: '{0}'.", filepath.filename().string());
            return false;
        }

        // 转为相对项目根目录的正斜杠路径（与 AssetRegistry 存储格式一致）
        std::filesystem::path relPath = std::filesystem::relative(filepath);
        std::string normalizedPath = relPath.generic_string();

        AssetHandle handle = AssetManager::ImportAsset(normalizedPath, AssetType::Scene);
        if (!handle.IsValid())
        {
            LF_CORE_ERROR("SceneManager::OpenScene - Failed to import scene: '{0}'.", normalizedPath);
            return false;
        }

        return OpenScene(handle);
    }

    bool SceneManager::NewSceneWithDialog()
    {
        // 通过文件对话框指定保存路径
        std::string filepath = FileDialogs::SaveFile("Luck3D Scene(*.luck3d)\0*.luck3d\0");
        if (filepath.empty())
        {
            return false;   // 用户取消
        }

        // 确保扩展名为 .luck3d
        std::filesystem::path path(filepath);
        if (path.extension() != ".luck3d")
        {
            path.replace_extension(".luck3d");
        }

        // 创建新的空场景
        Ref<Scene> scene = CreateRef<Scene>();
        scene->SetName(path.stem().string());

        // 计算相对路径（AssetManager 要求相对路径 + 正斜杠）
        std::filesystem::path relPath = std::filesystem::relative(path);
        std::string normalizedPath = relPath.generic_string();

        AssetHandle handle = AssetManager::CreateAsset(scene, normalizedPath);
        if (!handle.IsValid())
        {
            LF_CORE_ERROR("SceneManager::NewSceneWithDialog - Failed to create scene asset: '{0}'.", normalizedPath);
            return false;
        }

        SetActiveScene(scene);
        LF_CORE_INFO("SceneManager::NewSceneWithDialog - Created scene '{0}' at '{1}'.", scene->GetName(), normalizedPath);
        return true;
    }

    bool SceneManager::SaveSceneAs()
    {
        if (!s_ActiveScene)
        {
            LF_CORE_WARN("SceneManager::SaveSceneAs - No active scene to save.");
            return false;
        }

        // 通过文件对话框指定另存路径
        std::string filepath = FileDialogs::SaveFile("Luck3D Scene(*.luck3d)\0*.luck3d\0");
        if (filepath.empty())
        {
            return false;   // 用户取消
        }

        // 确保扩展名为 .luck3d
        std::filesystem::path path(filepath);
        if (path.extension() != ".luck3d")
        {
            path.replace_extension(".luck3d");
        }

        // 计算相对路径（AssetManager 要求相对路径 + 正斜杠）
        std::filesystem::path relPath = std::filesystem::relative(path);
        std::string normalizedPath = relPath.generic_string();

        // 直接用当前 ActiveScene 实例创建新资产
        // 内部会：注册到 Registry -> 序列化到新文件 -> 缓存 -> 保存 Registry
        // 注意：CreateAsset 会调用 asset->SetHandle(newHandle)，直接改写 s_ActiveScene 的 Handle
        // 后续 EditorLayer::SaveScene 通过 scene->GetHandle() 就能拿到新路径，行为对齐 Unity Save As
        AssetHandle newHandle = AssetManager::CreateAsset(s_ActiveScene, normalizedPath);
        if (!newHandle.IsValid())
        {
            LF_CORE_ERROR("SceneManager::SaveSceneAs - Failed to create scene asset: '{0}'.", normalizedPath);
            return false;
        }

        // 更新场景名（与新文件名保持一致）
        s_ActiveScene->SetName(path.stem().string());

        // 广播一次切换事件：让 Inspector 等面板刷新对场景 Handle / Name 的显示
        // 场景实例本身未变，仅 Handle 与 Name 更新，因此这里保持 s_ActiveScene 不动、只走通知
        Notify(s_ActiveScene);

        LF_CORE_INFO("SceneManager::SaveSceneAs - Saved scene '{0}' to '{1}'.", s_ActiveScene->GetName(), normalizedPath);
        return true;
    }

    // ---- 事件订阅 ----

    SceneManager::SubscriptionHandle SceneManager::Subscribe(SceneChangedCallback callback)
    {
        if (!callback)
        {
            return 0;
        }

        SubscriptionHandle handle = s_NextHandle++;
        s_Subscribers.emplace(handle, std::move(callback));
        return handle;
    }

    void SceneManager::Unsubscribe(SubscriptionHandle handle)
    {
        if (handle == 0)
        {
            return;
        }
        s_Subscribers.erase(handle);
    }
}
