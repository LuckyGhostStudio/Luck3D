#pragma once

namespace Lucky::UI
{
    /// <summary>
    /// 拖拽目标端视觉反馈
    /// 
    /// 提供拖拽悬停期间目标端的高亮绘制方法。视觉形式由目标自身决定：
    /// - AssetField 等控件式目标：使用 HighlightTargetRect（矩形边框高亮）
    /// - Hierarchy 树节点排序目标：使用 HighlightTargetInsertLine（插入下划线）
    /// - Hierarchy 父节点目标：使用 HighlightTargetNode（节点背景高亮）
    /// - Scene 视口等无需高亮的目标：不调用
    /// 
    /// 使用约定：应在 BeginDragDropTarget() 内、AcceptDragDropPayload 返回非空
    /// 且业务校验通过后调用，以避免非法目标显示高亮
    /// </summary>
    class DragDropVisuals
    {
    public:
        /// <summary>
        /// 目标端矩形高亮框（用于 AssetField 等控件式目标）
        /// 内部按当前 item rect 绘制，颜色与 DrawItemActivityOutline 悬停态一致
        /// 
        /// 注意：调用方应在 AcceptDragDropPayload 时传入 ImGuiDragDropFlags_AcceptNoDrawDefaultRect
        /// 以抑制 ImGui 内置的默认黄色高亮框，避免与本函数绘制的高亮框叠加
        /// </summary>
        /// <param name="rounding">边框圆角半径（默认使用 ImGui 全局 FrameRounding）</param>
        static void HighlightTargetRect(float rounding = 0.0f);

        /// <summary>
        /// 目标端插入线高亮（用于 Hierarchy 树节点的 Before / After 放置模式）
        /// 内部按当前 item rect 的顶部（above=true）或底部（above=false）绘制一条水平线
        /// 颜色与 HighlightTargetRect 一致，风格统一
        /// </summary>
        /// <param name="above">true 表示在 item 顶部绘制（Before），false 表示在 item 底部绘制（After）</param>
        static void HighlightTargetInsertLine(bool above);

        /// <summary>
        /// 目标端整节点高亮（用于 Hierarchy 树节点的 Inside 放置模式）
        /// 内部按当前 item rect 绘制整节点高亮框，与 HighlightTargetRect 风格一致
        /// </summary>
        /// <param name="rounding">边框圆角半径（默认使用 ImGui 全局 FrameRounding）</param>
        static void HighlightTargetNode(float rounding = 0.0f);
    };
}
