#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Asset/AssetType.h"
#include "Lucky/Asset/AssetHandle.h"
#include "Lucky/Renderer/Texture.h"
#include "Lucky/UI/Widgets.h"

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

        // ---- 内联重命名作用域标签（用于 UI::RenameController 区分同一 TId 出现在多个视图的场景） ----
        // 0 保留给"不区分作用域"（BeginEditing 默认值），非 0 值由本面板自定义：
        // - kScopeDirTree：左侧目录树
        // - kScopeContent：右侧内容区（列表 / 未来的 Grid）
        static constexpr int kScopeDirTree = 1;
        static constexpr int kScopeContent = 2;

        // ---- CRUD ----
        void DeleteFolderRecursive(const std::filesystem::path& dir);

        /// <summary>
        /// 重命名目录：磁盘 rename + 该目录下所有资产的 Registry 路径级联更新 + 目录树重建
        /// - Handle 保持不变（AssetManager::UpdateAssetPath 只改 Registry 记录）
        /// - 跨资产引用（Scene 中的 Mesh/Material Handle）不断裂
        /// - m_CurrentDirectory 若在被改名子树内，同步纠正到新路径
        /// </summary>
        /// <param name="oldPath">旧目录路径（相对项目根）</param>
        /// <param name="newName">新目录名（不含分隔符与扩展名）</param>
        void RenameFolderTo(const std::filesystem::path& oldPath, const std::string& newName);

        /// <summary>
        /// 重命名资产：走 AssetManager::MoveAsset（磁盘 rename + Registry 更新），Handle 保持不变
        /// 扩展名保留原样，仅替换 stem
        /// </summary>
        /// <param name="handle">资产 Handle</param>
        /// <param name="newStem">新的资产名 stem（不含扩展名）</param>
        void RenameAssetTo(AssetHandle handle, const std::string& newStem);

        // ---- Pending Create（对齐 Unity 的"先占位、提交后落盘"语义） ----

        /// <summary>
        /// 待定创建类型：右键 Create → 立即进入 Rename 编辑态，此时磁盘尚未落盘；
        /// 用户提交名称后（含空字符串 = 用默认名）才真正调用 CommitCreateXxx；
        /// 按 Esc 或跨面板 Cancel 则完全丢弃，不产生磁盘 IO
        /// </summary>
        enum class PendingCreateKind : uint8_t
        {
            None = 0,
            Folder,
            Material,
            Scene
        };

        /// <summary>
        /// Pending create 完整状态（跨帧稳定，由面板持有）
        /// - VirtualPath 是绝对不会与磁盘真实路径冲突的临时占位（附加 "##pending" 后缀），
        ///   作为 UI::RenameController 的 TId 使用
        /// </summary>
        struct PendingCreateState
        {
            PendingCreateKind Kind = PendingCreateKind::None;
            std::filesystem::path ParentDir;    // 父目录
            std::string InitialName;            // 默认名（"New Folder" / "New Material" / "New Scene"）
            std::string Extension;              // "" / ".lmat" / ".luck3d"
            int         ScopeTag = 0;           // 归属作用域（kScopeDirTree / kScopeContent）
            std::filesystem::path VirtualPath;  // 占位路径（ParentDir/(InitialName + "##pending")）
        };

        /// <summary>
        /// 触发"新建 Folder"编辑态：不落盘，只登记 PendingCreate + 进入 RenameController 编辑态
        /// </summary>
        void BeginCreateFolder(const std::filesystem::path& parentDir, int scopeTag);
        void BeginCreateMaterial(const std::filesystem::path& parentDir, int scopeTag);
        void BeginCreateScene(const std::filesystem::path& parentDir, int scopeTag);

        /// <summary>
        /// 提交 pending create：用 stem 落盘（stem 为空时回退到 InitialName），并进行 uniquify 保证不冲突
        /// </summary>
        void CommitCreateFolder(const std::filesystem::path& parentDir, const std::string& stem);
        void CommitCreateMaterial(const std::filesystem::path& parentDir, const std::string& stem);
        void CommitCreateScene(const std::filesystem::path& parentDir, const std::string& stem);

        /// <summary>
        /// 丢弃当前 pending create（不落盘）
        /// </summary>
        void CancelPendingCreate();

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

        /// <summary>
        /// 内联重命名状态机：同时用于左侧目录树、右侧资产项、右侧目录项
        /// TId 选择 std::filesystem::path，覆盖"目录 / 资产（以路径为主键）"两类节点
        /// 提交回调按目标类型分派到 RenameFolderTo / RenameAssetTo / CommitCreateXxx
        /// 左右两侧通过 scopeTag（kScopeDirTree / kScopeContent）隔离，避免同一 path 在两处同时进入编辑态
        /// </summary>
        UI::RenameController<std::filesystem::path> m_Rename;

        /// <summary>
        /// 当前待定创建状态（对齐 Unity："右键 Create → 显示占位编辑框 → 提交才落盘"）
        /// Kind == None 表示无待定项；渲染时若父目录 + 作用域匹配则在该处插入占位节点
        /// </summary>
        PendingCreateState m_PendingCreate;
    };
}
