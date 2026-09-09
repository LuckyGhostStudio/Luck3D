#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Renderer/SceneRenderer.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Scene/SceneManager.h"

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// Game 面板：使用场景内 Primary CameraComponent 渲染游戏视角
    /// 顶部工具栏提供分辨率模式选择（Free Aspect / Aspect Ratio / Fixed Resolution）
    /// 不绘制任何编辑器 Overlay（Grid / Gizmo / Outline / Frustum）
    /// 无主相机时显示纯黑
    /// </summary>
    class GameViewportPanel : public EditorPanel
    {
    public:
        GameViewportPanel() = default;
        GameViewportPanel(const Ref<Scene>& scene);
        ~GameViewportPanel() override;

        void OnUpdate(DeltaTime dt) override;
        void OnGUI() override;
    private:
        Ref<Scene> m_Scene;
        Ref<SceneRenderer> m_SceneRenderer;     // 场景渲染器（持有 FBO / 状态）

        glm::vec2 m_ViewportSize = { 0, 0 };    // ToolBar 下方可用区域大小
        glm::uvec2 m_LastRTSize = { 0, 0 };     // 上一帧的 RT 尺寸，用于变更检测

        int m_ResolutionIndex = 1;              // 当前选中的分辨率预设索引

        // SceneManager 订阅句柄：ctor 中 Subscribe，dtor 中 Unsubscribe
        SceneManager::SubscriptionHandle m_SceneChangedSub = 0;
    };
}
