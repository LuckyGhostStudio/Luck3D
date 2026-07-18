#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Asset/AssetType.h"
#include "Lucky/Asset/AssetHandle.h"
#include "Lucky/Renderer/Texture.h"

#include <filesystem>
#include <functional>

namespace Lucky
{
    /// <summary>
    /// 目录树节点缓存
    /// </summary>
    struct DirectoryNode
    {
        std::string Name;                           // 目录名
        std::filesystem::path FullPath;             // 完整路径
        std::vector<DirectoryNode> SubDirectories;  // 子目录
    };

    /// <summary>
    /// 右键上下文类别：描述本次 DrawAssetContextMenu 面向的对象类型
    /// Directory：目录（左树 / 右侧文件夹项）
    /// Asset：已注册资产
    /// EmptySpace：右侧内容区空白位置
    /// </summary>
    enum class AssetContextKind : uint8_t
    {
        Directory,
        Asset,
        EmptySpace
    };

    /// <summary>
    /// 资产面板右键上下文
    /// 统一封装：命中类型 / 命中路径 / 命中 Handle / Create 目标目录
    /// </summary>
    struct AssetContext
    {
        AssetContextKind Kind = AssetContextKind::EmptySpace;
        std::filesystem::path Path;         // Directory / Asset 有效，EmptySpace 为空
        AssetHandle Handle;                 // 仅 Asset 有效
        std::filesystem::path TargetDir;    // Create 类操作的落点目录（构造时按 Kind 一次算好）
    };

    /// <summary>
    /// 项目资产面板
    /// </summary>
    class ProjectAssetsPanel : public EditorPanel
    {
    public:
        ProjectAssetsPanel();
        ~ProjectAssetsPanel() override = default;
        
        void OnUpdate(DeltaTime dt) override;
        void OnGUI() override;
        
        void OnEvent(Event& event) override;
    private:
        // ---- 绘制 ----
        void DrawToolbar();
        void DrawDirectoryTreeNode(DirectoryNode& node);
        void DrawContentArea();
        void DrawAssetItem(const std::filesystem::directory_entry& entry);
        void DrawAssetContextMenu(const AssetContext& ctx);

        // ---- 上下文构造 ----
        AssetContext MakeContext(AssetContextKind kind, const std::filesystem::path& path, AssetHandle handle) const;

        // ---- 导航 / 刷新 ----
        void NavigateTo(const std::filesystem::path& directory);

        /// <summary>
        /// 请求刷新：调用 AssetManager::Refresh() 并重建目录树
        /// 由 Ctrl+R 快捷键、顶部刷新按钮共同触发
        /// </summary>
        void OnRefreshRequested();

        void RebuildDirectoryTree();
        DirectoryNode BuildDirectoryNode(const std::filesystem::path& path);

        // ---- CRUD ----
        void CreateFolderAt(const std::filesystem::path& parentDir);
        void CreateMaterialAt(const std::filesystem::path& parentDir);
        void CreateSceneAt(const std::filesystem::path& parentDir);
        void DeleteFolderRecursive(const std::filesystem::path& dir);

        // ---- 工具 ----
        static std::filesystem::path MakeUniquePath(const std::filesystem::path& baseDir, const std::string& stem, const std::string& ext);

        // ---- 延迟执行队列 ----

        /// <summary>
        /// 将一个写操作（会修改目录树 / Registry 的行为）延迟到当帧 UI 遍历完成后执行
        /// 必须在菜单回调中使用，避免在递归遍历 m_RootNode 时重建导致迭代器/引用悬空
        /// </summary>
        void EnqueueAction(std::function<void()> action);

        /// <summary>
        /// 在帧末（所有面板绘制结束后）依次执行队列中的写操作，并清空队列
        /// </summary>
        void FlushPendingActions();

        // ---- 缩略图 / 类型识别 ----
        Ref<Texture2D> GetThumbnail(const std::filesystem::path& filepath);
        AssetType GetAssetTypeFromPath(const std::filesystem::path& filepath) const;
    private:
        std::filesystem::path m_AssetsDirectory;    // Assets 根目录
        std::filesystem::path m_CurrentDirectory;   // 当前浏览目录
        
        DirectoryNode m_RootNode;                   // 目录树缓存
        
        float m_TreePanelWidth = 200.0f;            // 目录树宽度

        bool m_IsFocused = false;                   // 当前帧面板是否处于聚焦（由 OnGUI 更新，供 OnEvent 判定快捷键作用域）

        std::vector<std::function<void()>> m_PendingActions;    // 延迟到帧末执行的写操作（避免 UI 遍历中重建目录树导致悬空）
    };
}
