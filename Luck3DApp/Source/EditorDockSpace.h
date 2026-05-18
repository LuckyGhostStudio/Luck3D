#pragma once

#include "EditorLayoutManager.h"

namespace Lucky
{
    typedef unsigned int uint32_t;

    /// <summary>
    /// 编辑器停靠空间
    /// </summary>
    class EditorDockSpace
    {
    public:
        EditorDockSpace(bool fullScreen = true);

        /// <summary>
        /// 绘制 DockSpace
        /// </summary>
        void ImGuiRender();

        /// <summary>
        /// 请求重置为默认布局（下一帧生效）
        /// </summary>
        void ResetToDefaultLayout();
    private:
        bool m_IsFullScreen = true;             // 是否全屏
        uint32_t m_Flags;                       // DockSpace 标志（ImGuiDockNodeFlags）
        uint32_t m_WindowFlags;                 // DockSpace 窗口标志（ImGuiWindowFlags）
        bool m_ResetLayout = false;             // 是否需要重置布局
        EditorLayoutManager m_LayoutManager;    // 布局管理器
    };
}