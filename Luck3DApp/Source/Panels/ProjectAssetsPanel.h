#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Asset/AssetType.h"
#include "Lucky/Renderer/Texture.h"

#include <filesystem>

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
        void DrawDirectoryTreeNode(DirectoryNode& node);
        void DrawContentArea();
        void DrawAssetItem(const std::filesystem::directory_entry& entry);
        
        void NavigateTo(const std::filesystem::path& directory);
        void Refresh();
        void RebuildDirectoryTree();
        DirectoryNode BuildDirectoryNode(const std::filesystem::path& path);
        Ref<Texture2D> GetThumbnail(const std::filesystem::path& filepath);
        AssetType GetAssetTypeFromPath(const std::filesystem::path& filepath) const;
    private:
        std::filesystem::path m_AssetsDirectory;    // Assets 根目录
        std::filesystem::path m_CurrentDirectory;   // 当前浏览目录
        
        DirectoryNode m_RootNode;                   // 目录树缓存
        
        float m_TreePanelWidth = 200.0f;            // 目录树宽度
        
        std::filesystem::path m_SelectionPath;      // 当前选中文件路径
    };
}
