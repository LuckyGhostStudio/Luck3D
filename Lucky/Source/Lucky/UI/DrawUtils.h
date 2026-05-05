#pragma once

namespace Lucky::UI
{
    namespace Draw
    {
        /// <summary>
        /// 绘制下划线分隔线
        /// </summary>
        /// <param name="fullWidth">是否占满窗口宽度（false 则仅占内容区域宽度）</param>
        /// <param name="lineWidth">线宽</param>
        /// <param name="offsetX">水平偏移</param>
        /// <param name="offsetY">垂直偏移（默认 -1.0f，在当前行上方 1px）</param>
        void Underline(bool fullWidth = false, float lineWidth = 1.0f, float offsetX = 0.0f, float offsetY = -1.0f);
        
        /// <summary>
        /// 绘制控件顶部阴影线
        /// </summary>
        void ItemTopShadow();
        
        /// <summary>
        /// 绘制控件底部阴影线
        /// </summary>
        void ItemBottomShadow();
        
        void HorizontalLine(float alpha = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f);
    }

    /// <summary>
    /// 绘制控件活动状态边框（焦点高亮、悬停效果）
    /// 在控件绘制之后立即调用
    /// </summary>
    /// <param name="rounding">边框圆角半径（默认使用 ImGui 全局 FrameRounding）</param>
    void DrawItemActivityOutline(float rounding = 0.0f);
}