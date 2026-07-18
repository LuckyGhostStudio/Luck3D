#include "ProjectAssetsPanel.h"

#include "Lucky/UI/DrawUtils.h"
#include "Lucky/UI/Widgets.h"
#include "Lucky/UI/ScopedGuards.h"
#include "Lucky/UI/Controls.h"

#include "Lucky/Asset/AssetManager.h"
#include "Lucky/Editor/EditorIconManager.h"
#include "Lucky/Editor/DragDropPayloads.h"
#include "Lucky/Editor/DragDropContext.h"
#include "Lucky/Scene/SelectionManager.h"

#include "Lucky/Core/Events/KeyEvent.h"
#include "Lucky/Core/Input/Input.h"
#include "Lucky/Core/Input/KeyCodes.h"

#include "imgui/imgui.h"

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
        // 更新窗口聚焦状态（供 OnEvent 中 Ctrl+R 判定作用域）
        m_IsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        // 顶部工具栏（刷新按钮）
        DrawToolbar();

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
                    SelectionManager::Deselect();
                }
            }
            ImGui::EndChild();

            ImGui::EndTable();
        }
    }

    void ProjectAssetsPanel::OnEvent(Event& event)
    {
        if (!m_IsFocused)
        {
            return;
        }

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) -> bool
        {
            // Ctrl+R 触发 Refresh
            if (e.GetKeyCode() == Key::R && Input::IsKeyPressed(Key::LeftControl))
            {
                OnRefreshRequested();
                return true;
            }

            return false;
        });
    }

    void ProjectAssetsPanel::DrawToolbar()
    {
        // 刷新按钮（等价于 Ctrl+R）
        if (UI::Button("Refresh"))
        {
            OnRefreshRequested();
        }

        UI::Draw::HorizontalLine();
    }

    void ProjectAssetsPanel::DrawDirectoryTreeNode(DirectoryNode& node)
    {
        const std::string& strID = node.Name;
        bool isRoot = node.FullPath == m_AssetsDirectory;    // 根目录默认展开
        bool isLeaf = node.SubDirectories.empty();

        bool isCurrentDir = (m_CurrentDirectory == node.FullPath);

        const Ref<Texture2D>& folderClosedIcon = EditorIconManager::GetFolderIcon(false);
        const Ref<Texture2D>& folderOpenIcon = EditorIconManager::GetFolderIcon(true);

        if (isRoot)
        {
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);    // TODO 封装 Fonts
        }
        
        bool opened = UI::BeginTreeNode(folderClosedIcon, folderOpenIcon, strID.c_str(), isRoot, isCurrentDir, isLeaf);
        
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

        // 获取图标
        const Ref<Texture2D>& icon = isDirectory ? EditorIconManager::GetFolderIcon(false) : EditorIconManager::GetAssetTypeIcon(GetAssetTypeFromPath(path));
        
        // 提前获取资产 Handle（使用 generic_string 与 AssetRegistry 存储格式一致，避免 Windows 反斜杠不匹配）
        AssetHandle assetHandle;
        if (!isDirectory)
        {
            assetHandle = AssetManager::GetAssetHandle(path.generic_string());
            strID = std::format("{}##{}", path.stem().string(), static_cast<uint32_t>(assetHandle));
        }

        bool isSelected = false;
        if (isDirectory)
        {
            isSelected = SelectionManager::IsFolderSelected(path);
        }
        else if (assetHandle.IsValid())
        {
            isSelected = SelectionManager::IsAssetSelected(assetHandle);
        }

        // ---- 普通态：TreeNode + 拖拽源 + 右键菜单 ----
        if (UI::BeginTreeNode(icon, strID.c_str(), false, isSelected, true))
        {
            UI::EndTreeNode();
        }
        
        // 拖拽源：非目录且资产已注册（handle 有效）时才作为拖拽源
        if (!isDirectory && assetHandle.IsValid() && UI::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            ImGui::SetDragDropPayload(DragDrop::AssetHandle, &assetHandle, sizeof(AssetHandle));
            
            UI::DragDropPreview(UI::DragDropContext::IsRejected(DragDrop::AssetHandle));
            
            UI::EndDragDropSource();
        }

        // 右键上下文菜单（Delete）：仅对已注册的资产项启用
        if (!isDirectory && assetHandle.IsValid())
        {
            DrawAssetContextMenu(assetHandle);
        }
        
        // 选中触发时机：鼠标"抬起"时（且期间未发生拖拽），避免 MouseDown 抢占拖拽源
        // 语义参考 Unity：按下不切换 Selection；若发生拖拽则不选中；仅在正常点击（按下+抬起，未拖）时提交选中
        bool itemHovered = ImGui::IsItemHovered();
        bool leftReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        bool wasDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
        if (itemHovered && leftReleased && !wasDragging && !ImGui::IsItemToggledOpen())
        {
            // 目录：内容区内单击选中（写入 SelectionManager::Folder）
            //       不切换浏览目录，已与左侧目录树的 NavigateTo 行为分离
            // 资产：写入全局 SelectionManager，Inspector 面板会据此显示对应资产信息
            if (isDirectory)
            {
                SelectionManager::SelectFolder(path);
            }
            else if (assetHandle.IsValid())
            {
                SelectionManager::SelectAsset(assetHandle);
            }
        }
    }

    void ProjectAssetsPanel::DrawAssetContextMenu(AssetHandle assetHandle)
    {
        if (!UI::BeginPopupContextItem())
        {
            return;
        }

        // 与 SceneHierarchyPanel 一致：弹出菜单时先选中该 Asset，Inspector 同步显示对应资产
        SelectionManager::SelectAsset(assetHandle);

        if (ImGui::MenuItem("Delete"))
        {
            if (AssetManager::DeleteAsset(assetHandle))
            {
                SelectionManager::Deselect();
                RebuildDirectoryTree();
            }
            ImGui::CloseCurrentPopup();
        }

        UI::EndPopup();
    }
    
    void ProjectAssetsPanel::NavigateTo(const std::filesystem::path& directory)
    {
        if (std::filesystem::exists(directory) && std::filesystem::is_directory(directory))
        {
            m_CurrentDirectory = directory;
            
            // 切换浏览目录：若当前选中的是 Asset / Folder，则清空（选中项已不在可见范围内）
            SelectionType currType = SelectionManager::GetSelectionType();
            if (currType == SelectionType::Asset || currType == SelectionType::Folder)
            {
                SelectionManager::Deselect();
            }
        }
    }

    void ProjectAssetsPanel::OnRefreshRequested()
    {
        RefreshResult result = AssetManager::Refresh();

        RebuildDirectoryTree();

        if (!std::filesystem::exists(m_CurrentDirectory))
        {
            m_CurrentDirectory = m_AssetsDirectory;
        }

        if (result.Added > 0 || result.Removed > 0)
        {
            LF_CORE_INFO("ProjectAssetsPanel::Refresh - {0} added, {1} removed, {2} total.", result.Added, result.Removed, result.Total);
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
