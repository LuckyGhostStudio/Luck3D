#include "LightingPanel.h"

#include "Lucky/UI/Widgets.h"
#include "Lucky/UI/PropertyGrid.h"
#include "Lucky/UI/DrawUtils.h"

#include "Lucky/Editor/MaterialEditor.h"

namespace Lucky
{
    LightingPanel::LightingPanel(const Ref<Material>& skyboxMaterial)
        : m_SkyboxMaterial(skyboxMaterial)
    {
        
    }

    void LightingPanel::OnUpdate(DeltaTime dt)
    {
        
    }

    void LightingPanel::OnGUI()
    {
        ImGui::Spacing();
        
        if (UI::BeginPrimaryCollapsing("Environment"))
        {
            const std::string& skyboxMaterialName = m_SkyboxMaterial ? m_SkyboxMaterial->GetName() : "None (Material)";
            UI::PropertyObject("Skybox Material", skyboxMaterialName.c_str());
            
            UI::EndPrimaryCollapsing();
        }
        
        if (UI::BeginPrimaryCollapsing("Other Settings"))
        {
            UI::PropertyObject("Test", "TTTT");
            
            UI::EndPrimaryCollapsing();
        }
        
        // Ìì¿ÕºÐ²ÄÖÊÃæ°å TODO remove
        MaterialEditor::OnGUI(m_SkyboxMaterial);
        
        UI::Draw::HorizontalLine();
    }

    void LightingPanel::OnEvent(Event& event)
    {
        
    }
}
