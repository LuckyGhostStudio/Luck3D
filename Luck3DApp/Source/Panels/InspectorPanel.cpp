#include "InspectorPanel.h"

#include "Lucky/Scene/SelectionManager.h"

#include "Lucky/Scene/Components/Components.h"

#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/RenderContext.h"

#include "Lucky/Utils/PlatformUtils.h"

#include "Lucky/UI/Controls.h"
#include "Lucky/UI/PropertyGrid.h"
#include "Lucky/UI/Widgets.h"

#include "Lucky/Editor/MaterialEditor.h"

#include <glm/gtc/type_ptr.hpp>

namespace Lucky
{
    InspectorPanel::InspectorPanel(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
        
    }

    void InspectorPanel::SetScene(const Ref<Scene>& scene)
    {
        m_Scene = scene;
    }

    void InspectorPanel::OnUpdate(DeltaTime dt)
    {
        
    }

    void InspectorPanel::OnGUI()
    {
        UUID selectionID = SelectionManager::GetSelection();
        if (selectionID != 0)
        {
            DrawComponents(m_Scene->GetEntityWithUUID(selectionID));
        }
    }

    void InspectorPanel::DrawComponents(Entity entity)
    {
        UUID id = entity.GetUUID();
        
        // Name 组件
        if (entity.HasComponent<NameComponent>())
        {
            std::string& name = entity.GetComponent<NameComponent>().Name;   // 物体名

            char buffer[256];                               // 输入框内容 buffer
            memset(buffer, 0, sizeof(buffer));              // 将 buffer 置零
            strcpy_s(buffer, sizeof(buffer), name.c_str()); // buffer = name
            
            UI::ShiftCursor(8.0f, 8.0f);
            if (UI::InputText("##Name", buffer, sizeof(buffer)))
            {
                name = std::string(buffer);
            }
            UI::ShiftCursorY(8.0f);
        }
        
        // Transform 组件
        DrawComponent<TransformComponent>("Transform", entity, [](TransformComponent& transform)
        {
            UI::BeginPropertyGrid();
    
            UI::PropertyFloat3("Position", transform.Translation, 0.01f);
            
            glm::vec3 rotationEuler = glm::degrees(transform.GetRotationEuler());
            if (UI::PropertyFloat3("Rotation", rotationEuler, 1.0f))
            {
                transform.SetRotationEuler(glm::radians(rotationEuler));
            }

            UI::PropertyFloat3("Scale", transform.Scale, 0.01f);
            
            UI::EndPropertyGrid();
        });
        
        // Light 组件
        DrawComponent<LightComponent>("Light", entity, [](LightComponent& light)
        {
            UI::BeginPropertyGrid();

            const char* lightTypes[] = { "Directional", "Point", "Spot" };
            int currentType = static_cast<int>(light.Type);
            if (UI::PropertyCombo("Type", currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
            {
                light.Type = static_cast<LightType>(currentType);
            }

            UI::PropertyColor("Color", light.Color);
            UI::PropertyFloat("Intensity", light.Intensity, 0.01f, 0.0f, 100.0f);
            
            // Point / Spot 属性
            if (light.Type == LightType::Point || light.Type == LightType::Spot)
            {
                UI::PropertyFloat("Range", light.Range, 0.1f, 0.1f, 1000.0f);
            }

            // Spot 属性
            if (light.Type == LightType::Spot)
            {
                UI::PropertyFloat("Inner Cutoff", light.InnerCutoffAngle, 0.5f, 0.0f, light.OuterCutoffAngle);
                UI::PropertyFloat("Outer Cutoff", light.OuterCutoffAngle, 0.5f, light.InnerCutoffAngle, 90.0f);
            }

            // 阴影属性
            const char* shadowTypes[] = { "No Shadows", "Hard Shadows", "Soft Shadows" };
            int currentShadow = static_cast<int>(light.Shadows);
            if (UI::PropertyCombo("Shadow Type", currentShadow, shadowTypes, IM_ARRAYSIZE(shadowTypes)))
            {
                light.Shadows = static_cast<ShadowType>(currentShadow);
            }

            if (light.Shadows != ShadowType::None)
            {
                UI::PropertyFloat("Shadow Bias", light.ShadowBias, 0.0001f, 0.0f, 0.05f);
                UI::PropertyFloat("Shadow Strength", light.ShadowStrength, 0.01f, 0.0f, 1.0f);
            }
            
            UI::EndPropertyGrid();
        });

        // MeshFilter 组件
        static std::string meshName;
        DrawComponent<MeshFilterComponent>(meshName + " (Mesh Filter)", entity, [](MeshFilterComponent& meshFilter)
        {
            meshName = meshFilter.Mesh->GetName();
            
            UI::BeginPropertyGrid();
            
            UI::PropertyObject("Mesh", meshName.c_str());
            
            UI::EndPropertyGrid();
        });

        // MeshRenderer 组件
        DrawComponent<MeshRendererComponent>("Mesh Renderer", entity, [&](MeshRendererComponent& meshRenderer)
        {
            const std::string& strID = std::format("Materials##{0}", static_cast<uint64_t>(id));

            if (UI::BeginCollapsing(strID.c_str()))
            {
                UI::BeginPropertyGrid();
                
                // 材质数量 TODO: 可编辑
                int materialSize = static_cast<int>(meshRenderer.Materials.size());
                UI::PropertyInt("Size", materialSize);
                
                // 材质列表
                for (int i = 0; i < materialSize; i++)
                {
                    const std::string& label = std::format("Element {0}", i);
                    const std::string& materialName = meshRenderer.Materials[i]->GetName();
                
                    UI::PropertyObject(label.c_str(), materialName.c_str());
                }
                
                UI::EndPropertyGrid();
                
                UI::EndCollapsing();
            }
        });
        
        // PostProcessVolume 组件
        DrawComponent<PostProcessVolumeComponent>("Post Process Volume", entity, [&](PostProcessVolumeComponent& volume)
        {
            UI::BeginPropertyGrid();
            
            // Volume 设置
            UI::PropertyCheckbox("Is Global", volume.IsGlobal);
            UI::PropertyFloat("Priority", volume.Priority, 0.1f);
            
            UI::EndPropertyGrid();
            
            // ---- Tonemapping ----
            const std::string& strTonemappingID = std::format("Tonemapping##{0}", static_cast<uint64_t>(id));
            if (UI::BeginCollapsing(strTonemappingID.c_str()))
            {
                UI::BeginPropertyGrid();
                
                const char* tonemapModes[] = { "Reinhard", "ACES Filmic", "Uncharted 2" };
                int tonemapIndex = static_cast<int>(volume.Tonemap);
                if (UI::PropertyCombo("Tonemap Mode", tonemapIndex, tonemapModes, IM_ARRAYSIZE(tonemapModes)))
                {
                    volume.Tonemap = static_cast<TonemapMode>(tonemapIndex);
                }
                UI::PropertyFloat("Exposure", volume.Exposure, 0.01f, 0.0f, 10.0f);
                
                UI::EndPropertyGrid();
                
                UI::EndCollapsing();
            }

            // ---- Bloom ----
            const std::string& strBloomID = std::format("Bloom##{0}", static_cast<uint64_t>(id));
            if (UI::BeginCollapsing(strBloomID.c_str()))
            {
                UI::BeginPropertyGrid();
                
                UI::PropertyCheckbox("Bloom Enabled", volume.BloomEnabled);
                if (volume.BloomEnabled)
                {
                    UI::PropertyFloat("Threshold", volume.BloomThreshold, 0.01f, 0.0f, 10.0f);
                    UI::PropertyFloat("Bloom Intensity", volume.BloomIntensity, 0.01f, 0.0f, 10.0f);
                    UI::PropertyInt("Iterations", volume.BloomIterations, 1, 1, 10);
                }
                
                UI::EndPropertyGrid();
                
                UI::EndCollapsing();
            }

            // ---- FXAA ----
            const std::string& strFXAAID = std::format("FXAA##{0}", static_cast<uint64_t>(id));
            if (UI::BeginCollapsing(strFXAAID.c_str()))
            {
                UI::BeginPropertyGrid();
                
                UI::PropertyCheckbox("FXAA Enabled", volume.FXAAEnabled);
                
                UI::EndPropertyGrid();
                
                UI::EndCollapsing();
            }

            // ---- Vignette ----
            const std::string& strVignetteID = std::format("Vignette##{0}", static_cast<uint64_t>(id));
            if (UI::BeginCollapsing(strVignetteID.c_str()))
            {
                UI::BeginPropertyGrid();
                
                UI::PropertyCheckbox("Vignette Enabled", volume.VignetteEnabled);
                if (volume.VignetteEnabled)
                {
                    UI::PropertyFloat("Vignette Intensity", volume.VignetteIntensity, 0.01f, 0.0f, 1.0f);
                    UI::PropertyFloat("Smoothness", volume.VignetteSmoothness, 0.01f, 0.0f, 10.0f);
                }
                
                UI::EndPropertyGrid();
                
                UI::EndCollapsing();
            }
        });

        if (entity.HasComponent<MeshRendererComponent>())
        {
			// 绘制材质编辑器
            MeshRendererComponent& meshRenderer = entity.GetComponent<MeshRendererComponent>();
            for (Ref<Material>& material : meshRenderer.Materials)
            {
                if (!material)
                {
                    material = Renderer3D::GetInternalErrorMaterial();  // 使用内部错误材质（表示材质丢失）
                }
                
                MaterialEditor::OnGUI(material);
            }
        }
        
        UI::Draw::HorizontalLine();
    }

    void InspectorPanel::OnEvent(Event& event)
    {
        
    }
}
