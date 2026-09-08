#pragma once

namespace Lucky
{
    /// <summary>
    /// 编辑器全局工具条：MainMenuBar 之下、DockSpace 之上，贴顶显示 Play / Pause 两个 Toggle 按钮
    /// 
    /// 按钮模型：
    /// - Play：切换"是否运行时"，蓝色背景表示已进入运行时（点击退出即 Stop）
    /// - Pause：切换"暂停意图"（PauseArmed）
    ///   - Edit 态下点 Pause：仅高亮，不动 Scene（"预暂停"）；下次 Play 会直接进 Pause 态
    ///   - 运行态下点 Pause：Scene 立即 Pause / 恢复 Play
    /// </summary>
    class EditorToolbar
    {
    public:
        void ImGuiRender();

        /// <summary>
        /// 工具条实际高度：取设计目标与 ImGui 全局 WindowMinSize.y 的较大值
        /// 供 EditorLayer 从 viewport WorkPos / WorkSize 中扣除，保证 DockSpace 起点与工具条底部严丝合缝
        /// </summary>
        static float GetHeight();
    private:
        bool m_PauseArmed = false;  // 暂停意图位：Edit 态下 Pause 高亮但不影响 Scene

        static constexpr float s_DesiredHeight = 42.0f; // 工具条设计目标高度（实际高度不会小于全局 WindowMinSize.y）
    };
}
