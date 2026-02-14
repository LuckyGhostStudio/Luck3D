#pragma once

#include <vector>

#include "Lucky/Core/Base.h"
#include "Layer.h"

namespace Lucky
{
    /*
     * 普通层在列表前半部分 覆盖层在列表后半部分
     * 覆盖层：GUI、Guizmo 等
     */

    /// <summary>
    /// 层栈
    /// </summary>
    class LayerStack
    {
    public:
        LayerStack();
        ~LayerStack();

        /// <summary>
        /// 添加普通层
        /// </summary>
        /// <param name="layer">层</param>
        void PushLayer(Layer* layer);

        /// <summary>
        /// 添加覆盖层到栈顶
        /// </summary>
        /// <param name="overlay">覆盖层</param>
        void PushOverlay(Layer* overlay);

        /// <summary>
        /// 移除普通层
        /// </summary>
        /// <param name="layer">普通层</param>
        void PopLayer(Layer* layer);

        /// <summary>
        /// 移除覆盖层
        /// </summary>
        /// <param name="overlay">覆盖层</param>
        void PopOverlay(Layer* overlay);

        std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
        std::vector<Layer*>::iterator end() { return m_Layers.end(); }
    private:
        std::vector<Layer*> m_Layers;           // 层列表
        unsigned int m_LayerInsertIndex = 0;    // 插入位置索引
    };
}