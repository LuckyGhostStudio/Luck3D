#pragma once

#include <Lucky.h>

#include "EditorDockSpace.h"
#include "Lucky/Editor/PanelManager.h"

namespace Lucky
{
    class EditorLayer : public Layer
    {
    public:
        EditorLayer();

        virtual ~EditorLayer() override = default;

        virtual void OnAttach() override;

        virtual void OnDetach() override;

        virtual void OnUpdate() override;

        virtual void OnImGuiRender() override;

        virtual void OnEvent(Event& event) override;

        void UI_DrawMenuBar();
    private:
        EditorDockSpace m_EditorDockSpace;  // Í£¿¿¿Õ¼ä

        Scope<PanelManager> m_PanelManager; // ±à¼­Æ÷Ãæ°å¹ÜÀíÆ÷
    };
}