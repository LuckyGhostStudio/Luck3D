#pragma once

namespace Lucky::UI
{
    /// <summary>
    /// 拖拽目标端视觉反馈
    /// 
    /// 提供拖拽悬停期间目标端的高亮绘制方法。视觉形式由目标自身决定：
    /// - AssetField 等控件式目标：使用 HighlightTargetRect（矩形边框高亮）
    /// - Hierarchy 树节点排序目标：使用 HighlightTargetInsertLine（插入下划线，待实现）
    /// - Hierarchy 父节点目标：使用 HighlightTargetNode（节点背景高亮，待实现）
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
    };
}
