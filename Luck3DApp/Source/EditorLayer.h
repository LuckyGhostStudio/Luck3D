#pragma once

#include <Lucky.h>

#include "EditorDockSpace.h"
#include "Lucky/Editor/PanelManager.h"

#include "Lucky/Renderer/EditorCamera.h"
#include "Lucky/Renderer/Framebuffer.h"

namespace Lucky
{
    class EditorLayer : public Layer
    {
    public:
        EditorLayer();

        ~EditorLayer() override = default;

        void OnAttach() override;

        void OnDetach() override;

        void OnUpdate(DeltaTime dt) override;

        void OnImGuiRender() override;

        void OnEvent(Event& event) override;

        void UI_DrawMenuBar();
    private:
        EditorDockSpace m_EditorDockSpace;  // 停靠空间

        Scope<PanelManager> m_PanelManager; // 编辑器面板管理器

        EditorCamera m_EditorCamera;        // 编辑器相机
        Ref<Framebuffer> m_Framebuffer;     // 帧缓冲区

        Ref<Texture2D> m_Texture;
        glm::vec4 m_SquareColor = { 1.0f, 1.0f, 1.0f, 1.0f };

        glm::vec2 m_Size = { 0, 0 };    // 视口大小
    };
}