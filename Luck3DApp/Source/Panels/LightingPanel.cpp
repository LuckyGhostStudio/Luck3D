#include "LightingPanel.h"

#include "Lucky/UI/Widgets.h"
#include "Lucky/UI/PropertyGrid.h"
#include "Lucky/UI/DrawUtils.h"

#include "Lucky/Editor/MaterialEditor.h"

namespace Lucky
{
    LightingPanel::LightingPanel(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
        
    }

    void LightingPanel::SetScene(const Ref<Scene>& scene)
    {
        m_Scene = scene;
    }

    void LightingPanel::OnUpdate(DeltaTime dt)
    {
        
    }

    void LightingPanel::OnGUI()
    {
        if (!m_Scene)
        {
            return;
        }
        
        EnvironmentSettings& env = m_Scene->GetEnvironmentSettings();
        
        ImGui::Spacing();
        
        // ======== Environment ========
        if (UI::BeginPrimaryCollapsing("Environment"))
        {
            // 天空盒材质显示
            const Ref<Material>& skyboxMaterial = env.SkyboxMaterial;
            const std::string& skyboxMaterialName = skyboxMaterial ? skyboxMaterial->GetName() : "None (Material)";
            UI::PropertyObject("Skybox Material", skyboxMaterialName.c_str());
            
            UI::EndPrimaryCollapsing();
        }
        
        // ======== Environment Lighting ========
        if (UI::BeginPrimaryCollapsing("Environment Lighting"))
        {
            // Source 下拉框
            static const char* sourceOptions[] = { "Skybox", "Color" };
            int sourceIndex = static_cast<int>(env.Source);
            if (UI::PropertyCombo("Source", sourceIndex, sourceOptions, 2))
            {
                env.Source = static_cast<AmbientSource>(sourceIndex);
            }
            
            // 根据 Source 显示不同参数
            if (env.Source == AmbientSource::Skybox)
            {
                // Skybox 模式：显示 Diffuse Intensity
                UI::PropertyFloat("Diffuse Intensity", env.DiffuseIntensity, 0.01f, 0.0f, 8.0f);
            }
            else
            {
                // Color 模式：显示 Ambient Color
                UI::PropertyColor("Ambient Color", env.AmbientColor);
            }
            
            UI::EndPrimaryCollapsing();
        }
        
        // ======== Environment Reflections ========
        if (UI::BeginPrimaryCollapsing("Environment Reflections"))
        {
            UI::PropertyFloat("Specular Intensity", env.SpecularIntensity, 0.01f, 0.0f, 1.0f);
            
            // Resolution 下拉框
            static const char* resolutionOptions[] = { "16", "32", "64", "128", "256", "512", "1024", "2048" };
            static const int resolutionValues[] = { 16, 32, 64, 128, 256, 512, 1024, 2048 };
            
            // 根据当前值找到对应的索引
            int resolutionIndex = 3; // 默认 128
            for (int i = 0; i < 8; i++)
            {
                if (resolutionValues[i] == env.ReflectionResolution)
                {
                    resolutionIndex = i;
                    break;
                }
            }
            
            if (UI::PropertyCombo("Resolution", resolutionIndex, resolutionOptions, 8))
            {
                env.ReflectionResolution = resolutionValues[resolutionIndex];
            }
            
            UI::EndPrimaryCollapsing();
        }
        
        // ======== Skybox Material Editor ========
        const Ref<Material>& skyboxMat = env.SkyboxMaterial;
        if (skyboxMat)
        {
            MaterialEditor::OnGUI(skyboxMat);
        }
        
        UI::Draw::HorizontalLine();
    }

    void LightingPanel::OnEvent(Event& event)
    {
        
    }
}
