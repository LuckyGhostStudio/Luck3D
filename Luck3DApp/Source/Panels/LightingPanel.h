#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Renderer/Material.h"

namespace Lucky
{
    class LightingPanel : public EditorPanel
    {
    public:
        LightingPanel(const Ref<Scene>& scene);
        ~LightingPanel() override = default;
        
        void SetScene(const Ref<Scene>& scene);
        
        void OnUpdate(DeltaTime dt) override;
        void OnGUI() override;
        
        void OnEvent(Event& event) override;
    private:
        Ref<Scene> m_Scene;
    };
}
