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
#include "Lucky/Scene/Scene.h"
#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/Shader.h"
#include "Lucky/Utils/PlatformUtils.h"

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

        // ---- 帧末：执行当帧入队的写操作 ----
        // 任何会重建 m_RootNode / 修改 Registry / 修改 m_CurrentDirectory 的操作均统一在此处执行，
        // 避免在 DrawDirectoryTreeNode 递归遍历 SubDirectories 时悬空迭代器
        FlushPendingActions();
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
            // Ctrl+R 触发 Refresh（入队到帧末执行，保持与菜单 Refresh 一致的安全语义）
            if (e.GetKeyCode() == Key::R && Input::IsKeyPressed(Key::LeftControl))
            {
                EnqueueAction([this]() { OnRefreshRequested(); });
                return true;
            }

            return false;
        });
    }

    void ProjectAssetsPanel::DrawToolbar()
    {
        // 刷新按钮（等价于 Ctrl+R，入队到帧末执行）
        if (UI::Button("Refresh"))
        {
            EnqueueAction([this]() { OnRefreshRequested(); });
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

        // ---- 右键菜单（必须紧跟 BeginTreeNode，作用于上一个 item） ----
        {
            std::string popupID = std::format("##DirTreePopup_{}", node.FullPath.generic_string());
            if (UI::BeginPopupContextItem(popupID.c_str(), ImGuiPopupFlags_MouseButtonRight))
            {
                SelectionManager::SelectFolder(node.FullPath);   // 右键即选中
                AssetContext ctx = MakeContext(AssetContextKind::Directory, node.FullPath, AssetHandle{});
                DrawAssetContextMenu(ctx);
                UI::EndPopup();
            }
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

        // ---- 空白区右键：NoOpenOverItems 保证不与 item 菜单打架 ----
        if (UI::BeginPopupContextWindow("##ContentAreaEmptyPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            SelectionManager::Deselect();   // 空白右键：清空选中，与"空白左键"语义一致
            AssetContext ctx = MakeContext(AssetContextKind::EmptySpace, std::filesystem::path{}, AssetHandle{});
            DrawAssetContextMenu(ctx);
            UI::EndPopup();
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

        // ---- 右键上下文菜单：目录 / 资产 分别挂，走统一菜单实现 ----
        if (isDirectory)
        {
            if (UI::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight))
            {
                SelectionManager::SelectFolder(path);
                AssetContext ctx = MakeContext(AssetContextKind::Directory, path, AssetHandle{});
                DrawAssetContextMenu(ctx);
                UI::EndPopup();
            }
        }
        else if (assetHandle.IsValid())
        {
            if (UI::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight))
            {
                SelectionManager::SelectAsset(assetHandle);
                AssetContext ctx = MakeContext(AssetContextKind::Asset, path, assetHandle);
                DrawAssetContextMenu(ctx);
                UI::EndPopup();
            }
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

    void ProjectAssetsPanel::DrawAssetContextMenu(const AssetContext& ctx)
    {
        // ---- Create 子菜单：三种上下文都可用 ----
        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Folder"))
            {
                std::filesystem::path targetDir = ctx.TargetDir;
                EnqueueAction([this, targetDir]() { CreateFolderAt(targetDir); });
            }
            if (ImGui::MenuItem("Material"))
            {
                std::filesystem::path targetDir = ctx.TargetDir;
                EnqueueAction([this, targetDir]() { CreateMaterialAt(targetDir); });
            }
            if (ImGui::MenuItem("Scene"))
            {
                std::filesystem::path targetDir = ctx.TargetDir;
                EnqueueAction([this, targetDir]() { CreateSceneAt(targetDir); });
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        // ---- Show in Explorer：所有上下文都可用（不修改目录树，无需入队） ----
        if (ImGui::MenuItem("Show in Explorer"))
        {
            std::filesystem::path target = (ctx.Kind == AssetContextKind::EmptySpace) ? m_CurrentDirectory : ctx.Path;
            PlatformShell::RevealInExplorer(target);
        }

        ImGui::Separator();

        // ---- Rename（预留，第一阶段灰置） ----
        ImGui::BeginDisabled(true);
        ImGui::MenuItem("Rename", "F2");
        ImGui::EndDisabled();

        // ---- Delete：Directory / Asset 可用（入队到帧末执行） ----
        bool canDelete = (ctx.Kind == AssetContextKind::Directory) || (ctx.Kind == AssetContextKind::Asset);
        ImGui::BeginDisabled(!canDelete);
        if (ImGui::MenuItem("Delete", "Del"))
        {
            AssetContext captured = ctx;   // 拷贝一份，ctx 是临时引用，lambda 列后每帧执行时已失效
            EnqueueAction([this, captured]()
            {
                if (captured.Kind == AssetContextKind::Asset)
                {
                    if (AssetManager::DeleteAsset(captured.Handle))
                    {
                        SelectionManager::Deselect();
                        RebuildDirectoryTree();
                    }
                }
                else if (captured.Kind == AssetContextKind::Directory)
                {
                    DeleteFolderRecursive(captured.Path);
                    SelectionManager::Deselect();
                    RebuildDirectoryTree();
                }
            });
        }
        ImGui::EndDisabled();

        ImGui::Separator();

        // ---- Refresh：所有上下文都可用（入队，等价 Ctrl+R） ----
        if (ImGui::MenuItem("Refresh", "Ctrl+R"))
        {
            EnqueueAction([this]() { OnRefreshRequested(); });
        }
    }

    AssetContext ProjectAssetsPanel::MakeContext(AssetContextKind kind, const std::filesystem::path& path, AssetHandle handle) const
    {
        AssetContext ctx;
        ctx.Kind = kind;
        ctx.Path = path;
        ctx.Handle = handle;

        switch (kind)
        {
            case AssetContextKind::Directory:
            {
                ctx.TargetDir = path;
                break;
            }
            case AssetContextKind::Asset:
            {
                ctx.TargetDir = path.parent_path();
                break;
            }
            case AssetContextKind::EmptySpace:
            {
                ctx.TargetDir = m_CurrentDirectory.empty() ? m_AssetsDirectory : m_CurrentDirectory;
                break;
            }
        }
        return ctx;
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

    // ======== CRUD ========

    void ProjectAssetsPanel::CreateFolderAt(const std::filesystem::path& parentDir)
    {
        std::filesystem::path target = MakeUniquePath(parentDir, "New Folder", "");

        std::error_code ec;
        std::filesystem::create_directory(target, ec);
        if (ec)
        {
            LF_CORE_ERROR("ProjectAssetsPanel::CreateFolderAt - Failed to create '{0}': {1}", target.generic_string(), ec.message());
            return;
        }

        LF_CORE_INFO("ProjectAssetsPanel::CreateFolderAt - Created '{0}'", target.generic_string());
        RebuildDirectoryTree();

        // 与 Unity 一致：创建后自动选中新建的 Folder，方便立即 Rename
        // 只写入 SelectionManager（右侧内容区据此高亮），不修改 m_CurrentDirectory，保持左侧目录树浏览状态不变
        SelectionManager::SelectFolder(target);
    }

    void ProjectAssetsPanel::CreateMaterialAt(const std::filesystem::path& parentDir)
    {
        std::filesystem::path target = MakeUniquePath(parentDir, "New Material", ".lmat");

        // 使用 Standard Shader 作为默认 Shader
        const Ref<ShaderLibrary>& shaderLib = Renderer3D::GetShaderLibrary();
        Ref<Shader> standardShader = shaderLib->Get("Standard");

        std::string materialName = target.stem().string();
        Ref<Material> material = CreateRef<Material>(materialName, standardShader);

        AssetHandle handle = AssetManager::CreateAsset(material, target.generic_string());
        if (!handle.IsValid())
        {
            LF_CORE_ERROR("ProjectAssetsPanel::CreateMaterialAt - Failed to create material at '{0}'", target.generic_string());
            return;
        }

        LF_CORE_INFO("ProjectAssetsPanel::CreateMaterialAt - Created '{0}' (handle {1})", target.generic_string(), static_cast<uint64_t>(handle));
        RebuildDirectoryTree();
        SelectionManager::SelectAsset(handle);   // 与 Unity 一致：创建后自动选中
    }

    void ProjectAssetsPanel::CreateSceneAt(const std::filesystem::path& parentDir)
    {
        std::filesystem::path target = MakeUniquePath(parentDir, "New Scene", ".luck3d");

        Ref<Scene> scene = CreateRef<Scene>();
        scene->SetName(target.stem().string());

        AssetHandle handle = AssetManager::CreateAsset(scene, target.generic_string());
        if (!handle.IsValid())
        {
            LF_CORE_ERROR("ProjectAssetsPanel::CreateSceneAt - Failed to create scene at '{0}'", target.generic_string());
            return;
        }

        LF_CORE_INFO("ProjectAssetsPanel::CreateSceneAt - Created '{0}' (handle {1})", target.generic_string(), static_cast<uint64_t>(handle));
        RebuildDirectoryTree();
        SelectionManager::SelectAsset(handle);
    }

    void ProjectAssetsPanel::DeleteFolderRecursive(const std::filesystem::path& dir)
    {
        if (!std::filesystem::exists(dir))
        {
            return;
        }

        // ---- 1. 先按 Registry 级联删除资产（避免 Registry 悬空条目） ----
        std::vector<AssetHandle> toDelete;
        for (auto& entry : std::filesystem::recursive_directory_iterator(dir))
        {
            if (entry.is_directory())
            {
                continue;
            }

            AssetHandle handle = AssetManager::GetAssetHandle(entry.path().generic_string());
            if (handle.IsValid())
            {
                toDelete.push_back(handle);
            }
        }

        for (AssetHandle handle : toDelete)
        {
            AssetManager::DeleteAsset(handle);
        }

        // ---- 2. 兜底：删除目录本身（含未识别文件） ----
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
        if (ec)
        {
            LF_CORE_ERROR("ProjectAssetsPanel::DeleteFolderRecursive - Failed to remove '{0}': {1}", dir.generic_string(), ec.message());
            return;
        }

        LF_CORE_INFO("ProjectAssetsPanel::DeleteFolderRecursive - Removed '{0}' (cascaded {1} assets)", dir.generic_string(), toDelete.size());

        // ---- 3. m_CurrentDirectory 回退保护 ----
        if (!std::filesystem::exists(m_CurrentDirectory))
        {
            m_CurrentDirectory = m_AssetsDirectory;
        }
    }

    std::filesystem::path ProjectAssetsPanel::MakeUniquePath(const std::filesystem::path& baseDir, const std::string& stem, const std::string& ext)
    {
        std::filesystem::path candidate = baseDir / (stem + ext);
        if (!std::filesystem::exists(candidate))
        {
            return candidate;
        }

        int index = 1;
        while (true)
        {
            std::string name = std::format("{} {}{}", stem, index, ext);
            candidate = baseDir / name;
            if (!std::filesystem::exists(candidate))
            {
                return candidate;
            }
            ++index;
        }
    }

    // ======== 延迟执行队列 ========

    void ProjectAssetsPanel::EnqueueAction(std::function<void()> action)
    {
        if (action)
        {
            m_PendingActions.push_back(std::move(action));
        }
    }

    void ProjectAssetsPanel::FlushPendingActions()
    {
        if (m_PendingActions.empty())
        {
            return;
        }

        // 取到本地后再逐一执行：防止执行中途回弹新的 EnqueueAction 无限循环，
        // 新入的队列项会留到下一帧才执行
        std::vector<std::function<void()>> actions;
        actions.swap(m_PendingActions);

        for (const std::function<void()>& action : actions)
        {
            action();
        }
    }
}
