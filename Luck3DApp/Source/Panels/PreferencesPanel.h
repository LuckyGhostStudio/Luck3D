#pragma once

#include "Lucky/Editor/EditorPanel.h"

namespace Lucky
{
    /// <summary>
    /// 偏好设置面板
    /// </summary>
    class PreferencesPanel : public EditorPanel
    {
    public:
        PreferencesPanel();
        ~PreferencesPanel() override = default;
        
        void OnUpdate(DeltaTime dt) override;
        void OnGUI() override;
        
        void OnEvent(Event& event) override;
    private:
        /// <summary>
        /// 绘制颜色设置页
        /// </summary>
        void DrawColorsPage();
    private:
        /// <summary>
        /// 当前选中的分类索引（0 = Colors）
        /// </summary>
        int m_SelectedCategory = 0;
    };
}