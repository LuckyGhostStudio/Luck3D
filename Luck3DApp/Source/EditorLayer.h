#pragma once

#include "Lucky.h"

#include "EditorDockSpace.h"
#include "Lucky/Editor/PanelManager.h"

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
        EditorDockSpace m_EditorDockSpace;  // Í£¿¿¿Õ¼ä

        Scope<PanelManager> m_PanelManager; // ±à¼­Æ÷Ãæ°å¹ÜÀíÆ÷
        
        Ref<Scene> m_Scene;
    };
}
