#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Core/Events/Event.h"
#include "Lucky/Core/DeltaTime.h"

namespace Lucky
{
    class Layer
    {
    public:
        Layer(const std::string& name = "Layer");

        virtual ~Layer();

        /// <summary>
        /// 添加到层栈时调用
        /// </summary>
        virtual void OnAttach() {}

        /// <summary>
        /// 从层栈移除时调用
        /// </summary>
        virtual void OnDetach() {}

        /// <summary>
        /// 更新：每帧调用
        /// </summary>
        /// <param name="dt">帧间隔</param>
        virtual void OnUpdate(DeltaTime dt) {}

        /// <summary>
        /// 渲染ImGui
        /// </summary>
        virtual void OnImGuiRender() {}

        /// <summary>
        /// 事件回调函数
        /// </summary>
        /// <param name="event">事件</param>
        virtual void OnEvent(Event& event) {}

        inline const std::string& GetName() const { return m_DebugName; }
    protected:
        std::string m_DebugName;    // Layer 在 Debug 模式中的名字
    };
}