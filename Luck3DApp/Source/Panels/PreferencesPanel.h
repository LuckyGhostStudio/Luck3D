#pragma once

namespace Lucky
{
    /// <summary>
    /// 偏好设置面板：独立面板，不走 PanelManager
    /// </summary>
    class PreferencesPanel
    {
    public:
        PreferencesPanel() = default;
        
        /// <summary>
        /// 渲染偏好设置面板
        /// </summary>
        void OnImGuiRender();
        
        /// <summary>
        /// 打开偏好设置面板
        /// </summary>
        void Open() { m_IsOpen = true; }
        
        /// <summary>
        /// 面板是否打开
        /// </summary>
        bool IsOpen() const { return m_IsOpen; }
        
    private:
        /// <summary>
        /// 绘制颜色设置页
        /// </summary>
        void DrawColorsPage();
        
    private:
        bool m_IsOpen = false;
        
        /// <summary>
        /// 当前选中的分类索引（0 = Colors）
        /// </summary>
        int m_SelectedCategory = 0;
    };
}