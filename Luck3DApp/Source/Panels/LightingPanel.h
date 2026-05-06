#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Renderer/Material.h"

namespace Lucky
{
    class LightingPanel : public EditorPanel
    {
    public:
        LightingPanel(const Ref<Material>& skyboxMaterial);
        ~LightingPanel() override = default;
        
        void OnUpdate(DeltaTime dt) override;
        void OnGUI() override;
        
        void OnEvent(Event& event) override;
    private:
        Ref<Material> m_SkyboxMaterial;
    };
}
