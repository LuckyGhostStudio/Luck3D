#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Texture.h"

#include <imgui/imgui.h>

#include <string>

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
    InlineRenameResult InlineRenameInput(const char* id,
                                         const ImVec2& rectMin,
                                         const ImVec2& rectMax,
                                         char* buffer, size_t bufferSize,
                                         bool firstFrame);

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
}