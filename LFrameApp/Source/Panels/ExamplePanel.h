#pragma once

#include <Lucky/Editor/EditorPanel.h>

namespace Lucky
{
    class ExamplePanel : public EditorPanel
    {
    public:
        ExamplePanel();
        ~ExamplePanel() = default;

        void OnGUI() override;

        /// <summary>
        /// 事件处理函数
        /// </summary>
        /// <param name="event">事件</param>
        void OnEvent(Event& event) override;
    };
}