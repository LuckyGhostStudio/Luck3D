#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Renderer/Framebuffer.h"
#include "Lucky/Scene/Scene.h"

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
        Ref<Scene> m_Scene;
        Ref<Framebuffer> m_Framebuffer; // 帧缓冲区
        
        EditorCamera m_EditorCamera;    // 编辑器相机
        
        glm::vec2 m_ViewportSize = { 0, 0 };    // 视口大小
    };
}
