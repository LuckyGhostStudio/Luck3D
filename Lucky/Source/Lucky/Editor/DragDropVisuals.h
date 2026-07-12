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
        /// 
        /// 视觉样式（Unity Hierarchy 风格）：
        /// - 左端点在 (x, y) 处绘制一个空心小圆圈，指示"插入的层级锚点"
        /// - 从圆圈右侧起延伸一条水平直线，直到当前窗口内容区右边界
        /// - y 由 above 决定：当前 item rect 的顶部（Before）或底部（After）
        /// - 圆圈与直线均使用与激活边框一致的蓝色高亮
        /// </summary>
        /// <param name="x">线左端点（也是圆圈中心）的屏幕坐标 X，通常对齐到目标层级的图标左侧</param>
        /// <param name="above">true 表示在 item 顶部绘制（Before），false 表示在 item 底部绘制（After）</param>
        static void HighlightTargetInsertLine(float x, bool above);

        /// <summary>
        /// 目标端插入线高亮（显式 y 版本）：直接指定屏幕坐标 (x, y) 绘制，不依赖当前 LastItem
        /// 
        /// 使用场景：当调用方需要"延后到帧末统一绘制"以确保 Insert 线位于所有 header 之上时，
        /// 应在遍历目标节点时提前计算好 y（通常 = above ? itemMin.y : itemMax.y），
        /// 然后在整个树遍历结束后再调用此重载绘制，避免被后续兄弟节点的选中态 header 遮挡
        /// 
        /// 视觉样式与 HighlightTargetInsertLine(x, above) 完全一致
        /// </summary>
        /// <param name="x">线左端点（也是圆圈中心）的屏幕坐标 X</param>
        /// <param name="y">线（圆圈中心）的屏幕坐标 Y</param>
        static void HighlightTargetInsertLineAt(float x, float y);

        /// <summary>
        /// 目标端"孤立小圆圈"（无线段），用于 Hierarchy 拖拽 After 模式发生 X 层级回退时，
        /// 在被跨越的祖先节点行底部额外指示"如果鼠标右移则会落到那个更深层级"
        /// 
        /// 视觉样式：与 HighlightTargetInsertLineAt 的左端圆圈完全一致（同色、同半径、同线宽），
        /// 仅绘制圆圈，不绘制水平线
        /// </summary>
        /// <param name="x">圆圈中心的屏幕坐标 X（通常与主线左端圆圈同 X）</param>
        /// <param name="y">圆圈中心的屏幕坐标 Y（通常 = 被跨越祖先节点 header 的 ItemRectMax.y）</param>
        static void HighlightTargetInsertDotAt(float x, float y);

        /// <summary>
        /// 目标端整节点高亮（用于 Hierarchy 树节点的 Inside 放置模式）
        /// 内部按当前 item rect 绘制整节点高亮框，与 HighlightTargetRect 风格一致
        /// </summary>
        /// <param name="rounding">边框圆角半径（默认使用 ImGui 全局 FrameRounding）</param>
        static void HighlightTargetNode(float rounding = 0.0f);

        /// <summary>
        /// 目标端整节点高亮（显式坐标版本）：不依赖当前 LastItem，直接指定矩形绘制
        /// 
        /// 使用场景：当调用方需要"延后到帧末统一绘制"以确保高亮框位于所有 header 之上时，
        /// 应在遍历目标节点时提前记录矩形（通常 = 目标 header 的 ItemRectMin / Max），
        /// 然后在整个树遍历结束后再调用此重载绘制
        /// 
        /// 视觉：使用与 Insert 线一致的蓝色描边（s_InsertLineColor），外沿与传入矩形完全等大（不外扩）
        /// </summary>
        /// <param name="minX">矩形左上角 X（通常 = 目标 header ItemRectMin.x）</param>
        /// <param name="minY">矩形左上角 Y（通常 = 目标 header ItemRectMin.y）</param>
        /// <param name="maxX">矩形右下角 X（通常 = 目标 header ItemRectMax.x）</param>
        /// <param name="maxY">矩形右下角 Y（通常 = 目标 header ItemRectMax.y）</param>
        /// <param name="rounding">边框圆角半径（默认使用 ImGui 全局 FrameRounding）</param>
        static void HighlightTargetNodeAt(float minX, float minY, float maxX, float maxY, float rounding = 0.0f);
    };
}
