#pragma once

#include "Lucky/Core/DeltaTime.h"
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
        /// 更新：每帧调用
        /// </summary>
        /// <param name="dt">帧间隔</param>
        virtual void OnUpdate(DeltaTime dt) = 0;
        
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

        /// <summary>
        /// 本面板当前是否被鼠标悬停（含子窗口）。
        /// 由基类在 OnImGuiRender 的 OnGUI 之后统一捕获；供子类 OnEvent 判定
        /// 鼠标类事件（滚轮缩放、拖拽、点击等）是否应落到本面板。
        /// 注意：状态是上一帧 UI 结束时记下的，相对当前鼠标最多滞后一帧。
        /// </summary>
        bool IsHovered() const { return m_IsHovered; }

        /// <summary>
        /// 本面板当前是否处于聚焦（含根窗口与所有子窗口）。
        /// 由基类在 OnImGuiRender 的 OnGUI 之后统一捕获；供子类 OnEvent 判定
        /// 键盘类快捷键（F2 / Delete / Ctrl+R 等）的作用域。
        /// </summary>
        bool IsFocused() const { return m_IsFocused; }
    protected:
        void SetFlags(int flags) { m_WindowFlags = flags; }
        virtual void OnBegin(const char* name);
        virtual void OnEnd();
        virtual void OnGUI() = 0;
    private:
        int m_WindowFlags = 0;

        bool m_IsHovered = false;   // 当前帧面板是否被鼠标悬停（基类在 OnImGuiRender 内更新）
        bool m_IsFocused = false;   // 当前帧面板是否处于聚焦（基类在 OnImGuiRender 内更新）
    };
}
