#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Core/Events/Event.h"

namespace Lucky
{
    /// <summary>
    /// 编辑器面板基类
    /// </summary>
    class EditorPanel
    {
    public:
        virtual ~EditorPanel() = default;

        /// <summary>
        /// 渲染 ImGui 时调用
        /// </summary>
        /// <param name="name">面板名称</param>
        /// <param name="isOpen">是否打开</param>
        virtual void OnImGuiRender(const char* name, bool& isOpen);

        /// <summary>
        /// 事件处理函数
        /// </summary>
        /// <param name="event">事件</param>
        virtual void OnEvent(Event& event) {}
    protected:
        virtual void OnBegin(const char* name);
        virtual void OnGUI() = 0;
        virtual void OnEnd();
    };
}
