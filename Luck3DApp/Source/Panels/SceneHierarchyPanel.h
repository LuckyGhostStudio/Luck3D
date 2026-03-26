#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Scene/Scene.h"

namespace Lucky
{
    class SceneHierarchyPanel : public EditorPanel
    {
    public:
        SceneHierarchyPanel() = default;
        SceneHierarchyPanel(const Ref<Scene>& scene);
        ~SceneHierarchyPanel() override = default;

        void OnUpdate(DeltaTime dt) override;
        
        void OnGUI() override;
        void DrawEntityNode(Entity entity);
        
        void OnEvent(Event& event) override;
    private:
        Ref<Scene> m_Scene;
    };
}
