#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Renderer/Framebuffer.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Core/Events/KeyEvent.h"
#include "Lucky/Core/Events/MouseEvent.h"

namespace Lucky
{
    class SceneViewportPanel : public EditorPanel
    {
    public:
        SceneViewportPanel() = default;
        SceneViewportPanel(const Ref<Scene>& scene);
        ~SceneViewportPanel() override = default;
        
        void SetScene(const Ref<Scene>& scene);

        void OnUpdate(DeltaTime dt) override;
        
        void OnBegin(const char* name) override;
        void OnEnd() override;
        void OnGUI() override;
        
        void OnEvent(Event& event) override;
    private:
        void UI_DrawViewOrientationGizmo();
        void UI_DrawGizmos();
        
        /// <summary>
        /// 按键按下时调用
        /// </summary>
        /// <param name="e">按键按下事件</param>
        /// <returns></returns>
        bool OnKeyPressed(KeyPressedEvent& e);

        /// <summary>
        /// 鼠标按键按下时调用
        /// </summary>
        /// <param name="e">鼠标按键按下事件</param>
        /// <returns></returns>
        bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
    private:
        Ref<Scene> m_Scene;
        Ref<Framebuffer> m_Framebuffer; // 帧缓冲区
        
        EditorCamera m_EditorCamera;    // 编辑器相机
        
        glm::vec2 m_ViewportSize = { 0, 0 };    // 视口大小
        glm::vec2 m_Bounds[2];                  // 视口边界（左上角，右下角）
        
        int m_GizmoType = -1;   // 当前操作的 Gizmo 类型（-1 表示无）
        int m_GizmoMode = 0;    // Gizmo 模式: 0 本地坐标 1 世界坐标
    };
}
