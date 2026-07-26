#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Texture.h"
#include "Lucky/UI/Theme.h"

#include <imgui/imgui.h>

#include <cstring>
#include <string>
#include <type_traits>
#include <utility>

namespace Lucky::UI
{
    // ========================================================================
    // 非 Property 语义化控件
    // ========================================================================
    
    // ---- 只用于第一层的可折叠子分组：无嵌套，可嵌套 Sub 或者 普通分组 ----
    bool BeginPrimaryCollapsing(const char* label);
    void EndPrimaryCollapsing();
    
    // bool BeginSubCollapsing(const char* label, bool defaultOpen = true);
    
    bool BeginCollapsing(const char* label, bool defaultOpen = true);
    
    void EndCollapsing();

    /// <summary>
    /// 树节点 用于 Hierarchy / Assets 等面板的树形结构
    /// </summary>
    /// <param name="icon">节点图标（可为 nullptr，不显示图标）</param>
    /// <param name="name">名称</param>
    /// <param name="defaultOpen">默认打开</param>
    /// <param name="selected">已选中</param>
    /// <param name="isLeaf">是叶节点</param>
    /// <param name="renderName">是否绘制默认名称文本（内联重命名时由调用方接管文本渲染，可传 false）</param>
    /// <returns>是否展开</returns>
    bool BeginTreeNode(const Ref<Texture2D>& icon, const char* name, bool defaultOpen = false, bool selected = false, bool isLeaf = false, bool renderName = true);

    /// <summary>
    /// 树节点（双图标版本）根据展开/折叠状态自动切换图标
    /// </summary>
    /// <param name="closedIcon">折叠时显示的图标</param>
    /// <param name="openIcon">展开时显示的图标</param>
    /// <param name="name">名称</param>
    /// <param name="defaultOpen">默认打开</param>
    /// <param name="selected">已选中</param>
    /// <param name="isLeaf">是叶节点</param>
    /// <param name="renderName">是否绘制默认名称文本（内联重命名时由调用方接管文本渲染，可传 false）</param>
    /// <returns>是否展开</returns>
    bool BeginTreeNode(const Ref<Texture2D>& closedIcon, const Ref<Texture2D>& openIcon, const char* name, bool defaultOpen = false, bool selected = false, bool isLeaf = false, bool renderName = true);

    void EndTreeNode();

    /// <summary>
    /// 内联重命名输入框的返回结果
    /// - Submitted：本帧提交（Enter 或失焦）→ CommittedName 为输入内容（未做空校验，由调用方决定处理）
    /// - Cancelled：本帧取消（Esc）→ 保留原名
    /// - 若两者都为 false，表示仍在编辑中
    /// </summary>
    struct InlineRenameResult
    {
        bool Submitted = false;
        bool Cancelled = false;
        std::string CommittedName;
    };

    /// <summary>
    /// 内联重命名输入框：在指定屏幕矩形内绘制 InputText，接管焦点/全选/Enter/Esc/失焦语义
    /// 
    /// 用于 Hierarchy / Assets 等 TreeNode 面板的原地重命名。
    /// 调用方需自行维护 buffer 与 firstFrame 状态（跨帧稳定），并根据返回的 InlineRenameResult 决定后续动作
    /// </summary>
    /// <param name="id">输入框 ImGui ID（需在同一父作用域下唯一）</param>
    /// <param name="rectMin">输入框左上角屏幕坐标</param>
    /// <param name="rectMax">输入框右下角屏幕坐标</param>
    /// <param name="buffer">InputText 缓冲区（由调用方持有，跨帧稳定）</param>
    /// <param name="bufferSize">缓冲区字节数</param>
    /// <param name="firstFrame">是否本帧首次进入编辑态（true 时自动 SetKeyboardFocusHere 并全选）</param>
    InlineRenameResult InlineRenameInput(const char* id, const ImVec2& rectMin, const ImVec2& rectMax, char* buffer, size_t bufferSize, bool firstFrame);

    /// <summary>
    /// 绘制图像
    /// </summary>
    /// <param name="texture">纹理（可为 nullptr，此时不绘制）</param>
    /// <param name="size">图像尺寸</param>
    /// <param name="uv0">UV 左上角（默认 {0, 0}）</param>
    /// <param name="uv1">UV 右下角（默认 {1, 1}）</param>
    /// <param name="tintColor">着色颜色（默认白色）</param>
    /// <param name="borderColor">边框颜色（默认无边框）</param>
    void Image(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tintColor = ImVec4(1, 1, 1, 1), const ImVec4& borderColor = ImVec4(0, 0, 0, 0));

    /// <summary>
    /// 绘制图像（OpenGL Y 翻转版本，UV 默认为 { 0, 1 } -> { 1, 0 }）适用于 Framebuffer 颜色附件等需要 Y 翻转的场景
    /// </summary>
    void ImageFlipped(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec4& tintColor = ImVec4(1, 1, 1, 1), const ImVec4& borderColor = ImVec4(0, 0, 0, 0));

    /// <summary>
    /// 绘制图像按钮
    /// </summary>
    /// <param name="texture">纹理（可为 nullptr，此时不绘制并返回 false）</param>
    /// <param name="size">图像尺寸</param>
    /// <param name="uv0">UV 左上角（默认 {0, 0}）</param>
    /// <param name="uv1">UV 右下角（默认 {1, 1}）</param>
    /// <param name="framePadding">按钮内边距（-1 表示使用当前默认 FramePadding，0 表示无内边距）</param>
    /// <param name="backgroundColor">背景颜色（默认透明）</param>
    /// <param name="tintColor">着色颜色（默认白色）</param>
    /// <returns>是否被点击</returns>
    bool ImageButton(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), int framePadding = -1, const ImVec4& backgroundColor = ImVec4(0, 0, 0, 0), const ImVec4& tintColor = ImVec4(1, 1, 1, 1));

    /// <summary>
    /// 绘制图像按钮（OpenGL Y 翻转版本，UV 默认为 { 0, 1 } -> { 1, 0 }）适用于 Framebuffer 颜色附件等需要 Y 翻转的场景
    /// </summary>
    bool ImageButtonFlipped(const Ref<Texture2D>& texture, const ImVec2& size, int framePadding = -1, const ImVec4& backgroundColor = ImVec4(0, 0, 0, 0), const ImVec4& tintColor = ImVec4(1, 1, 1, 1));

    /// <summary>
    /// 拖拽预览 tooltip：显示通用拖拽图标或禁止图标
    /// 需在 BeginDragDropSource() 和 EndDragDropSource() 之间调用
    /// </summary>
    /// <param name="rejected">是否显示为"禁止"状态（true 显示红色禁止圈，false 显示通用拖拽图标）</param>
    void DragDropPreview(bool rejected = false);

    /// <summary>
    /// 开始拖拽源（去除 ImGui tooltip 默认底框，仅显示 DragDropPreview 的图标）
    /// 需与 EndDragDropSource() 成对使用；仅当返回 true 时调用 EndDragDropSource()
    /// </summary>
    /// <param name="flags">ImGui 拖拽源标志</param>
    bool BeginDragDropSource(ImGuiDragDropFlags flags = 0);

    /// <summary>
    /// 结束拖拽源，与 BeginDragDropSource() 成对
    /// </summary>
    void EndDragDropSource();
    
    // ---- Popup ----
    
    bool BeginPopupContextWindow(const char* strID = nullptr, ImGuiPopupFlags popupFlags = 1);
    bool BeginPopupContextItem(const char* strID = nullptr, ImGuiPopupFlags popupFlags = 1);
    bool BeginPopup(const char* strID = nullptr, ImGuiPopupFlags popupFlags = 0);
    void EndPopup();
    
    bool DropdownList(int& selected, const char* const* options, int count);

    // ========================================================================
    // 内联重命名（Inline Rename）整合
    // ========================================================================

    /// <summary>
    /// RenameController::NotifyClicked 的处理结果
    /// 指导调用方是否走"正常 Select"分支
    /// </summary>
    enum class RenameClickOutcome : uint8_t
    {
        None = 0,                       // 未处理（一般不会返回）
        RegisteredAsRenameCandidate,    // 已登记为 Rename 候选（不要 Select）
        ShouldSelect                    // 应走正常 Select 分支
    };

    /// <summary>
    /// 内联重命名状态机（与容器 UI 解耦，由面板持有跨帧稳定）
    /// 
    /// 职责：
    /// - 维护"两阶段候选 → 编辑态"的跨帧状态机（按下登记 pending / 抬起未拖拽才进入编辑态）
    /// - 承接容器上报的名称文本"命中矩形"
    /// - 在编辑态调用 UI::InlineRenameInput 渲染 InputText，并处理 Submitted / Cancelled
    /// 
    /// TId 需满足：
    /// - 支持 operator== / operator!=
    /// - 支持默认构造（默认值表示"无编辑目标"）
    /// - 支持拷贝构造
    /// 
    /// 常见特化：UUID（Hierarchy）、std::filesystem::path（ProjectAssets）
    /// </summary>
    template <typename TId>
    class RenameController
    {
    public:
        RenameController() = default;

        // ---- 查询 ----

        /// <summary>
        /// 指定 (id, scopeTag) 是否正处于编辑态
        /// - scopeTag：作用域标签，用于区分"同一 TId 出现在多个视图"的场景（如 Project 面板左树 vs 右侧列表）
        ///   * 0（默认）表示"不区分作用域"，与 BeginEditing(scopeTag=0) 组合使用（Hierarchy 场景）
        ///   * 非 0 值由面板层自定义（如 Project 面板 kScopeDirTree=1 / kScopeContent=2）
        /// - 只有 (id, scopeTag) 双匹配才返回 true
        /// </summary>
        bool IsEditing(const TId& id, int scopeTag = 0) const
        {
            return m_HasEditing && m_EditingScope == scopeTag && (m_EditingID == id);
        }

        /// <summary>
        /// 是否有任意节点处于编辑态
        /// </summary>
        bool IsEditingAny() const { return m_HasEditing; }

        /// <summary>
        /// 当前编辑态的作用域标签（仅 IsEditingAny 为 true 时有意义）
        /// </summary>
        int GetEditingScope() const { return m_EditingScope; }

        // ---- 容器适配器调用（每节点绘制时按序调用） ----

        /// <summary>
        /// 上报"名称文本命中矩形"（屏幕坐标）
        /// 由容器算好后传入，供 DrawInlineInputIfEditing 覆盖 InputText，也供 NotifyClicked 判定 inHit
        /// </summary>
        void SubmitHitRect(const ImVec2& rectMin, const ImVec2& rectMax)
        {
            m_HitValid = (rectMax.x > rectMin.x) && (rectMax.y > rectMin.y);
            m_HitMin = rectMin;
            m_HitMax = rectMax;
        }

        /// <summary>
        /// 点击回调：处理"已选中 + 名称区"的两阶段登记
        /// - 已选中 + 鼠标在命中矩形内 → 登记 pending，返回 RegisteredAsRenameCandidate
        /// - 否则 → 清 pending，若其他节点处于编辑态则强退，返回 ShouldSelect
        /// </summary>
        /// <param name="id">当前节点 ID</param>
        /// <param name="wasSelectedBeforeClick">本次点击"之前"该节点是否已选中</param>
        /// <param name="currentName">当前节点名（登记 pending 时快照使用）</param>
        RenameClickOutcome NotifyClicked(const TId& id, bool wasSelectedBeforeClick, const std::string& currentName, int scopeTag = 0)
        {
            ImVec2 mouse = ImGui::GetMousePos();
            bool inHit = m_HitValid
                && mouse.x >= m_HitMin.x && mouse.x <= m_HitMax.x
                && mouse.y >= m_HitMin.y && mouse.y <= m_HitMax.y;

            if (wasSelectedBeforeClick && inHit)
            {
                m_PendingID    = id;
                m_PendingScope = scopeTag;
                m_PendingName  = currentName;
                m_HasPending   = true;
                return RenameClickOutcome::RegisteredAsRenameCandidate;
            }

            // 未选中 or 不在名称区：清 pending，若当前编辑"非本 (id, scopeTag)"则强退（避免跨节点/跨作用域串扰）
            m_HasPending = false;
            m_PendingName.clear();
            if (m_HasEditing && !(m_EditingID == id && m_EditingScope == scopeTag))
            {
                // 通知外部"当前编辑态被强制取消"（走 Cancel 语义，用于 pending create 场景丢弃占位）
                m_CancelledThisFrame = true;
                m_HasEditing = false;
            }
            return RenameClickOutcome::ShouldSelect;
        }

        /// <summary>
        /// 编辑态渲染：若本节点处于编辑态，调用 UI::InlineRenameInput 覆盖 InputText
        /// 
        /// 三种回调路径（对齐 Unity 语义 + 项目 pending create 需求）：
        /// - Enter / 已编辑失焦（Submitted）：调用 onCommit(newName)。newName 可能为空字符串，由业务层决定处理
        ///   （对齐 Unity：常规 Rename 视为"保留原名"；pending create 视为"用 InitialName 落盘"）
        /// - Esc / 未编辑失焦（Cancelled）：调用 onCancel()（若提供），用于业务层丢弃占位/回滚等
        /// - 仍在编辑中：不调用任何回调
        /// 
        /// - scopeTag：作用域标签，必须与 BeginEditing 传入值一致才会渲染 InputText
        /// - 返回 true 表示本帧发生了 Commit / Cancel（编辑态本帧结束）
        /// </summary>
        template <typename FnCommit, typename FnCancel = std::nullptr_t>
        bool DrawInlineInputIfEditing(const TId& id, const char* imguiID, FnCommit&& onCommit, FnCancel&& onCancel = nullptr, int scopeTag = 0)
        {
            if (!IsEditing(id, scopeTag) || !m_HitValid)
            {
                return false;
            }

            InlineRenameResult result = InlineRenameInput(
                imguiID, m_HitMin, m_HitMax,
                m_Buffer, sizeof(m_Buffer), m_FirstFrame);

            // 首帧过后清除 FirstFrame，避免每帧都 SetKeyboardFocusHere
            m_FirstFrame = false;

            if (result.Submitted)
            {
                m_HasEditing = false;
                onCommit(result.CommittedName);
                return true;
            }
            if (result.Cancelled)
            {
                m_HasEditing = false;
                m_CancelledThisFrame = true;
                if constexpr (!std::is_null_pointer_v<std::decay_t<FnCancel>>)
                {
                    onCancel();
                }
                return true;
            }
            return false;
        }

        // ---- 编程式控制 ----

        /// <summary>
        /// 直接进入编辑态（用于 F2 快捷键 / 创建后自动 Rename / pending create 占位）
        /// - scopeTag：作用域标签（同 IsEditing / NotifyClicked / DrawInlineInputIfEditing）
        /// </summary>
        void BeginEditing(const TId& id, const std::string& currentName, int scopeTag = 0)
        {
            m_EditingID    = id;
            m_EditingScope = scopeTag;
            m_HasEditing   = true;
            m_FirstFrame   = true;
            std::strncpy(m_Buffer, currentName.c_str(), sizeof(m_Buffer) - 1);
            m_Buffer[sizeof(m_Buffer) - 1] = '\0';
            m_HasPending = false;
            m_PendingName.clear();
            m_CancelledThisFrame = false;   // 新会话开始，清历史
        }

        /// <summary>
        /// 若指定 (id, scopeTag) 正处于编辑态则强退（用于外部信号：节点被删除等）
        /// 强退走 Cancel 语义（不触发 onCommit）
        /// </summary>
        void CancelIfEditing(const TId& id, int scopeTag = 0)
        {
            if (IsEditing(id, scopeTag))
            {
                m_HasEditing = false;
                m_CancelledThisFrame = true;
            }
        }

        /// <summary>
        /// 强制清空所有状态（编辑态 + 候选态）
        /// 若原本处于编辑态，会置 m_CancelledThisFrame = true 供外部通过 ConsumeCancelledFlag 感知
        /// </summary>
        void CancelAll()
        {
            if (m_HasEditing)
            {
                m_CancelledThisFrame = true;
            }
            m_HasEditing = false;
            m_HasPending = false;
            m_PendingName.clear();
        }

        /// <summary>
        /// 读取并清空"本帧是否发生了外部/内部 Cancel"标志
        /// 
        /// 使用场景：pending create 需要在"编辑态被强制取消"（如点击空白、选中其他节点、面板 Deselect 等）
        /// 时也丢弃占位。业务层在 OnGUI 帧末调用一次此接口，若返回 true 则清空 PendingCreate。
        /// 
        /// 注意：DrawInlineInputIfEditing 的 Esc 路径已通过 onCancel 回调直接通知业务层，不依赖此接口；
        /// 此接口专门处理"控制流走不到 DrawInlineInputIfEditing 的取消场景"（例如占位节点被折叠导致该帧不再绘制）
        /// </summary>
        bool ConsumeCancelledFlag()
        {
            bool wasCancelled = m_CancelledThisFrame;
            m_CancelledThisFrame = false;
            return wasCancelled;
        }

        // ---- 帧末统一调用一次 ----

        /// <summary>
        /// 处理"按下 → 抬起未拖拽 → 进入编辑态"两阶段仲裁
        /// 面板应在 OnGUI 帧末（所有节点绘制完成后）调用一次
        /// 
        /// 语义：
        /// - 若鼠标在按下-抬起期间发生了拖拽（IsMouseDragging）→ 取消候选，Rename 让位给 Drag
        /// - 若鼠标释放时候选仍存在 → 视为"点击已选中节点抬起"，正式进入编辑态
        ///   * 用 PendingName 快照初始化 InputText 缓冲（隔离抬起前 name 变化等边界情况）
        ///   * 标记 FirstFrame，让 InlineRenameInput 首帧 SetKeyboardFocusHere + AutoSelectAll
        /// - 释放后无论是否进入编辑态都必须清空 pending，防止跨点击串扰
        /// 
        /// 副作用：本方法末尾重置 m_HitValid，避免下一帧节点未绘制时使用上一帧过期矩形
        /// </summary>
        void FlushPending()
        {
            if (m_HasPending)
            {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    m_HasPending = false;
                    m_PendingName.clear();
                }
                else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    m_EditingID    = m_PendingID;
                    m_EditingScope = m_PendingScope;
                    m_HasEditing   = true;
                    m_FirstFrame   = true;
                    std::strncpy(m_Buffer, m_PendingName.c_str(), sizeof(m_Buffer) - 1);
                    m_Buffer[sizeof(m_Buffer) - 1] = '\0';

                    m_HasPending = false;
                    m_PendingName.clear();
                }
            }

            // 帧末重置命中矩形有效标志：下一帧节点绘制时会通过 SubmitHitRect 重新填充；
            // 若某节点这帧未再被绘制（如折叠或滚出可视区），则 m_HitValid 保持 false，
            // DrawInlineInputIfEditing 不会误用上一帧的过期矩形
            m_HitValid = false;
        }

    private:
        // ---- 编辑态 ----
        bool  m_HasEditing   = false;
        TId   m_EditingID{};
        int   m_EditingScope = 0;       // 作用域标签（0 = 不区分，Hierarchy 用；非 0 = 面板层自定义）
        bool  m_FirstFrame   = false;
        char  m_Buffer[128]  = { 0 };

        // ---- 两阶段候选 ----
        bool         m_HasPending   = false;
        TId          m_PendingID{};
        int          m_PendingScope = 0;
        std::string  m_PendingName;

        // ---- 帧内命中矩形（SubmitHitRect 写入，DrawInlineInputIfEditing / NotifyClicked 消费） ----
        bool   m_HitValid = false;
        ImVec2 m_HitMin{};
        ImVec2 m_HitMax{};

        // ---- 帧内 Cancel 标志（Esc / 外部强退 / 跨节点点击 时置 true） ----
        // ConsumeCancelledFlag 读取后清零，供业务层在帧末感知"是否需要丢弃 pending create 占位"
        bool m_CancelledThisFrame = false;
    };

    // ---- 内部辅助（供 BeginRenamableTreeNode 使用）----
    namespace Detail
    {
        /// <summary>
        /// 编辑态开始：向下一 Item 施加 Disabled ItemFlag，屏蔽 TreeNode 的鼠标交互
        /// 详见 SceneHierarchyPanel.cpp 中 isRenaming 分支的注释
        /// </summary>
        void PushRenameEditingItemFlag();

        /// <summary>
        /// 编辑态收尾：Pop 上一次 PushRenameEditingItemFlag 的 flag，并对刚绘制完的 TreeNode 调用
        /// SetItemAllowOverlap，允许 InputText 覆盖交互
        /// </summary>
        void PopRenameEditingItemFlagAndAllowOverlap();
    }

    /// <summary>
    /// 可重命名树节点（单图标版本）：BeginTreeNode + RenameController 的组合封装
    /// 
    /// 内部完成：
    /// - 若节点处于编辑态：自动 PushItemFlag(Disabled) + renderName=false + SetItemAllowOverlap
    /// - 调用 drawRightSide 让面板绘制右侧图标 / 计算并上报名称命中矩形
    /// - 调用 rename.DrawInlineInputIfEditing 处理编辑态渲染
    /// - 若发生 IsItemClicked，调用 rename.NotifyClicked 并通过 outClickOutcome 传回结果
    /// 
    /// 面板仍需自行完成：
    /// - drawRightSide 回调内绘制右侧内容并调用 rename.SubmitHitRect 上报名称命中矩形
    /// - 根据 outClickOutcome 决定是否走"正常 Select"分支
    /// - 编辑态下的子节点递归、TreePop 等业务逻辑
    /// </summary>
    /// <param name="icon">节点图标</param>
    /// <param name="name">名称（含 ## 部分，用于 TreeNode ID）</param>
    /// <param name="displayName">显示名 / 快照名（登记 pending 时使用，通常等于 name 去掉 ##）</param>
    /// <param name="id">唯一 ID（TId）</param>
    /// <param name="selected">是否选中（本次点击前的状态）</param>
    /// <param name="isLeaf">是否叶节点</param>
    /// <param name="defaultOpen">默认展开</param>
    /// <param name="rename">Rename 状态机（面板持有）</param>
    /// <param name="onCommit">改名回调：void(const std::string& newName)</param>
    /// <param name="drawRightSide">右侧图标绘制 + 命中矩形上报回调，签名 void()；传 nullptr 时适配器按 BeginTreeNode 内部布局公式自动计算命中矩形（图标右侧 → 内容区右端）</param>
    /// <param name="renameImguiID">InputText 的 ImGui ID（需在同一父作用域下唯一，默认 "##InlineRename"）</param>
    /// <param name="outClickOutcome">点击结果输出（可为 nullptr）</param>
    /// <param name="scopeTag">作用域标签（0 = 不区分，用于 Hierarchy；非 0 由面板层自定义，用于 Project 面板左树/右侧列表隔离）</param>
    /// <param name="onCancel">Esc / 未编辑失焦回调（可选），签名 void()；用于 pending create 场景的"丢弃占位"分派</param>
    /// <returns>TreeNode 是否展开</returns>
    template <typename TId, typename FnCommit, typename FnRightSide = std::nullptr_t, typename FnCancel = std::nullptr_t>
    bool BeginRenamableTreeNode(
        const Ref<Texture2D>&       icon,
        const char*                 name,
        const std::string&          displayName,
        const TId&                  id,
        bool                        selected,
        bool                        isLeaf,
        bool                        defaultOpen,
        RenameController<TId>&      rename,
        FnCommit&&                  onCommit,
        FnRightSide                 drawRightSide = nullptr,
        const char*                 renameImguiID = "##InlineRename",
        RenameClickOutcome*         outClickOutcome = nullptr,
        int                         scopeTag = 0,
        FnCancel&&                  onCancel = nullptr)
    {
        bool isRenaming = rename.IsEditing(id, scopeTag);

        // 提前抓取节点起绘 X（含当前 Indent 层数）：BeginTreeNode 内部 TreePush 会增加 DC.Indent，之后再取就偏了
        // 用于默认分支重算命中矩形的左边界（对齐图标右侧）
        const float nodeStartScreenX = ImGui::GetCursorScreenPos().x;

        if (isRenaming)
        {
            Detail::PushRenameEditingItemFlag();
        }

        bool opened = BeginTreeNode(icon, name, defaultOpen, selected, isLeaf, /*renderName*/ !isRenaming);

        if (isRenaming)
        {
            Detail::PopRenameEditingItemFlagAndAllowOverlap();
        }

        // 右侧内容 + 命中矩形上报：交给面板回调
        if constexpr (!std::is_null_pointer_v<std::decay_t<FnRightSide>>)
        {
            drawRightSide();
        }
        else
        {
            // 无右侧回调：默认按 BeginTreeNode 内部布局公式重算"名称文本区"命中矩形
            // 
            // 【为何不能直接用 GetItemRectMin/Max】：
            //   BeginTreeNode 内部使用 SpanFullWidth | SpanAvailWidth，ItemRectMin.x = WorkRect.Min.x（跨越到窗口最左），
            //   ItemRectMax.x = WorkRect.Max.x（跨越到窗口最右），若直接用作命中区，编辑框会横跨整行、越过图标位置。
            // 
            // 【布局公式】（严格对齐 BeginTreeNodeInternal 的绘制序列）：
            //   [节点起绘 X（含 Indent）] + [箭头宽度 = FontSize]
            //   + [ArrowToIconSpacing] + [iconSize = FontSize - TreeNodeIconSizeShrink]
            //   + [IconToTextSpacing]  → 名称文本起点
            //   右边界取内容区右端（WorkRect.Max.x - ItemInnerSpacing.x 留一点视觉边距）
            //   上下沿取 ItemRect.MinY/MaxY（Y 方向本来就正确）
            const float fontSize = ImGui::GetFontSize();
            const float iconSize = fontSize - Theme::Layout::TreeNodeIconSizeShrink;
            const float nameStartScreenX = nodeStartScreenX
                + fontSize
                + Theme::Layout::TreeNodeArrowToIconSpacing
                + iconSize
                + Theme::Layout::TreeNodeIconToTextSpacing;

            // 右边界：使用公开 API 计算内容区右侧屏幕 X（避免引入 imgui_internal.h）
            //   GetWindowPos().x + GetWindowContentRegionMax().x = 内容区右侧屏幕坐标
            //   （GetWindowContentRegionMax() 返回相对窗口原点的坐标）
            //   再扣少 ItemInnerSpacing.x 作为右侧视觉边距
            const float rightScreenX = ImGui::GetWindowPos().x
                + ImGui::GetWindowContentRegionMax().x
                - ImGui::GetStyle().ItemInnerSpacing.x;

            ImVec2 rectMin(nameStartScreenX, ImGui::GetItemRectMin().y);
            ImVec2 rectMax(rightScreenX > nameStartScreenX ? rightScreenX : nameStartScreenX,
                           ImGui::GetItemRectMax().y);
            rename.SubmitHitRect(rectMin, rectMax);
        }

        // 编辑态：InputText 覆盖（携带 scopeTag + onCancel 分派）
        rename.DrawInlineInputIfEditing(id, renameImguiID,
            std::forward<FnCommit>(onCommit),
            std::forward<FnCancel>(onCancel),
            scopeTag);

        // 点击处理
        RenameClickOutcome outcome = RenameClickOutcome::None;
        if (!isRenaming && ImGui::IsItemClicked())
        {
            outcome = rename.NotifyClicked(id, selected, displayName, scopeTag);
        }
        if (outClickOutcome)
        {
            *outClickOutcome = outcome;
        }

        return opened;
    }

    /// <summary>
    /// 可重命名树节点（双图标版本，根据展开/折叠状态自动切换图标）
    /// </summary>
    template <typename TId, typename FnCommit, typename FnRightSide = std::nullptr_t, typename FnCancel = std::nullptr_t>
    bool BeginRenamableTreeNode(
        const Ref<Texture2D>&       closedIcon,
        const Ref<Texture2D>&       openIcon,
        const char*                 name,
        const std::string&          displayName,
        const TId&                  id,
        bool                        selected,
        bool                        isLeaf,
        bool                        defaultOpen,
        RenameController<TId>&      rename,
        FnCommit&&                  onCommit,
        FnRightSide                 drawRightSide = nullptr,
        const char*                 renameImguiID = "##InlineRename",
        RenameClickOutcome*         outClickOutcome = nullptr,
        int                         scopeTag = 0,
        FnCancel&&                  onCancel = nullptr)
    {
        bool isRenaming = rename.IsEditing(id, scopeTag);

        // 提前抓取节点起绘 X（含当前 Indent 层数）：详见单图标版本相同位置的注释
        const float nodeStartScreenX = ImGui::GetCursorScreenPos().x;

        if (isRenaming)
        {
            Detail::PushRenameEditingItemFlag();
        }

        bool opened = BeginTreeNode(closedIcon, openIcon, name, defaultOpen, selected, isLeaf, /*renderName*/ !isRenaming);

        if (isRenaming)
        {
            Detail::PopRenameEditingItemFlagAndAllowOverlap();
        }

        if constexpr (!std::is_null_pointer_v<std::decay_t<FnRightSide>>)
        {
            drawRightSide();
        }
        else
        {
            // 无右侧回调：默认按 BeginTreeNode 内部布局公式重算"名称文本区"命中矩形
            // 详见单图标版本相同位置的详细注释
            const float fontSize = ImGui::GetFontSize();
            const float iconSize = fontSize - Theme::Layout::TreeNodeIconSizeShrink;
            const float nameStartScreenX = nodeStartScreenX
                + fontSize
                + Theme::Layout::TreeNodeArrowToIconSpacing
                + iconSize
                + Theme::Layout::TreeNodeIconToTextSpacing;

            // 右边界：使用公开 API 计算内容区右侧屏幕 X（避免引入 imgui_internal.h）
            //   GetWindowPos().x + GetWindowContentRegionMax().x = 内容区右侧屏幕坐标
            //   （GetWindowContentRegionMax() 返回相对窗口原点的坐标）
            //   再扣少 ItemInnerSpacing.x 作为右侧视觉边距
            const float rightScreenX = ImGui::GetWindowPos().x
                + ImGui::GetWindowContentRegionMax().x
                - ImGui::GetStyle().ItemInnerSpacing.x;

            ImVec2 rectMin(nameStartScreenX, ImGui::GetItemRectMin().y);
            ImVec2 rectMax(rightScreenX > nameStartScreenX ? rightScreenX : nameStartScreenX,
                           ImGui::GetItemRectMax().y);
            rename.SubmitHitRect(rectMin, rectMax);
        }

        rename.DrawInlineInputIfEditing(id, renameImguiID,
            std::forward<FnCommit>(onCommit),
            std::forward<FnCancel>(onCancel),
            scopeTag);

        RenameClickOutcome outcome = RenameClickOutcome::None;
        if (!isRenaming && ImGui::IsItemClicked())
        {
            outcome = rename.NotifyClicked(id, selected, displayName, scopeTag);
        }
        if (outClickOutcome)
        {
            *outClickOutcome = outcome;
        }

        return opened;
    }
}
