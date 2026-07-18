#include "SceneHierarchyPanel.h"

#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/Components.h"
#include "Lucky/Scene/Components/ComponentType.h"

#include "Lucky/Renderer/MeshFactory.h"
#include "Lucky/Renderer/Renderer3D.h"

#include "Lucky/Scene/SelectionManager.h"

#include "Lucky/Editor/EditorIconManager.h"
#include "Lucky/Editor/DragDropPayloads.h"
#include "Lucky/Editor/DragDropContext.h"
#include "Lucky/Editor/DragDropVisuals.h"

#include "Lucky/UI/Widgets.h"
#include "Lucky/UI/Theme.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Lucky
{
    namespace
    {
        /// <summary>
        /// 拖拽放置模式（Hierarchy 面板内部实现细节，不暴露到头文件）
        /// </summary>
        enum class DropMode
        {
            Before,     // 插入到目标节点前面（同级）
            Inside,     // 成为目标节点的子节点
            After       // 插入到目标节点后面（可能吸附到祖先层级）
        };

        /// <summary>
        /// 拖拽放置目标：由 ComputeDropTarget 计算得出，包含 delivery 与视觉反馈所需的全部信息
        /// </summary>
        struct DropTarget
        {
            DropMode Mode = DropMode::Inside;
            Entity   Parent;                // Before/After 模式：落地后的父节点（可为空 = 根）；Inside 模式：无意义
            int      InsertIndex = -1;      // Before/After 模式：落地在 Parent.Children 中的索引；Inside 模式：无意义
            float    VisualLineX = 0.0f;    // Insert 高亮线左端点（圆圈中心）的屏幕坐标 X
            bool     Acceptable = false;    // 循环校验/自身校验通过后置 true

            // 仅 After 模式发生 X 层级回退时使用：
            // 被跨越的祖先节点 UUID（= chosen 层级对应的"pivot"节点，也就是新父节点的最后一个子）
            // 视觉上需要在该节点行底部绘制一个孤立小圆圈，提示层级回退
            // 0 表示无需绘制
            UUID     ExtraDotEntityUUID = 0;
        };

        /// <summary>
        /// 检查 potentialChild 设为 potentialParent 的子节点是否会形成循环
        /// - potentialParent 为空（拖到根层级）：一定不会形成循环
        /// - potentialParent == potentialChild：形成循环（自成父）
        /// - potentialChild 是 potentialParent 的祖先：形成循环
        /// </summary>
        bool WouldCreateCycleImpl(Entity potentialParent, Entity potentialChild)
        {
            if (!potentialParent) return false;
            if (potentialParent == potentialChild) return true;
            Entity current = potentialParent.GetParent();
            while (current)
            {
                if (current == potentialChild) return true;
                current = current.GetParent();
            }
            return false;
        }

        /// <summary>
        /// 按当前节点的可视 depth 计算该层级"图标左侧"的屏幕坐标 X
        /// 
        /// 布局假设（与 UI::BeginTreeNode 保持一致）：
        ///   [窗口内容区左边界] + [(depth+1) * IndentSpacing] + [FontSize] + [ArrowToIconSpacing] → 图标左侧
        /// 说明：
        /// - depth+1 的原因：Scene 是根 TreeNode，进入其 opened 分支后 ImGui 已对所有 entity 施加了 1 层缩进
        /// - 箭头占位宽度取纯 FontSize：参考 imgui_widgets.cpp 中 TreeNodeBehavior 的 ItemSize 使用的
        ///   text_width = g.FontSize + (label_size.x > 0 ? label_size.x + padding.x*2 : 0)；
        ///   BeginTreeNode 用空 label（""）调用 TreeNodeEx，label_size.x = 0，所以 text_width 就是 FontSize；
        ///   之后 SameLine 在 ItemSpacing.x = 0 的 ScopedStyle 下，CursorPos.x = CursorPosPrevLine.x + 0，
        ///   相当于紧接 FontSize 之后。text_offset_x（含 padding）只影响文本渲染位置，不影响 ItemSize 步进
        /// </summary>
        float ComputeIconLeftScreenX(int depth)
        {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            float windowContentLeft = window->WorkRect.Min.x;
            const ImGuiStyle& style = ImGui::GetStyle();
            float arrowWidth = ImGui::GetFontSize();
            return windowContentLeft
                + style.IndentSpacing * static_cast<float>(depth + 1)
                + arrowWidth
                + UI::Theme::Layout::TreeNodeArrowToIconSpacing;
        }

        /// <summary>
        /// 根据鼠标位置和 target entity 计算拖拽放置目标
        /// 
        /// 语义：
        /// - Y 方向使用 25%/50%/25% 三分区判定 Before / Inside / After（与 Unity 一致）
        /// - X 方向仅在 After 且 target 处于"其父的最后一个子"时启用"层级回退"吸附：
        ///     鼠标 X 越靠左，越向祖先层级吸附（对齐 Unity 拖拽表现）
        /// - 每个候选层级独立校验 WouldCreateCycleImpl；不合法层级从候选链中剔除
        /// </summary>
        /// <param name="scene">场景</param>
        /// <param name="target">Y 命中的目标 entity</param>
        /// <param name="targetDepth">target 在 Hierarchy 中的可视 depth</param>
        /// <param name="dragged">被拖拽的 entity</param>
        DropTarget ComputeDropTarget(Scene* scene, Entity target, int targetDepth, Entity dragged)
        {
            DropTarget result;

            // Y 三分区
            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 itemMin = ImGui::GetItemRectMin();
            ImVec2 itemMax = ImGui::GetItemRectMax();
            float itemHeight = itemMax.y - itemMin.y;
            if (itemHeight <= 0.0f)
            {
                result.Mode = DropMode::Inside;
                result.Acceptable = dragged && dragged != target && !WouldCreateCycleImpl(target, dragged);
                return result;
            }

            float relativeY = (mousePos.y - itemMin.y) / itemHeight;
            DropMode mode = DropMode::Inside;
            if (relativeY < 0.25f)      mode = DropMode::Before;
            else if (relativeY > 0.75f) mode = DropMode::After;

            result.Mode = mode;

            // 基础校验：自身 / 无效实体
            if (!dragged || dragged == target)
            {
                result.Acceptable = false;
                return result;
            }

            switch (mode)
            {
                case DropMode::Inside:
                {
                    result.Acceptable = !WouldCreateCycleImpl(target, dragged);
                    // Inside 模式下的 VisualLineX 用不到（走 HighlightTargetNode）
                    break;
                }
                case DropMode::Before:
                {
                    // Before：不做层级回退，直接对齐 target 自身层级
                    result.Parent = target.GetParent();
                    result.InsertIndex = scene->GetEntityIndexInParent(target);
                    result.VisualLineX = ComputeIconLeftScreenX(targetDepth);
                    result.Acceptable = !WouldCreateCycleImpl(result.Parent, dragged);
                    break;
                }
                case DropMode::After:
                {
                    // 候选层级：从深到浅
                    struct LevelCandidate
                    {
                        Entity Parent;      // 落地后的父（空 = 根）
                        int    InsertIndex; // 落地索引
                        int    Depth;       // 落地后的可视 depth
                        float  IconX;       // 该层级"图标左侧"屏幕 X
                        Entity Pivot;       // 该候选层级对应的"pivot"节点（用于定位孤立小圆圈的 Y）：
                                            // - candidates[0].Pivot = target 自身（不回退）
                                            // - 每次向上回退时，Pivot = 被跨越的"cursorParent"节点
                                            //   （即回退后新父节点的最后一个可见子，也是该层级的"最后一个可见节点"）
                    };
                    std::vector<LevelCandidate> candidates;

                    // 候选 0：target 自身层级（默认 After 语义）
                    candidates.push_back({
                        target.GetParent(),
                        scene->GetEntityIndexInParent(target) + 1,
                        targetDepth,
                        ComputeIconLeftScreenX(targetDepth),
                        target
                    });

                    // 向祖先方向扩展：仅当每层都是"父的最后一个子"时可回退
                    Entity cursor = target;
                    int cursorDepth = targetDepth;
                    while (cursorDepth > 0)
                    {
                        Entity cursorParent = cursor.GetParent();
                        if (!cursorParent) break;
                        const std::vector<UUID>& siblings = cursorParent.GetChildren();
                        if (siblings.empty() || siblings.back() != cursor.GetUUID()) break;

                        Entity newParent = cursorParent.GetParent();
                        int newIndex = scene->GetEntityIndexInParent(cursorParent) + 1;
                        int newDepth = cursorDepth - 1;
                        candidates.push_back({
                            newParent,
                            newIndex,
                            newDepth,
                            ComputeIconLeftScreenX(newDepth),
                            cursorParent
                        });

                        cursor = cursorParent;
                        cursorDepth = newDepth;
                    }

                    // 按鼠标 X 吸附：candidates 按由深到浅排序
                    // 手感：容差取 IndentSpacing 的 1.0 倍，等价于"当前深层图标 X 作为分界线"
                    // 意义：鼠标横坐标只要越过当前深层图标左端（进入其左侧的缩进区），就切换到上一层级
                    // 这与 Unity Hierarchy 的实际手感一致??用户直觉是"进入图标左边就升级"而非精确对齐中点
                    float snapTolerance = ImGui::GetStyle().IndentSpacing * 1.0f;
                    LevelCandidate chosen = candidates.back(); // 默认兜底：鼠标非常靠左时取最浅层
                    for (const LevelCandidate& c : candidates)
                    {
                        if (mousePos.x >= c.IconX - snapTolerance)
                        {
                            chosen = c;
                            break;
                        }
                    }

                    result.Parent = chosen.Parent;
                    result.InsertIndex = chosen.InsertIndex;
                    result.VisualLineX = chosen.IconX;
                    result.Acceptable = !WouldCreateCycleImpl(result.Parent, dragged);

                    // 若 X 吸附导致层级回退（chosen.Pivot ≠ target），则需要在"被跨越的祖先节点"
                    // 底部绘制一个孤立小圆圈，直观提示层级回退
                    // - chosen.Pivot == target：没有发生回退，孤立圆圈的 Y 等于主线 Y，无需绘制
                    // - chosen.Pivot != target：发生了回退，chosen.Pivot 就是需要在其底部画圆圈的节点
                    if (chosen.Pivot && chosen.Pivot != target)
                    {
                        result.ExtraDotEntityUUID = chosen.Pivot.GetUUID();
                    }
                    break;
                }
            }

            return result;
        }
    }

    SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
        // 订阅 SceneManager 的场景切换事件，activeScene 变更时自动同步 m_Scene
        // 面板生命周期通常晚于 SceneManager 首次 SetActiveScene，因此这里的 lambda 也会承接首帧广播
        m_SceneChangedSub = SceneManager::Subscribe([this](const Ref<Scene>& newScene)
        {
            SetScene(newScene);
        });
    }

    SceneHierarchyPanel::~SceneHierarchyPanel()
    {
        SceneManager::Unsubscribe(m_SceneChangedSub);
    }

    void SceneHierarchyPanel::SetScene(const Ref<Scene>& scene)
    {
        m_Scene = scene;
    }

    void SceneHierarchyPanel::OnUpdate(DeltaTime dt)
    {
        
    }

    void SceneHierarchyPanel::OnGUI()
    {
        // 帧首清空 Pending Insert 线：本帧遍历时若有 Before/After 目标命中，会重新写入
        m_PendingInsertLine.Valid = false;
        m_PendingInsertLine.HasExtraDot = false;

        // 帧首清空实体底部 Y 缓存：本帧遍历时会重新写入每个已绘制节点的 ItemRectMax.y
        m_EntityBottomY.clear();

        // 帧首重置"悬停自动展开"的命中标记：本帧任意节点触发悬停累计逻辑时会置 true
        // 帧末据此判断是否清零累计（拖拽结束或鼠标离开所有节点时归零）
        m_HoverExpand.HitThisFrame = false;

        std::string sceneName = m_Scene->GetName();
        
        const std::string& strSceneID = std::format("{0}##{1}", sceneName, typeid(Scene).hash_code());
        
        const Ref<Texture2D>& icon = EditorIconManager::GetAssetTypeIcon(AssetType::Scene);
        
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);    // TODO 封装 Fonts
        bool opened = UI::BeginTreeNode(icon, strSceneID.c_str(), true);
        ImGui::PopFont();
        
        if (opened)
        {
            // 按 m_RootEntityOrder 顺序绘制根节点
            const std::vector<UUID>& rootOrder = m_Scene->GetRootEntityOrder();
            for (UUID rootID : rootOrder)
            {
                Entity entity = m_Scene->TryGetEntityWithUUID(rootID);
                if (entity)
                {
                    DrawEntityNode(entity, 0);
                }
                else
                {
                    // 兜底：m_RootEntityOrder 与 m_EntityIDMap 不一致时（理论上不应发生）
                    LF_CORE_WARN("[Hierarchy] Root entity UUID {0} not found in scene", static_cast<uint64_t>(rootID));
                }
            }
            
            UI::EndTreeNode();
        }
        
        // 点击鼠标 && 鼠标悬停在该窗口（点击空白位置）
        // 注意：必须排除"点击落在任意 Item 上"的情况：
        //   - IsWindowHovered 默认在鼠标位于 Item 上时仍返回 true（Item 属于该窗口），
        //     若不加 !IsAnyItemHovered，则用户在内联重命名 InputText 内部点击时也会走进该分支，
        //     导致 m_Rename.EditingEntityUUID = 0 → 下一帧编辑框直接消失，无法通过鼠标编辑文本
        //     （问题 3：日志中 InputText 在 mouse down 那一帧突然 deact=true 的根因）
        //   - 空白点击（真正的空白区域）不会命中任何 Item，IsAnyItemHovered() 为 false，行为不变
        if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
        {
            SelectionManager::Deselect();   // 取消选中

            // 同步退出内联重命名编辑态（点击 InputText 内部不会命中此分支，因为 InputText 会消费点击）
            m_Rename.EditingEntityUUID = 0;
        }
        
        // ---- 空白区域拖拽目标：拖到空白 → 变为根节点（末尾） ----
        // 注意：这里必须使用 BeginDragDropTargetCustom（自定义矩形版本），而不是
        //       InvisibleButton/Dummy + BeginDragDropTarget。原因：
        //       InvisibleButton/Dummy 会创建一个 ImGui Item，把整个空白区域"占据"，导致：
        //         1) 下方 BeginPopupContextWindow(NoOpenOverItems) 判定失败 → 右键菜单弹不出，无法创建物体
        //         2) ImGui::IsWindowHovered / IsMouseClicked 的"点击空白取消选中"逻辑失效
        //       BeginDragDropTargetCustom 直接注册一个纯粹的拖放热区，不产生 Item，天然规避以上副作用
        {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
            ImVec2 windowMax = window->WorkRect.Max;
            ImRect emptyRect(cursorScreenPos, windowMax);

            if (emptyRect.Min.y < emptyRect.Max.y &&
                ImGui::BeginDragDropTargetCustom(emptyRect, ImGui::GetID("##HierarchyEmptyDropArea")))
            {
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                    DragDrop::EntityHierarchy,
                    ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

                if (payload && payload->DataSize == sizeof(UUID))
                {
                    UUID draggedID = *static_cast<UUID*>(payload->Data);
                    Entity dragged = m_Scene->TryGetEntityWithUUID(draggedID);

                    // 空白区域语义："把 dragged 移动到根层级的末尾"（对齐 Unity 表现）：
                    // - dragged 是非根节点：从原父节点移出，追加到 m_RootEntityOrder 末尾
                    // - dragged 已是根节点：在 m_RootEntityOrder 中挪到末尾（若原本就在末尾则位置不变）
                    // 无论是否为根，只要 Entity 有效就视为合法目标，tooltip 显示允许图标
                    bool acceptable = static_cast<bool>(dragged);

                    if (acceptable)
                    {
                        // 通知源端 → tooltip 切换为"允许"图标
                        UI::DragDropContext::NotifyTargetAccepts(DragDrop::EntityHierarchy);

                        // 空白区域默认不绘制视觉反馈（tooltip 图标已足够）

                        if (payload->IsDelivery())
                        {
                            if (dragged.GetParent())
                            {
                                // 非根节点：脱离旧父，追加到根节点末尾
                                dragged.SetParent({});
                            }
                            else
                            {
                                // 已是根节点：在根节点顺序列表中挪到末尾
                                dragged.MoveToIndex(-1);
                            }
                        }
                    }
                }

                ImGui::EndDragDropTarget();
            }
        }
        
        // 创建物体 右键点击窗口白区域弹出菜单：- 右键 不在物体项上
        if (UI::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            DrawEntityCreateMenu({});   // 绘制创建物体菜单
            UI::EndPopup();
        }

        // ---- 帧末维护"悬停自动展开"累计状态 ----
        // 若本帧没有任何节点命中悬停累计逻辑（拖拽已释放 / 鼠标移出所有可展开节点 / 目标是叶子或已展开），
        // 则清零累计，避免下次悬停到别处时误触发
        if (!m_HoverExpand.HitThisFrame)
        {
            m_HoverExpand.HoveredEntityUUID = 0;
            m_HoverExpand.HoverAccumTime = 0.0f;
        }

        // ---- 帧末维护"内联重命名 pending"两阶段仲裁（对齐 Unity 表现）----
        // 前置约定：DrawEntityNode 在点击"已选中节点的名称区"时只登记 PendingEntityUUID + PendingName 快照，
        //           不立即进入编辑态。真正进入编辑态由本处根据鼠标行为决定：
        //
        //   1) 若鼠标在按下-抬起期间跨过了 IO.MouseDragThreshold（IsMouseDragging(0) 返回 true）
        //      → 视为用户在拖拽，取消候选（rename 优先级低于 drag）
        //      注意：ImGui::BeginDragDropSource 本身也是靠这个阈值启动 Drag Payload，两处判据一致，天然对齐
        //   2) 若鼠标释放时（IsMouseReleased(0)）候选仍存在
        //      → 期间未发生拖拽，视为"点击已选中节点抬起"，正式进入编辑态：
        //        * 用 PendingName 快照初始化 InputText 缓冲（隔离抬起前 name 变化 / 引用失效等边界情况）
        //        * 标记 FirstFrame，让 InlineRenameInput 首帧 SetKeyboardFocusHere + AutoSelectAll
        //   3) 释放后无论是否进入编辑态都必须清空 pending，防止跨点击串扰
        if (m_Rename.PendingEntityUUID != 0)
        {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                // 用户在拖拽 → 取消候选
                m_Rename.PendingEntityUUID = 0;
                m_Rename.PendingName.clear();
            }
            else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                // 未拖拽即释放 → 进入编辑态
                m_Rename.EditingEntityUUID = m_Rename.PendingEntityUUID;
                m_Rename.FirstFrame        = true;
                std::strncpy(m_Rename.Buffer, m_Rename.PendingName.c_str(), sizeof(m_Rename.Buffer) - 1);
                m_Rename.Buffer[sizeof(m_Rename.Buffer) - 1] = '\0';

                m_Rename.PendingEntityUUID = 0;
                m_Rename.PendingName.clear();
            }
        }

        // ---- 帧末统一绘制拖拽视觉反馈 ----
        // 延后到整个 Hierarchy 遍历结束后再绘制，确保高亮的 DrawList 命令位于所有节点
        // （含选中态 header 蓝色填充）之后，几何上位于最上层，不会被后续兄弟节点的
        // 选中态 header 遮挡
        if (m_PendingInsertLine.Valid)
        {
            switch (m_PendingInsertLine.Kind)
            {
                case PendingInsertLine::KindType::Line:
                    UI::DragDropVisuals::HighlightTargetInsertLineAt(
                        m_PendingInsertLine.X,
                        m_PendingInsertLine.Y);
                    // After 模式发生 X 层级回退时：在被跨越的祖先节点底部额外绘制孤立小圆圈，
                    // 与主线左端圆圈同 X、但 Y 为祖先节点行底部
                    if (m_PendingInsertLine.HasExtraDot)
                    {
                        UI::DragDropVisuals::HighlightTargetInsertDotAt(
                            m_PendingInsertLine.ExtraDotX,
                            m_PendingInsertLine.ExtraDotY);
                    }
                    break;
                case PendingInsertLine::KindType::Rect:
                    UI::DragDropVisuals::HighlightTargetNodeAt(
                        m_PendingInsertLine.MinX,
                        m_PendingInsertLine.MinY,
                        m_PendingInsertLine.MaxX,
                        m_PendingInsertLine.MaxY);
                    break;
            }
        }
    }
    
    void SceneHierarchyPanel::DrawEntityNode(Entity entity, int depth)
    {
        const std::string& name = entity.GetComponent<NameComponent>().Name;  // 物体名
        UUID id = entity.GetUUID();
        const std::string& strID = std::format("{0}##{1}", name, static_cast<uint64_t>(id));

        bool isLeaf = entity.GetChildren().empty(); // 是叶节点

        const Ref<Texture2D>& icon = EditorIconManager::GetEntityIcon();

        // 若该节点被标记为"需强制展开"（来源于三种时机：悬停达阈值 / Inside Drop 落地 / 右键创建子节点），
        // 在调用 BeginTreeNode（内部即将 TreeNodeEx("")）前预置 open 状态。
        // ImGuiCond_Always 保证覆盖 StateStorage 中的原折叠状态
        if (m_PendingExpand.erase(id) > 0)
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }

        // 是否处于内联重命名编辑态：若是，则由本函数在名称位置绘制 InputText，TreeNode 内部不再渲染默认文本
        bool isRenaming = (m_Rename.EditingEntityUUID == id);

        // 编辑态下禁用 TreeNode 的鼠标交互：
        // - 原因：TreeNode 使用 SpanFullWidth，其 interact_bb 覆盖整行；即便后续调用 SetItemAllowOverlap
        //   使 InputText 能拿到激活，TreeNode 自身的 ButtonBehavior 已经在本帧较早处理完鼠标事件
        //   （产生 hover 视觉反馈、选中态副作用），导致"点击穿透到下面的树节点"的观感
        // - ImGuiItemFlags_Disabled 会让 TreeNode 在 ItemHoverable 阶段直接返回 false，
        //   完全不产生 hover / press / active 状态，注释明确指出该 flag "doesn't affect visuals"
        //   ?? TreeNode 的选中态蓝色 header 由 flags & ImGuiTreeNodeFlags_Selected 独立控制，不受影响
        // - 必须在 BeginTreeNode（内部 TreeNodeEx）之前 push，才能作用到 TreeNode 的 InFlags
        if (isRenaming)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        }

        bool opened = UI::BeginTreeNode(icon, strID.c_str(), false, SelectionManager::IsSelected(id), isLeaf, !isRenaming);

        if (isRenaming)
        {
            ImGui::PopItemFlag();

            // ---- 关键修复：让 InlineRenameInput 内的 InputText 能正常接收鼠标点击 ----
            // 根因：TreeNode 使用 SpanFullWidth，其 interact_bb 覆盖整行；虽然本节点已 PushItemFlag(Disabled)，
            //       但 ImGui 的 ItemHoverable 内部执行顺序为：
            //           1) SetHoveredID(id) 无条件将 g.HoveredId 设为 TreeNode 的 ID
            //           2) 后置的 Disabled 检查置 g.HoveredIdDisabled = true 并返回 false
            //       结果：TreeNode 依旧"抢占"了 g.HoveredId。
            //       后续 InlineRenameInput 内部 InputText 的 ItemHoverable 判定：
            //           if (g.HoveredId != 0 && g.HoveredId != id && !g.HoveredIdAllowOverlap) return false;
            //       g.HoveredId 已经是 TreeNode 的 ID → InputText hover 返回 false
            //       → InputTextEx 内 user_clicked = false → 命中"Release focus when we click outside"
            //         (g.ActiveId == id && MouseClicked && !init_make_active) → clear_active_id = true
            //       → InputText 被强制失活，本函数走 IsItemDeactivated 分支 → Cancelled → 编辑态被清零
            //       → 用户观察到"点击编辑框内部编辑框直接消失"
            //
            // 修复：SetItemAllowOverlap() 作用于上一个 Item（此时 LastItemData.ID = TreeNode ID），
            //       将 g.HoveredIdAllowOverlap 置 true，InputText 的 hover 判定通过 !g.HoveredIdAllowOverlap
            //       条件即可正常 SetHoveredID(InputText_ID) 并接收点击，不再被误 clear_active_id。
            ImGui::SetItemAllowOverlap();
        }

        // 缓存当前节点的行底部 Y（= ItemRectMax.y）：供 After X 层级回退时定位孤立圆圈使用
        // 写入时机必须在 BeginTreeNode 后、递归子节点前，确保拿到的是 header 行的底部而非整个子树的底部
        m_EntityBottomY[id] = ImGui::GetItemRectMax().y;

        // 绘制右侧组件图标列表（同时会写入 m_LastNameHitRect：本节点名称文本命中矩形，供下方 rename 触发/覆盖使用）
        // depth 传入以便准确计算名称左边界（避免使用 BeginTreeNode 返回后已被 TreePush 改变的 DC.Indent.x）
        m_LastNameHitRect.Valid = false;
        DrawEntityComponentIcons(entity, depth);

        // ---- 内联重命名：编辑态覆盖文本渲染 ----
        // 处于编辑态时，屏蔽本节点的选中/拖拽/右键菜单交互，只保留 InputText 及其提交/取消处理
        if (isRenaming)
        {
            if (m_LastNameHitRect.Valid)
            {
                ImVec2 rectMin(m_LastNameHitRect.MinX, m_LastNameHitRect.MinY);
                ImVec2 rectMax(m_LastNameHitRect.MaxX, m_LastNameHitRect.MaxY);

                UI::InlineRenameResult renameResult = UI::InlineRenameInput(
                    "##HierarchyRename",
                    rectMin, rectMax,
                    m_Rename.Buffer, sizeof(m_Rename.Buffer),
                    m_Rename.FirstFrame);

                // 首帧过后清除 FirstFrame，避免每帧都 SetKeyboardFocusHere
                m_Rename.FirstFrame = false;

                if (renameResult.Submitted)
                {
                    // 提交：交由数据层做空校验与写入
                    // Entity::SetName 内部：empty → 打印警告并返回 false（此时保留原名，符合 Unity 表现）
                    entity.SetName(renameResult.CommittedName);
                    m_Rename.EditingEntityUUID = 0;
                }
                else if (renameResult.Cancelled)
                {
                    // 取消：保留原名，直接退出编辑态
                    m_Rename.EditingEntityUUID = 0;
                }
            }
            else
            {
                // 命中矩形失效（异常兜底）：直接退出编辑态，防止卡死
                m_Rename.EditingEntityUUID = 0;
            }

            // 编辑态下仍需正确处理子节点遍历与 TreePop（保持原有折叠状态语义）
            if (opened)
            {
                std::vector<UUID> children = entity.GetChildren();
                for (UUID childID : children)
                {
                    Entity child = m_Scene->GetEntityWithUUID(childID);
                    DrawEntityNode(child, depth + 1);
                }

                UI::EndTreeNode();
            }
            return;
        }

        // 树结点被点击
        if (ImGui::IsItemClicked())
        {
            // 触发内联重命名：仅当"点击发生前节点就已选中"且"命中矩形覆盖鼠标位置"时才为编辑候选
            // - 已选中判定：IsSelected 反映的是本帧点击之前的选中状态（本次 Select 调用在其之后）
            // - 命中矩形：图标右侧到组件图标区左侧的名称文本区（点击箭头/图标/组件图标不触发）
            //
            // 【对齐 Unity 表现，两阶段判定】：
            //   ImGui::IsItemClicked = IsMouseClicked（按下瞬间）+ IsItemHovered，所以本分支在鼠标"按下瞬间"就会命中。
            //   若这里立刻进入编辑态，用户"点击已选中节点后按住拖拽"就会先弹出编辑框、再启动拖拽，观感错误。
            //   Unity 的行为是：按下不进入编辑态，抬起时若期间未发生拖拽，才进入编辑态；期间发生拖拽则取消候选。
            //   因此本分支只登记候选（m_Rename.PendingEntityUUID + PendingName 快照），
            //   真正的"进入编辑态"由 OnGUI 帧末统一处理（观察 IsMouseDragging / IsMouseReleased）。
            bool wasSelectedBeforeClick = SelectionManager::IsSelected(id);
            ImVec2 mouse = ImGui::GetMousePos();
            bool inNameHit = m_LastNameHitRect.Valid
                && mouse.x >= m_LastNameHitRect.MinX && mouse.x <= m_LastNameHitRect.MaxX
                && mouse.y >= m_LastNameHitRect.MinY && mouse.y <= m_LastNameHitRect.MaxY;

            if (wasSelectedBeforeClick && inNameHit)
            {
                // 只登记候选：按下瞬间不进入编辑态、不清空选中、不启动 Select 副作用
                // - PendingName 快照当前 name，抬起时用它初始化 InputText 缓冲，
                //   避免抬起前 name 因外部原因变化时使用错误值
                m_Rename.PendingEntityUUID = id;
                m_Rename.PendingName       = name;
            }
            else
            {
                // 未选中 或 不在名称命中区：走原有选中逻辑，同时清除任何已有 pending（避免"先点未选中节点→再点选中节点抬起"跨节点串扰）
                m_Rename.PendingEntityUUID = 0;
                m_Rename.PendingName.clear();

                SelectionManager::Select(id);   // 选中物体

                // 选中项发生变化：若之前有节点处于编辑态且非本节点，则强制退出编辑态
                if (m_Rename.EditingEntityUUID != 0 && m_Rename.EditingEntityUUID != id)
                {
                    m_Rename.EditingEntityUUID = 0;
                }

                LF_TRACE("Selected Entity: [ENTT = {0}, UUID {1}, Name {2}]", static_cast<uint32_t>(entity), id, entity.GetName());
            }
        }

        // ---- 拖拽源：与 ProjectAssetsPanel 保持一致的写法 ----
        if (UI::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
        {
            UUID draggedID = entity.GetUUID();
            ImGui::SetDragDropPayload(DragDrop::EntityHierarchy, &draggedID, sizeof(UUID));

            // 源端 tooltip 图标：命中匹配目标时显示"允许"，否则显示"禁止"
            UI::DragDropPreview(UI::DragDropContext::IsRejected(DragDrop::EntityHierarchy));

            UI::EndDragDropSource();
        }

        // ---- 拖拽目标：peek + NotifyTargetAccepts + IsDelivery 分离 ----
        if (ImGui::BeginDragDropTarget())
        {
            // 悬停期间即可拿到 payload；抑制 ImGui 默认高亮矩形，由目标端自绘反馈
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                DragDrop::EntityHierarchy,
                ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

            if (payload && payload->DataSize == sizeof(UUID))
            {
                UUID draggedID = *static_cast<UUID*>(payload->Data);
                Entity dragged = m_Scene->TryGetEntityWithUUID(draggedID);

                // 综合计算放置目标：Y 三分区 + X 层级吸附（After 模式下向祖先层级回退）+ 循环校验
                DropTarget drop = ComputeDropTarget(m_Scene.get(), entity, depth, dragged);

                // ---- 悬停自动展开累计（对齐 Unity Spring-loaded folders）----
                // 仅对"非叶且当前折叠"的节点生效：
                // - 叶节点无展开语义
                // - 已展开节点无需再自动展开
                // 拖拽悬停在该节点上时（无论 drop.Acceptable 与否，因为语义是"帮用户看见子层"），
                // 只要 payload 存在于该 target 上就累计
                if (!isLeaf && !opened)
                {
                    m_HoverExpand.HitThisFrame = true;
                    if (m_HoverExpand.HoveredEntityUUID == id)
                    {
                        m_HoverExpand.HoverAccumTime += ImGui::GetIO().DeltaTime;
                        if (m_HoverExpand.HoverAccumTime >= s_HoverExpandDelay)
                        {
                            // 达阈值：下一帧强制展开，并复位累计（避免连续多帧重复触发）
                            m_PendingExpand.insert(id);
                            m_HoverExpand.HoveredEntityUUID = 0;
                            m_HoverExpand.HoverAccumTime = 0.0f;
                        }
                    }
                    else
                    {
                        // 切换到新的悬停目标：重置累计
                        m_HoverExpand.HoveredEntityUUID = id;
                        m_HoverExpand.HoverAccumTime = 0.0f;
                    }
                }

                if (drop.Acceptable)
                {
                    // 通知源端：本帧存在可接受目标 → tooltip 切换为"允许"图标
                    UI::DragDropContext::NotifyTargetAccepts(DragDrop::EntityHierarchy);

                    // 目标端主动绘制视觉反馈
                    // 三种模式的高亮均延后到帧末统一绘制，避免被后续兄弟节点选中态 header 蓝色填充遮挡：
                    // - Before / After：Insert 线（Line 模式）
                    // - Inside：整节点高亮框（Rect 模式），外沿与目标 header 完全等大
                    switch (drop.Mode)
                    {
                        case DropMode::Before:
                            m_PendingInsertLine.Valid = true;
                            m_PendingInsertLine.Kind  = PendingInsertLine::KindType::Line;
                            m_PendingInsertLine.X     = drop.VisualLineX;
                            m_PendingInsertLine.Y     = ImGui::GetItemRectMin().y;
                            break;
                        case DropMode::After:
                            m_PendingInsertLine.Valid = true;
                            m_PendingInsertLine.Kind  = PendingInsertLine::KindType::Line;
                            m_PendingInsertLine.X     = drop.VisualLineX;
                            m_PendingInsertLine.Y     = ImGui::GetItemRectMax().y;
                            // X 层级回退：在被跨越的祖先节点底部额外画孤立小圆圈
                            // 位置：与主线左端圆圈同 X，Y 为祖先节点的行底部（从缓存中取）
                            if (drop.ExtraDotEntityUUID != 0)
                            {
                                auto it = m_EntityBottomY.find(drop.ExtraDotEntityUUID);
                                if (it != m_EntityBottomY.end())
                                {
                                    m_PendingInsertLine.HasExtraDot = true;
                                    m_PendingInsertLine.ExtraDotX   = drop.VisualLineX;
                                    m_PendingInsertLine.ExtraDotY   = it->second;
                                }
                            }
                            break;
                        case DropMode::Inside:
                        {
                            ImVec2 itemMin = ImGui::GetItemRectMin();
                            ImVec2 itemMax = ImGui::GetItemRectMax();
                            m_PendingInsertLine.Valid = true;
                            m_PendingInsertLine.Kind  = PendingInsertLine::KindType::Rect;
                            m_PendingInsertLine.MinX  = itemMin.x;
                            m_PendingInsertLine.MinY  = itemMin.y;
                            m_PendingInsertLine.MaxX  = itemMax.x;
                            m_PendingInsertLine.MaxY  = itemMax.y;
                            break;
                        }
                    }

                    // 只有鼠标释放时才真正执行层级变更
                    if (payload->IsDelivery())
                    {
                        if (drop.Mode == DropMode::Inside)
                        {
                            dragged.SetParent(entity);

                            // 对齐 Unity：Inside Drop 落地后立即展开目标节点，
                            // 使用户能立刻看到刚放入的子节点（覆盖"未到悬停阈值就释放"的情况）
                            m_PendingExpand.insert(id);
                        }
                        else
                        {
                            // Before / After（含 X 层级回退）：统一用 (Parent, InsertIndex) 落地
                            dragged.SetParent(drop.Parent, drop.InsertIndex);
                        }
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }

        const std::string rightClickPopupID = std::format("{0}-ContextMenu", strID);
        
        // 删除物体
        bool entityDeleted = false;
        // 右键点击该物体结点
        if (UI::BeginPopupContextItem(rightClickPopupID.c_str(), ImGuiPopupFlags_MouseButtonRight))
        {
            SelectionManager::Select(id);   // 选中物体
            
            // 菜单项：删除物体
            if (ImGui::MenuItem("Delete"))
            {
                entityDeleted = true;   // 标记为已删除：渲染结束后面的 UI 再删除该物体
            }
            
            ImGui::Separator();
            
            DrawEntityCreateMenu(entity);   // 绘制创建物体菜单

            UI::EndPopup();
        }

        // 树结点已打开
        if (opened)
        {
            // 拷贝子节点列表后再遍历：避免遍历过程中原始列表被修改导致跳过子节点
            std::vector<UUID> children = entity.GetChildren();
            for (UUID childID : children)
            {
                Entity child = m_Scene->GetEntityWithUUID(childID);
                DrawEntityNode(child, depth + 1);
            }

            UI::EndTreeNode();
        }
        
        if (entityDeleted)
        {
            m_Scene->DestroyEntity(entity); // 销毁物体
            
            // 删除的物体为已选中物体
            if (SelectionManager::IsSelected(id))
            {
                SelectionManager::Deselect();   // 清空选中项
            }

            // 若删除的物体正处于内联重命名编辑态，同步退出编辑态，防止悬空 UUID
            if (m_Rename.EditingEntityUUID == id)
            {
                m_Rename.EditingEntityUUID = 0;
            }
        }
    }

    void SceneHierarchyPanel::DrawEntityCreateMenu(Entity parent)
    {
        if (!ImGui::BeginMenu("Create"))
        {
            return;
        }
        
        Entity newEntity;
        
        // 创建空物体
        if (ImGui::MenuItem("Create Empty"))
        {
            std::string uniqueName = GenerateUniqueName("Entity", parent);
            newEntity = m_Scene->CreateEntity(uniqueName, parent);
        }
        
        // 创建 3D Object
        if (ImGui::BeginMenu("3D Object"))
        {
            // 创建 Cube
            if (ImGui::MenuItem("Cube"))
            {
                std::string uniqueName = GenerateUniqueName("Cube", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
            
                // MeshFilter
                newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Cube);
            
                // MeshRenderer
                MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
                meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());  // 使用默认材质
            }
            
            // 创建 Plane
            if (ImGui::MenuItem("Plane"))
            {
                std::string uniqueName = GenerateUniqueName("Plane", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Plane);
                MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
                meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
            }
            
            // 创建 Sphere
            if (ImGui::MenuItem("Sphere"))
            {
                std::string uniqueName = GenerateUniqueName("Sphere", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Sphere);
                MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
                meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
            }
            
            // 创建 Cylinder
            if (ImGui::MenuItem("Cylinder"))
            {
                std::string uniqueName = GenerateUniqueName("Cylinder", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Cylinder);
                MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
                meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
            }
            
            // 创建 Capsule
            if (ImGui::MenuItem("Capsule"))
            {
                std::string uniqueName = GenerateUniqueName("Capsule", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Capsule);
                MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
                meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
            }
            
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Light"))
        {
            // 创建 Directional Light
            if (ImGui::MenuItem("Directional Light"))
            {
                std::string uniqueName = GenerateUniqueName("Directional Light", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<LightComponent>(LightType::Directional);
            
                // 设置初始方向斜向下
                TransformComponent& transform = newEntity.GetComponent<TransformComponent>();
                transform.SetRotationEuler(glm::vec3(glm::radians(50.0f), glm::radians(-32.0f), 0.0f));
            }
        
            // 创建 Point Light
            if (ImGui::MenuItem("Point Light"))
            {
                std::string uniqueName = GenerateUniqueName("Point Light", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<LightComponent>(LightType::Point);
            }

            // 创建 Spot Light
            if (ImGui::MenuItem("Spot Light"))
            {
                std::string uniqueName = GenerateUniqueName("Spot Light", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<LightComponent>(LightType::Spot);
            }
            
            ImGui::EndMenu();
        }

        // 创建 Post Process Volume
        if (ImGui::MenuItem("Post Process Volume"))
        {
            std::string uniqueName = GenerateUniqueName("Post Process Volume", parent);
            newEntity = m_Scene->CreateEntity(uniqueName, parent);
            newEntity.AddComponent<PostProcessVolumeComponent>();
        }
        
        if (newEntity)
        {
            SelectionManager::Deselect();
            SelectionManager::Select(newEntity.GetUUID());

            // 对齐 Unity：在折叠的 parent 节点上右键创建子节点后，parent 应立即展开
            // 使新创建的子节点可见；parent 无效（空白区域右键创建根节点）时无需展开
            if (parent)
            {
                m_PendingExpand.insert(parent.GetUUID());
            }
        }
        
        ImGui::EndMenu();
    }

    void SceneHierarchyPanel::DrawEntityComponentIcons(Entity entity, int depth)
    {
        // 保存 TreeNode 的 LastItemData，绘制完图标后恢复，确保调用方的 IsItemClicked 等交互检测正确工作
        ImGuiContext& g = *GImGui;
        const ImGuiLastItemData savedItemData = g.LastItemData;

        // 收集该实体拥有的非默认组件图标（排除 Transform）
        std::vector<const Ref<Texture2D>*> icons;
        if (entity.HasComponent<MeshFilterComponent>())         icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::MeshFilter));
        if (entity.HasComponent<MeshRendererComponent>())       icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::MeshRenderer));
        if (entity.HasComponent<LightComponent>())
        {
            LightType lightType = entity.GetComponent<LightComponent>().Type;
            icons.push_back(&EditorIconManager::GetLightIcon(lightType));
        }
        if (entity.HasComponent<PostProcessVolumeComponent>())  icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::PostProcessVolume));

        float iconSize = ImGui::GetTextLineHeight() - UI::Theme::Layout::TreeNodeIconSizeShrink;
        float iconSpacing = UI::Theme::Layout::TreeNodeComponentIconSpacing;
        float minGap = UI::Theme::Layout::TreeNodeComponentIconMinGap;

        // 计算图标区域总宽度
        float totalIconWidth = icons.empty() ? 0.0f : (icons.size() * iconSize + (icons.size() - 1) * iconSpacing);

        // 获取可用区域右边界（使用窗口内相对坐标，减去右边距）
        float contentRightLocal = ImGui::GetContentRegionMax().x - UI::Theme::Layout::TreeNodeComponentIconRightMargin;
        float iconsStartLocal = contentRightLocal - totalIconWidth;

        // 计算名称文本的实际右边界（窗口内相对坐标）
        // 注意：GetItemRectMax 返回的是 TreeNode 的 Rect（SpanFullWidth 导致其跨越整行），
        // 所以需要通过 TreeNode 左边界 + 缩进 + 箭头 + 图标 + 文本宽度 来计算名称的实际右边界
        const std::string& name = entity.GetComponent<NameComponent>().Name;
        float textWidth = ImGui::CalcTextSize(name.c_str()).x;

        // TreeNode 的左边界（ItemRectMin 是屏幕坐标，转为窗口内坐标）
        // 注意： SpanFullWidth 时 ItemRectMin.x = window->WorkRect.Min.x（不含缩进）
        float itemLeftLocal = ImGui::GetItemRectMin().x - ImGui::GetWindowPos().x + ImGui::GetScrollX();
        // 本节点的真实缩进量：不能使用 DC.Indent.x！
        // 因为 BeginTreeNode 内部 TreeNodeEx 已调用 TreePush 使 DC.Indent.x 多加了一层 IndentSpacing（为下一层子节点预留）。
        // 实际层级缩进 = (depth+1) * IndentSpacing：
        // - Scene 根 TreeNode opened 后 TreePush 产生 1 层基础缩进
        // - 本节点处于 depth 层 → 额外累加 depth 层
        float indentWidth = ImGui::GetStyle().IndentSpacing * static_cast<float>(depth + 1);
        // 箭头占位宽度（严格按 ImGui 内部 TreeNodeBehavior 的 ItemSize 步进计算）：
        // - imgui_widgets.cpp TreeNodeBehavior 中：text_width = FontSize + (label_size.x > 0 ? label_size.x + padding.x*2 : 0)
        // - BeginTreeNodeInternal 用空 label（""）调用 TreeNodeEx，label_size.x = 0 → text_width = FontSize
        // - 之后 SameLine 在 ItemSpacing.x = 0 的 ScopedStyle 下紧接 FontSize 后
        // 注意：这里 *不能* 加 FramePadding.x！之前 +FramePadding.x 属于 bug ?? 会让 nameStartLocal 偏右
        //       FramePadding.x 像素，进而使内联重命名编辑框内文本相对原文本偏右 FramePadding.x 像素
        float arrowWidth = ImGui::GetFontSize();
        // 名称文本的左边界 = TreeNode左边界 + 缩进 + 箭头宽度 + 箭头到图标间距 + 图标尺寸 + 图标到文本间距
        float nameStartLocal = itemLeftLocal + indentWidth + arrowWidth
            + UI::Theme::Layout::TreeNodeArrowToIconSpacing
            + iconSize
            + UI::Theme::Layout::TreeNodeIconToTextSpacing;
        float nameEndLocal = nameStartLocal + textWidth;

        // 碰撞检测：如果图标区域侵入名称区域，则从左侧开始移除图标
        while (!icons.empty() && iconsStartLocal < nameEndLocal + minGap)
        {
            icons.erase(icons.begin());
            totalIconWidth = icons.empty() ? 0.0f : (icons.size() * iconSize + (icons.size() - 1) * iconSpacing);
            iconsStartLocal = contentRightLocal - totalIconWidth;
        }

        // ---- 写入名称文本"命中矩形"缓存，供 DrawEntityNode 处理内联重命名触发 / 编辑态覆盖使用 ----
        // 命中区左端 = 图标右端（= nameStartLocal，紧接图标之后，包括文本本身及其后的空白）
        // 命中区右端 = 组件图标区左端 - 最小间距；若没有图标显示，则取内容区右端
        // 命中区上下 = TreeNode 行的 ItemRectMin/Max.y（屏幕坐标）
        {
            float hitLeftLocal  = nameStartLocal;
            float hitRightLocal = icons.empty() ? contentRightLocal : (iconsStartLocal - minGap);
            if (hitRightLocal < hitLeftLocal)
            {
                hitRightLocal = hitLeftLocal;   // 保底：宽度不足时至少为 0
            }
            ImVec2 winPos = ImGui::GetWindowPos();
            float scrollX = ImGui::GetScrollX();
            m_LastNameHitRect.Valid = true;
            m_LastNameHitRect.MinX = winPos.x + hitLeftLocal - scrollX;
            m_LastNameHitRect.MaxX = winPos.x + hitRightLocal - scrollX;
            m_LastNameHitRect.MinY = ImGui::GetItemRectMin().y;
            m_LastNameHitRect.MaxY = ImGui::GetItemRectMax().y;
        }

        if (icons.empty())
        {
            g.LastItemData = savedItemData;
            return;
        }

        // 保存当前光标位置
        ImVec2 savedCursorPos = ImGui::GetCursorPos();

        // 定位到图标绘制位置（与当前行同行）
        float lineY = ImGui::GetItemRectMin().y - ImGui::GetWindowPos().y + ImGui::GetScrollY();
        float iconOffsetY = (ImGui::GetTextLineHeight() - iconSize) * 0.5f;

        ImGui::SetCursorPos(ImVec2(iconsStartLocal, lineY + iconOffsetY));

        for (size_t i = 0; i < icons.size(); i++)
        {
            const Ref<Texture2D>& compIcon = *icons[i];
            if (compIcon)
            {
                UI::ImageFlipped(compIcon, ImVec2(iconSize, iconSize));
            }

            if (i < icons.size() - 1)
            {
                ImGui::SameLine(0, iconSpacing);
            }
        }

        // 恢复光标位置
        ImGui::SetCursorPos(savedCursorPos);

        // 恢复 LastItemData，使调用方的 IsItemClicked 等交互检测正确指向 TreeNode
        g.LastItemData = savedItemData;
    }

    void SceneHierarchyPanel::OnEvent(Event& event)
    {
        
    }

    std::string SceneHierarchyPanel::GenerateUniqueName(const std::string& baseName, Entity parent)
    {
        // 收集同层级下所有兄弟节点的名称
        std::vector<std::string> siblingNames;

        if (parent)
        {
            // 有父节点：遍历父节点的所有子节点
            for (UUID childID : parent.GetChildren())
            {
                Entity child = m_Scene->GetEntityWithUUID(childID);
                siblingNames.push_back(child.GetName());
            }
        }
        else
        {
            // 无父节点：遍历所有根节点（Parent == 0）
            auto view = m_Scene->GetAllEntitiesWith<NameComponent, RelationshipComponent>();
            for (auto entityID : view)
            {
                Entity entity{ entityID, m_Scene.get() };
                if (entity.GetParentUUID() == 0)
                {
                    siblingNames.push_back(entity.GetName());
                }
            }
        }

        // 检查名称是否已存在
        auto nameExists = [&siblingNames](const std::string& name) -> bool
        {
            return std::find(siblingNames.begin(), siblingNames.end(), name) != siblingNames.end();
        };

        // baseName 不重复则直接返回
        if (!nameExists(baseName))
        {
            return baseName;
        }

        // 从 (1) 开始递增直到找到不重复的名称
        int suffix = 1;
        std::string candidateName;
        do
        {
            candidateName = std::format("{} ({})", baseName, suffix);
            suffix++;
        } while (nameExists(candidateName));

        return candidateName;
    }

    bool SceneHierarchyPanel::WouldCreateCycle(Entity potentialParent, Entity potentialChild)
    {
        // 拖到根节点的兄弟位置时 potentialParent 可能是无效 Entity，一定不会形成循环
        if (!potentialParent)
        {
            return false;
        }

        // 不能将节点设为自身的子节点
        if (potentialParent == potentialChild)
        {
            return true;
        }

        // 检查 potentialParent 是否是 potentialChild 的子孙（即 potentialChild 是 potentialParent 的祖先）
        Entity current = potentialParent.GetParent();
        while (current)
        {
            if (current == potentialChild)
            {
                return true;
            }
            current = current.GetParent();
        }

        return false;
    }
}
