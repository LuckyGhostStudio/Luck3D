#pragma once

namespace Lucky
{
    typedef unsigned int uint32_t;

    /// <summary>
    /// 编辑器布局管理器：负责管理编辑器面板的停靠布局
    /// </summary>
    class EditorLayoutManager
    {
    public:
        EditorLayoutManager() = default;

        /// <summary>
        /// 应用默认布局
        /// </summary>
        /// <param name="dockspaceID">DockSpace 节点 ID</param>
        void ApplyDefaultLayout(uint32_t dockspaceID);
    };
}
