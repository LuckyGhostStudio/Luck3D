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

        glm::vec2 m_ViewportSize = { 0, 0 };    // 视口大小

        // SceneManager 订阅句柄：ctor 中 Subscribe，dtor 中 Unsubscribe
        SceneManager::SubscriptionHandle m_SceneChangedSub = 0;
    };
}
