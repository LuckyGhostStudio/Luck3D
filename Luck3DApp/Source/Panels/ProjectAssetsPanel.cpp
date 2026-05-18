#include "ProjectAssetsPanel.h"

#include "Lucky/UI/DrawUtils.h"
#include "Lucky/UI/Widgets.h"
#include "Lucky/UI/ScopedGuards.h"

#include "imgui/imgui.h"
#include "Lucky/Asset/AssetManager.h"

namespace Lucky
{
    ProjectAssetsPanel::ProjectAssetsPanel()
    {
        SetFlags(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);    // 禁用滚动条
        
        m_AssetsDirectory = "Assets";
        m_CurrentDirectory = m_AssetsDirectory;
        
        // 初始构建目录树
        RebuildDirectoryTree();
    }

    void ProjectAssetsPanel::OnUpdate(DeltaTime dt)
    {
        
    }

    void ProjectAssetsPanel::OnGUI()
    {
        if (ImGui::BeginTable("##ProjectAssets Table", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadInnerX))
        {
            float panelWidth = ImGui::GetContentRegionAvail().x;
            m_TreePanelWidth = panelWidth * 0.3f;
            ImGui::TableSetupColumn("Categories Column", 0, m_TreePanelWidth);
            ImGui::TableSetupColumn("Content Column", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            // 左侧：目录树
            ImGui::BeginChild("##DirectoryTree", { 0, 0 });
            {
                DrawDirectoryTreeNode(m_RootNode);

                // 点击鼠标 && 鼠标悬停在该窗口（点击空白位置）
                if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered())
                {
                    m_CurrentDirectory.clear();  // 取消选中：置空当前浏览目录
                }
            }
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(1);

            // 右侧：内容区
            ImGui::BeginChild("##ContentArea", { 0, 0 });
            {
                DrawContentArea();

                // 点击鼠标 && 鼠标悬停在该窗口（点击空白位置）
                if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered())
                {
                    m_SelectionPath.clear();  // 取消选中：置空当前选中文件路径
                }
            }
            ImGui::EndChild();

            ImGui::EndTable();
        }
    }

    void ProjectAssetsPanel::OnEvent(Event& event)
    {
        
    }

    void ProjectAssetsPanel::DrawDirectoryTreeNode(DirectoryNode& node)
    {
        const std::string& strID = node.Name;
        bool isRoot = node.FullPath == m_AssetsDirectory;    // 根目录默认展开
        bool isLeaf = node.SubDirectories.empty();

        if (isRoot)
        {
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);    // TODO 封装 Fonts
        }
        
        bool opened = UI::BeginTreeNode(strID.c_str(), isRoot, m_CurrentDirectory == node.FullPath, isLeaf);
        
        if (isRoot)
        {
            ImGui::PopFont();
        }
        
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            NavigateTo(node.FullPath);
        }
        
        if (opened)
        {
            for (DirectoryNode& subDir : node.SubDirectories)
            {
                DrawDirectoryTreeNode(subDir);
            }
            
            UI::EndTreeNode();
        }
    }

    void ProjectAssetsPanel::DrawContentArea()
    {
        if (m_CurrentDirectory.empty())
        {
            return;
        }
        
        for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
        {
            DrawAssetItem(entry);
        }
    }

    void ProjectAssetsPanel::DrawAssetItem(const std::filesystem::directory_entry& entry)
    {
        const std::filesystem::path& path = entry.path();
        std::string strID = path.stem().string();
        
        bool isDirectory = entry.is_directory();
        
        if (!isDirectory)
        {
            AssetHandle assetHandel = AssetManager::GetAssetHandle(path.string());
            strID = std::format("{}##{}", path.stem().string(), static_cast<uint32_t>(assetHandel));
        }
        
        if (UI::BeginTreeNode(strID.c_str(), false, m_SelectionPath == path, true))
        {
            UI::EndTreeNode();
        }
        
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            m_SelectionPath = path;
        }
    }
    
    void ProjectAssetsPanel::NavigateTo(const std::filesystem::path& directory)
    {
        if (std::filesystem::exists(directory) && std::filesystem::is_directory(directory))
        {
            m_CurrentDirectory = directory;
            m_SelectionPath.clear();
        }
    }

    void ProjectAssetsPanel::Refresh()
    {
        RebuildDirectoryTree();
        
        if (!std::filesystem::exists(m_CurrentDirectory))
        {
            m_CurrentDirectory = m_AssetsDirectory;
        }
    }

    void ProjectAssetsPanel::RebuildDirectoryTree()
    {
        m_RootNode = BuildDirectoryNode(m_AssetsDirectory);
    }

    DirectoryNode ProjectAssetsPanel::BuildDirectoryNode(const std::filesystem::path& path)
    {
        DirectoryNode node;
        node.Name = path.filename().string();
        node.FullPath = path;
        
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
        {
            for (auto& entry : std::filesystem::directory_iterator(path))
            {
                if (entry.is_directory())
                {
                    node.SubDirectories.push_back(BuildDirectoryNode(entry.path()));
                }
            }
            
            // 按名称排序
            std::sort(node.SubDirectories.begin(), node.SubDirectories.end(), [](const DirectoryNode& a, const DirectoryNode& b)
            {
                return a.Name < b.Name;
            });
        }
        
        return node;
    }

    Ref<Texture2D> ProjectAssetsPanel::GetThumbnail(const std::filesystem::path& filepath)
    {
        return nullptr;
    }

    AssetType ProjectAssetsPanel::GetAssetTypeFromPath(const std::filesystem::path& filepath) const
    {
        std::string ext = filepath.extension().string();
        return GetAssetTypeFromExtension(ext);
    }
}
