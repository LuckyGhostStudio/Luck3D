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
                // 排除 IsAnyItemHovered：避免 InputText 内部点击也走空白分支导致编辑态被误清
                if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
                {
                    m_CurrentDirectory.clear();  // 取消选中：置空当前浏览目录
                    m_Rename.CancelAll();        // 同步退出内联重命名编辑态
                    CancelPendingCreate();       // 同步丢弃 pending create（不落盘）
                }
            }
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(1);

            // 右侧：内容区
            ImGui::BeginChild("##ContentArea", { 0, 0 });
            {
                DrawContentArea();

                // 点击鼠标 && 鼠标悬停在该窗口（点击空白位置）
                if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
                {
                    SelectionManager::Deselect();
                    m_Rename.CancelAll();
                    CancelPendingCreate();
                }
            }
            ImGui::EndChild();

            ImGui::EndTable();
        }

        // ---- 帧末：内联重命名两阶段仲裁（对齐 Unity 表现）----
        // 语义详见 UI::RenameController::FlushPending 注释：
        // - 按下-抬起期间发生拖拽 → 取消候选
        // - 抬起时候选仍存在 → 正式进入编辑态
        m_Rename.FlushPending();

        // ---- 帧末：处理"控制流未走到 DrawInlineInputIfEditing 的 Cancel"场景 ----
        // 例如：占位节点因 m_CurrentDirectory 切换后本帧不再被绘制，DrawInlineInputIfEditing
        // 未被调用 → onCancel 无从触发。此时若 NotifyClicked / CancelIfEditing / CancelAll 中
        // 任一路径把 m_CancelledThisFrame 置 true，此处统一丢弃 pending create 保底
        if (m_Rename.ConsumeCancelledFlag() && m_PendingCreate.Kind != PendingCreateKind::None)
        {
            CancelPendingCreate();
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

            // F2：对当前选中的 Folder / Asset 触发内联重命名（对齐 Unity 表现）
            // scopeTag 归 kScopeContent：F2 属于面板级快捷键，默认作用于右侧内容区（与 Unity 一致）
            if (e.GetKeyCode() == Key::F2)
            {
                SelectionType selType = SelectionManager::GetSelectionType();
                if (selType == SelectionType::Folder)
                {
                    std::filesystem::path folder = SelectionManager::GetSelectedFolder();
                    if (!folder.empty() && folder != m_AssetsDirectory)
                    {
                        m_Rename.BeginEditing(folder, folder.filename().string(), kScopeContent);
                        return true;
                    }
                }
                else if (selType == SelectionType::Asset)
                {
                    AssetHandle handle = AssetHandle(SelectionManager::GetSelection());
                    if (handle.IsValid())
                    {
                        const std::string& filePath = AssetManager::GetAssetFilePath(handle);
                        std::filesystem::path path(filePath);
                        m_Rename.BeginEditing(path, path.stem().string(), kScopeContent);
                        return true;
                    }
                }
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

        // 拷贝一份路径，避免 Rebuild 后 node 引用悬空（延迟执行的 lambda 需要）
        std::filesystem::path pathCopy = node.FullPath;
        std::string nameCopy = node.Name;

        UI::RenameClickOutcome clickOutcome = UI::RenameClickOutcome::None;
        bool opened = UI::BeginRenamableTreeNode(
            folderClosedIcon, folderOpenIcon,
            strID.c_str(), nameCopy, node.FullPath,
            isCurrentDir, isLeaf, /*defaultOpen*/ isRoot,
            m_Rename,
            [this, pathCopy](const std::string& newName)
            {
                EnqueueAction([this, pathCopy, newName]() { RenameFolderTo(pathCopy, newName); });
            },
            /*drawRightSide*/ nullptr,   // 目录树无右侧图标，适配器默认按 TreeNode ItemRect 上报命中矩形
            "##DirTreeRename",
            &clickOutcome,
            /*scopeTag*/ kScopeDirTree);

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

        // 处理点击：ShouldSelect 走 NavigateTo；RegisteredAsRenameCandidate 不做任何事（等抬起进入编辑态）
        // 排除 IsItemToggledOpen：点击箭头展开/折叠时 clickOutcome 仍可能是 ShouldSelect，但不应切换浏览目录
        if (clickOutcome == UI::RenameClickOutcome::ShouldSelect && !ImGui::IsItemToggledOpen())
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

        // ---- Pending Create 占位节点（对齐 Unity："右键 Create 后立即显示占位编辑框"）----
        // 条件：
        // - 当前存在 pending create
        // - 作用域归属 Content
        // - 父目录 == 当前浏览目录（切换目录会取消 pending，因此这里通常一定命中）
        if (m_PendingCreate.Kind != PendingCreateKind::None
            && m_PendingCreate.ScopeTag == kScopeContent
            && m_PendingCreate.ParentDir == m_CurrentDirectory)
        {
            // 图标按 Kind 分派
            AssetType pendingAssetType = AssetType::None;
            switch (m_PendingCreate.Kind)
            {
                case PendingCreateKind::Material: pendingAssetType = AssetType::Material; break;
                case PendingCreateKind::Scene:    pendingAssetType = AssetType::Scene;    break;
                default:                                                                  break;
            }
            const Ref<Texture2D>& pendingIcon = (m_PendingCreate.Kind == PendingCreateKind::Folder)
                ? EditorIconManager::GetFolderIcon(false)
                : EditorIconManager::GetAssetTypeIcon(pendingAssetType);

            // TreeNode ID：InitialName + "##pending"（保证与真实资产项 ID 隔离，不冲突）
            std::string pendingStrID = m_PendingCreate.InitialName + "##pending_create";

            // scope 值提前捕获，避免 lambda 引用捕获后被 CancelPendingCreate 重置为 0 导致失效
            PendingCreateKind        capturedKind      = m_PendingCreate.Kind;
            std::filesystem::path    capturedParentDir = m_PendingCreate.ParentDir;

            // 通过 BeginRenamableTreeNode 进入编辑态：
            // - onCommit：按 Kind 分派到 CommitCreateXxx；stem 为空 → 用 InitialName（在 Commit* 内部处理）
            // - onCancel：仅丢弃 PendingCreate 状态，不落盘（Esc / 未编辑失焦触发）
            UI::RenameClickOutcome pendingClick = UI::RenameClickOutcome::None;
            if (UI::BeginRenamableTreeNode(
                    pendingIcon, pendingStrID.c_str(), m_PendingCreate.InitialName, m_PendingCreate.VirtualPath,
                    /*selected*/ true, /*isLeaf*/ true, /*defaultOpen*/ false,
                    m_Rename,
                    [this, capturedKind, capturedParentDir](const std::string& newName)
                    {
                        // 立即清 PendingCreate 状态，避免"落盘在帧末 EnqueueAction 才执行"期间下一帧短暂闪现占位
                        // 落盘走 EnqueueAction 保证不会在 UI 遍历中重建目录树导致悬空迭代器
                        m_PendingCreate = PendingCreateState{};
                        EnqueueAction([this, capturedKind, capturedParentDir, newName]()
                        {
                            switch (capturedKind)
                            {
                                case PendingCreateKind::Folder:   CommitCreateFolder(capturedParentDir, newName);   break;
                                case PendingCreateKind::Material: CommitCreateMaterial(capturedParentDir, newName); break;
                                case PendingCreateKind::Scene:    CommitCreateScene(capturedParentDir, newName);    break;
                                default:                                                                            break;
                            }
                        });
                    },
                    /*drawRightSide*/ nullptr,
                    "##ContentPendingCreateRename",
                    &pendingClick,
                    /*scopeTag*/ kScopeContent,
                    /*onCancel*/ [this]() { m_PendingCreate = PendingCreateState{}; }))
            {
                UI::EndTreeNode();
            }
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
        std::string displayName = strID;   // 快照名（登记 pending 时使用）
        
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

        // ---- 拷贝一份用于 lambda 捕获（Rebuild 后原 entry 引用可能失效） ----
        std::filesystem::path pathCopy = path;
        AssetHandle handleCopy = assetHandle;
        bool isDirCopy = isDirectory;

        // ---- 普通态：BeginRenamableTreeNode + 拖拽源 + 右键菜单 ----
        // scopeTag 传入 kScopeContent：与左侧目录树 kScopeDirTree 隔离，避免同一 path 两处同时进入编辑态
        UI::RenameClickOutcome clickOutcome = UI::RenameClickOutcome::None;
        if (UI::BeginRenamableTreeNode(
                icon, strID.c_str(), displayName, path,
                isSelected, /*isLeaf*/ true, /*defaultOpen*/ false,
                m_Rename,
                [this, isDirCopy, pathCopy, handleCopy](const std::string& newName)
                {
                    // 目录 → RenameFolderTo；资产 → RenameAssetTo；写操作入队到帧末，避免遍历中重建目录树导致悬空
                    if (isDirCopy)
                    {
                        EnqueueAction([this, pathCopy, newName]() { RenameFolderTo(pathCopy, newName); });
                    }
                    else if (handleCopy.IsValid())
                    {
                        EnqueueAction([this, handleCopy, newName]() { RenameAssetTo(handleCopy, newName); });
                    }
                },
                /*drawRightSide*/ nullptr,
                "##AssetItemRename",
                &clickOutcome,
                /*scopeTag*/ kScopeContent))
        {
            UI::EndTreeNode();
        }

        // 编辑态下：跳过后续所有交互（拖拽 / 右键菜单 / 抬起选中）
        if (m_Rename.IsEditing(path, kScopeContent))
        {
            return;
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
        //
        // 与内联重命名的协作：
        // - clickOutcome == RegisteredAsRenameCandidate：按下瞬间已被 RenameController 登记为编辑候选，
        //   本次抬起不再做 Select（抬起后 FlushPending 会决定进入编辑态还是取消）
        // - clickOutcome == ShouldSelect：说明"未选中 or 不在名称区"，走原有抬起选中逻辑
        bool itemHovered = ImGui::IsItemHovered();
        bool leftReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        bool wasDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
        if (clickOutcome != UI::RenameClickOutcome::RegisteredAsRenameCandidate
            && itemHovered && leftReleased && !wasDragging && !ImGui::IsItemToggledOpen())
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
        // 语义（对齐 Unity）：不立即落盘，只登记 PendingCreate + 让 RenameController 进入编辑态；
        // 用户提交名称后由 CommitCreateXxx 真正落盘（提交空字符串 = 用 InitialName 落盘；Esc = 完全取消）
        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Folder"))
            {
                std::filesystem::path targetDir = ctx.TargetDir;
                EnqueueAction([this, targetDir]() { BeginCreateFolder(targetDir, kScopeContent); });
            }
            if (ImGui::MenuItem("Material"))
            {
                std::filesystem::path targetDir = ctx.TargetDir;
                EnqueueAction([this, targetDir]() { BeginCreateMaterial(targetDir, kScopeContent); });
            }
            if (ImGui::MenuItem("Scene"))
            {
                std::filesystem::path targetDir = ctx.TargetDir;
                EnqueueAction([this, targetDir]() { BeginCreateScene(targetDir, kScopeContent); });
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

        // ---- Rename：Directory / Asset 可用，直接进入内联重命名编辑态 ----
        // scopeTag 归 kScopeContent：右键菜单 Rename 属于内容区行为（右键在左树上的条目也归 Content，
        // 因为左树目前无独立右键菜单 Rename 入口）
        bool canRename = (ctx.Kind == AssetContextKind::Directory) || (ctx.Kind == AssetContextKind::Asset);
        ImGui::BeginDisabled(!canRename);
        if (ImGui::MenuItem("Rename", "F2"))
        {
            AssetContext captured = ctx;
            EnqueueAction([this, captured]()
            {
                // 目录：以完整 path 为 TId，快照名 = 目录名
                // 资产：以完整 path 为 TId，快照名 = stem（不含扩展名）
                std::string snapshotName = (captured.Kind == AssetContextKind::Directory)
                    ? captured.Path.filename().string()
                    : captured.Path.stem().string();
                m_Rename.BeginEditing(captured.Path, snapshotName, kScopeContent);
            });
        }
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
            // 只在目录真实改变时才做副作用（Deselect / Cancel Pending / Cancel Rename）
            // 若 directory == m_CurrentDirectory：视为 no-op，避免 BeginCreateXxx 内部
            // 显式调用 NavigateTo(parentDir) 时把刚建立的 PendingCreate 误清
            if (m_CurrentDirectory != directory)
            {
                m_CurrentDirectory = directory;

                // 切换浏览目录：若当前选中的是 Asset / Folder，则清空（选中项已不在可见范围内）
                SelectionType currType = SelectionManager::GetSelectionType();
                if (currType == SelectionType::Asset || currType == SelectionType::Folder)
                {
                    SelectionManager::Deselect();
                }

                // 对齐 Unity：切换目录 → 取消当前 pending create（占位不再可见，直接丢弃）
                CancelPendingCreate();

                // 切换目录 → 同步退出内联重命名编辑态
                m_Rename.CancelAll();
            }
        }
    }

    void ProjectAssetsPanel::OnRefreshRequested()
    {
        // 与 Unity 一致：Refresh 会取消当前 pending create 与内联重命名编辑态
        // 避免刷新后占位节点悬空 / 编辑态指向不存在的路径
        CancelPendingCreate();
        m_Rename.CancelAll();

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

    // ---- Pending Create（对齐 Unity："先占位、提交后落盘"） ----

    namespace
    {
        /// <summary>
        /// 生成 pending create 占位路径：ParentDir / (InitialName + "##pending")
        /// "##pending" 后缀保证不与磁盘真实路径冲突，作为 UI::RenameController 的 TId 使用
        /// </summary>
        std::filesystem::path MakeVirtualPendingPath(const std::filesystem::path& parentDir, const std::string& initialName)
        {
            return parentDir / (initialName + "##pending");
        }
    }

    void ProjectAssetsPanel::BeginCreateFolder(const std::filesystem::path& parentDir, int scopeTag)
    {
        // 先取消可能残留的 pending（例如用户连续按两次 Create 而未处理上一个）
        CancelPendingCreate();

        // 先切目录再登记：若内容区 scope 需切到新 parentDir，NavigateTo 会触发 CancelPendingCreate 副作用，
        // 必须在登记 m_PendingCreate 之前完成，避免误清刚登记的 pending
        if (scopeTag == kScopeContent)
        {
            NavigateTo(parentDir);
        }

        m_PendingCreate.Kind        = PendingCreateKind::Folder;
        m_PendingCreate.ParentDir   = parentDir;
        m_PendingCreate.InitialName = "New Folder";
        m_PendingCreate.Extension.clear();
        m_PendingCreate.ScopeTag    = scopeTag;
        m_PendingCreate.VirtualPath = MakeVirtualPendingPath(parentDir, m_PendingCreate.InitialName);

        // 进入编辑态：占位节点在下一帧渲染时插入，编辑框自动获焦 + 全选（RenameController::BeginEditing 内部处理）
        m_Rename.BeginEditing(m_PendingCreate.VirtualPath, m_PendingCreate.InitialName, scopeTag);

        // BeginEditing 内部已置 m_CancelledThisFrame = false（新会话开始），但 CancelAll 一旦先行可能设置了 CancelledThisFrame，
        // 也已在 BeginEditing 里清除。无需额外处理
    }

    void ProjectAssetsPanel::BeginCreateMaterial(const std::filesystem::path& parentDir, int scopeTag)
    {
        CancelPendingCreate();

        if (scopeTag == kScopeContent)
        {
            NavigateTo(parentDir);
        }

        m_PendingCreate.Kind        = PendingCreateKind::Material;
        m_PendingCreate.ParentDir   = parentDir;
        m_PendingCreate.InitialName = "New Material";
        m_PendingCreate.Extension   = ".lmat";
        m_PendingCreate.ScopeTag    = scopeTag;
        m_PendingCreate.VirtualPath = MakeVirtualPendingPath(parentDir, m_PendingCreate.InitialName);

        m_Rename.BeginEditing(m_PendingCreate.VirtualPath, m_PendingCreate.InitialName, scopeTag);
    }

    void ProjectAssetsPanel::BeginCreateScene(const std::filesystem::path& parentDir, int scopeTag)
    {
        CancelPendingCreate();

        if (scopeTag == kScopeContent)
        {
            NavigateTo(parentDir);
        }

        m_PendingCreate.Kind        = PendingCreateKind::Scene;
        m_PendingCreate.ParentDir   = parentDir;
        m_PendingCreate.InitialName = "New Scene";
        m_PendingCreate.Extension   = ".luck3d";
        m_PendingCreate.ScopeTag    = scopeTag;
        m_PendingCreate.VirtualPath = MakeVirtualPendingPath(parentDir, m_PendingCreate.InitialName);

        m_Rename.BeginEditing(m_PendingCreate.VirtualPath, m_PendingCreate.InitialName, scopeTag);
    }

    void ProjectAssetsPanel::CancelPendingCreate()
    {
        // 静默丢弃：只清状态，不做任何 UI / RenameController 操作
        //   * RenameController 的编辑态已经由调用方（onCancel / ConsumeCancelledFlag）或本函数之外的路径清理
        //   * 不落盘，不产生磁盘 IO
        m_PendingCreate = PendingCreateState{};
    }

    void ProjectAssetsPanel::CommitCreateFolder(const std::filesystem::path& parentDir, const std::string& stem)
    {
        // 空字符串 → 用 InitialName 作为默认名（对齐用户"清空后回车 = 用默认名"的边界规则）
        std::string finalStem = stem.empty() ? std::string("New Folder") : stem;

        // uniquify 保证不与兄弟同名（防止创建时冲突）
        std::filesystem::path target = MakeUniquePath(parentDir, finalStem, "");

        std::error_code ec;
        std::filesystem::create_directory(target, ec);
        if (ec)
        {
            LF_CORE_ERROR("ProjectAssetsPanel::CommitCreateFolder - Failed to create '{0}': {1}", target.generic_string(), ec.message());
            return;
        }

        LF_CORE_INFO("ProjectAssetsPanel::CommitCreateFolder - Created '{0}'", target.generic_string());
        RebuildDirectoryTree();

        // 与 Unity 一致：创建后自动选中新建的 Folder
        SelectionManager::SelectFolder(target);
    }

    void ProjectAssetsPanel::CommitCreateMaterial(const std::filesystem::path& parentDir, const std::string& stem)
    {
        std::string finalStem = stem.empty() ? std::string("New Material") : stem;

        std::filesystem::path target = MakeUniquePath(parentDir, finalStem, ".lmat");

        // 使用 Standard Shader 作为默认 Shader
        const Ref<ShaderLibrary>& shaderLib = Renderer3D::GetShaderLibrary();
        Ref<Shader> standardShader = shaderLib->Get("Standard");

        std::string materialName = target.stem().string();
        Ref<Material> material = CreateRef<Material>(materialName, standardShader);

        AssetHandle handle = AssetManager::CreateAsset(material, target.generic_string());
        if (!handle.IsValid())
        {
            LF_CORE_ERROR("ProjectAssetsPanel::CommitCreateMaterial - Failed to create material at '{0}'", target.generic_string());
            return;
        }

        LF_CORE_INFO("ProjectAssetsPanel::CommitCreateMaterial - Created '{0}' (handle {1})", target.generic_string(), static_cast<uint64_t>(handle));
        RebuildDirectoryTree();
        SelectionManager::SelectAsset(handle);
    }

    void ProjectAssetsPanel::CommitCreateScene(const std::filesystem::path& parentDir, const std::string& stem)
    {
        std::string finalStem = stem.empty() ? std::string("New Scene") : stem;

        std::filesystem::path target = MakeUniquePath(parentDir, finalStem, ".luck3d");

        Ref<Scene> scene = CreateRef<Scene>();
        scene->SetName(target.stem().string());

        AssetHandle handle = AssetManager::CreateAsset(scene, target.generic_string());
        if (!handle.IsValid())
        {
            LF_CORE_ERROR("ProjectAssetsPanel::CommitCreateScene - Failed to create scene at '{0}'", target.generic_string());
            return;
        }

        LF_CORE_INFO("ProjectAssetsPanel::CommitCreateScene - Created '{0}' (handle {1})", target.generic_string(), static_cast<uint64_t>(handle));
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

    void ProjectAssetsPanel::RenameFolderTo(const std::filesystem::path& oldPath, const std::string& newName)
    {
        // ---- 1. 校验 ----
        if (newName.empty())
        {
            LF_CORE_WARN("ProjectAssetsPanel::RenameFolderTo - Empty name, keep original");
            return;
        }
        if (!std::filesystem::exists(oldPath) || !std::filesystem::is_directory(oldPath))
        {
            LF_CORE_WARN("ProjectAssetsPanel::RenameFolderTo - Old path invalid: '{0}'", oldPath.generic_string());
            return;
        }
        if (oldPath == m_AssetsDirectory)
        {
            LF_CORE_WARN("ProjectAssetsPanel::RenameFolderTo - Cannot rename the root Assets directory");
            return;
        }

        std::filesystem::path newPath = oldPath.parent_path() / newName;
        if (newPath == oldPath)
        {
            return;
        }
        if (std::filesystem::exists(newPath))
        {
            LF_CORE_WARN("ProjectAssetsPanel::RenameFolderTo - Target path already exists: '{0}'", newPath.generic_string());
            return;
        }

        // ---- 2. 提前收集"旧目录下所有已注册资产"的 (Handle, 旧相对路径)，用于改名后级联更新 Registry ----
        // 注意：必须在磁盘 rename 之前收集，因为 rename 后旧路径已不存在，无法再枚举
        // 使用 generic_string() 与 AssetRegistry 存储格式一致（正斜杠）
        std::vector<std::pair<AssetHandle, std::string>> handleAndOldRelPath;
        for (auto& entry : std::filesystem::recursive_directory_iterator(oldPath))
        {
            if (entry.is_directory())
            {
                continue;
            }
            std::string relPath = entry.path().generic_string();
            AssetHandle handle = AssetManager::GetAssetHandle(relPath);
            if (handle.IsValid())
            {
                handleAndOldRelPath.emplace_back(handle, relPath);
            }
        }

        // ---- 3. 磁盘 rename（一次性把整个子树移到新路径） ----
        std::error_code ec;
        std::filesystem::rename(oldPath, newPath, ec);
        if (ec)
        {
            LF_CORE_ERROR("ProjectAssetsPanel::RenameFolderTo - '{0}' -> '{1}': {2}", oldPath.generic_string(), newPath.generic_string(), ec.message());
            return;
        }

        // ---- 4. Registry 级联更新（Handle 保持不变，跨资产引用不断裂） ----
        // 旧相对路径前缀 → 新相对路径前缀，逐个替换 metadata->FilePath
        std::string oldPrefix = oldPath.generic_string();
        std::string newPrefix = newPath.generic_string();
        for (const std::pair<AssetHandle, std::string>& item : handleAndOldRelPath)
        {
            const std::string& oldRel = item.second;
            // 拼新相对路径：newPrefix + oldRel 去掉 oldPrefix 的部分
            std::string newRel = newPrefix + oldRel.substr(oldPrefix.size());
            AssetManager::UpdateAssetPath(item.first, newRel);
        }

        LF_CORE_INFO("ProjectAssetsPanel::RenameFolderTo - '{0}' -> '{1}' (cascaded {2} assets)", oldPrefix, newPrefix, handleAndOldRelPath.size());

        // ---- 5. 目录树重建 ----
        RebuildDirectoryTree();

        // ---- 6. m_CurrentDirectory 若在被改名子树内，同步纠正到新路径 ----
        std::string curStr = m_CurrentDirectory.generic_string();
        if (curStr == oldPrefix)
        {
            m_CurrentDirectory = newPath;
        }
        else if (curStr.size() > oldPrefix.size()
              && curStr.compare(0, oldPrefix.size(), oldPrefix) == 0
              && curStr[oldPrefix.size()] == '/')
        {
            m_CurrentDirectory = newPrefix + curStr.substr(oldPrefix.size());
        }

        // ---- 7. 若选中的是被改名目录本身，同步选中新路径 ----
        if (SelectionManager::GetSelectionType() == SelectionType::Folder
            && SelectionManager::GetSelectedFolder() == oldPath)
        {
            SelectionManager::SelectFolder(newPath);
        }
    }

    void ProjectAssetsPanel::RenameAssetTo(AssetHandle handle, const std::string& newStem)
    {
        if (newStem.empty())
        {
            LF_CORE_WARN("ProjectAssetsPanel::RenameAssetTo - Empty name, keep original");
            return;
        }
        if (!handle.IsValid())
        {
            LF_CORE_WARN("ProjectAssetsPanel::RenameAssetTo - Invalid handle");
            return;
        }

        // 取当前路径，拼新路径（保留扩展名）
        const std::string& oldFilePath = AssetManager::GetAssetFilePath(handle);
        std::filesystem::path oldPath(oldFilePath);
        std::filesystem::path newPath = oldPath.parent_path() / (newStem + oldPath.extension().string());

        if (newPath == oldPath)
        {
            return;
        }

        if (!AssetManager::MoveAsset(handle, newPath.generic_string()))
        {
            // AssetManager 内部已打错误日志（目标已存在 / rename 失败 / Registry 冲突）
            return;
        }

        RebuildDirectoryTree();
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
