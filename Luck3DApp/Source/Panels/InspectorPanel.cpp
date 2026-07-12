#include "InspectorPanel.h"

#include "Lucky/Scene/SelectionManager.h"

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
            const std::string& name = entity.GetName();     // 物体名

            char buffer[256];                               // 输入框内容 buffer
            memset(buffer, 0, sizeof(buffer));              // 将 buffer 置零
            strcpy_s(buffer, sizeof(buffer), name.c_str()); // buffer = name
            
            UI::ShiftCursor(8.0f, 8.0f);
            // 使用 EnterReturnsTrue：仅在按下 Enter 或失焦时才提交，避免用户中途清空触发空名警告
            if (UI::InputText("##Name", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                entity.SetName(std::string(buffer));
            }
            UI::ShiftCursorY(8.0f);
        }
        
        // Transform 组件
        DrawComponent<TransformComponent>("Transform", entity, [](TransformComponent& transform)
        {
            UI::PropertyFloat3("Position", transform.Translation, 0.01f);
            
            glm::vec3 rotationEuler = glm::degrees(transform.GetRotationEuler());
            if (UI::PropertyFloat3("Rotation", rotationEuler, 1.0f))
            {
                transform.SetRotationEuler(glm::radians(rotationEuler));
            }

            UI::PropertyFloat3("Scale", transform.Scale, 0.01f);
        });
        
        // Light 组件
        DrawComponent<LightComponent>("Light", entity, [](LightComponent& light)
        {
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

                // ---- CSM 属性（仅方向光 + 阴影开启时显示） ----
                if (light.Type == LightType::Directional)
                {
                    UI::PropertyFloat("Shadow Distance", light.ShadowDistance, 1.0f, 1.0f, 1000.0f);
                    UI::PropertyInt("Cascade Count", light.CascadeCount, 1.0f, 1, 4);

                    // Shadow Map Resolution（下拉框）
                    const char* resolutionOptions[] = { "512", "1024", "2048", "4096" };
                    int resolutionValues[] = { 512, 1024, 2048, 4096 };
                    int currentResIdx = 2;  // 默认 2048
                    for (int i = 0; i < 4; ++i)
                    {
                        if (resolutionValues[i] == light.ShadowMapResolution)
                        {
                            currentResIdx = i;
                            break;
                        }
                    }
                    if (UI::PropertyCombo("Shadow Resolution", currentResIdx, resolutionOptions, 4))
                    {
                        light.ShadowMapResolution = resolutionValues[currentResIdx];
                    }

                    // Cascade Splits（根据 CascadeCount 显示对应数量的滑块）
                    for (int i = 0; i < light.CascadeCount; ++i)
                    {
                        std::string label = "Cascade " + std::to_string(i);
                        float minVal = (i == 0) ? 0.001f : light.CascadeSplits[i - 1];
                        UI::PropertyFloat(label.c_str(), light.CascadeSplits[i], 0.001f, minVal, 1.0f);
                    }

                    // 确保最后一级始终为 1.0
                    light.CascadeSplits[light.CascadeCount - 1] = 1.0f;

                    // 确保分割比例单调递增
                    for (int i = 1; i < light.CascadeCount; ++i)
                    {
                        if (light.CascadeSplits[i] <= light.CascadeSplits[i - 1])
                        {
                            light.CascadeSplits[i] = light.CascadeSplits[i - 1] + 0.001f;
                        }
                    }
                }
            }
        });

        // MeshFilter 组件
        static std::string meshName;
        DrawComponent<MeshFilterComponent>(meshName + " (Mesh Filter)", entity, [](MeshFilterComponent& meshFilter)
        {
            meshName = meshFilter.Mesh ? meshFilter.Mesh->GetName() : "";
            
            UI::PropertyAsset("Mesh", meshFilter.Mesh);
        });

        // MeshRenderer 组件
        DrawComponent<MeshRendererComponent>("Mesh Renderer", entity, [&](MeshRendererComponent& meshRenderer)
        {
            const std::string& strID = std::format("Materials##{0}", static_cast<uint64_t>(id));

            if (UI::BeginCollapsing(strID.c_str()))
            {
                // 材质数量 TODO: 可编辑
                int materialSize = static_cast<int>(meshRenderer.Materials.size());
                UI::PropertyInt("Size", materialSize);
                
                // 材质列表
                for (int i = 0; i < materialSize; i++)
                {
                    const std::string& label = std::format("Element {0}", i);
                
                    UI::PropertyAsset(label.c_str(), meshRenderer.Materials[i]);
                }
                
                UI::EndCollapsing();
            }
        });
        
        // PostProcessVolume 组件
        DrawComponent<PostProcessVolumeComponent>("Post Process Volume", entity, [&](PostProcessVolumeComponent& volume)
        {
            // Volume 设置
            UI::PropertyCheckbox("Is Global", volume.IsGlobal);
            UI::PropertyFloat("Priority", volume.Priority, 0.1f);
            
            // ---- Tonemapping ----
            const std::string& strTonemappingID = std::format("Tonemapping##{0}", static_cast<uint64_t>(id));
            if (UI::BeginCollapsing(strTonemappingID.c_str()))
            {
                const char* tonemapModes[] = { "Reinhard", "ACES Filmic", "Uncharted 2" };
                int tonemapIndex = static_cast<int>(volume.Tonemap);
                if (UI::PropertyCombo("Tonemap Mode", tonemapIndex, tonemapModes, IM_ARRAYSIZE(tonemapModes)))
                {
                    volume.Tonemap = static_cast<TonemapMode>(tonemapIndex);
                }
                UI::PropertyFloat("Exposure", volume.Exposure, 0.01f, 0.0f, 10.0f);
                
                UI::EndCollapsing();
            }

            // ---- Bloom ----
            const std::string& strBloomID = std::format("Bloom##{0}", static_cast<uint64_t>(id));
            if (UI::BeginCollapsing(strBloomID.c_str()))
            {
                UI::PropertyCheckbox("Bloom Enabled", volume.BloomEnabled);
                if (volume.BloomEnabled)
                {
                    UI::PropertyFloat("Threshold", volume.BloomThreshold, 0.01f, 0.0f, 10.0f);
                    UI::PropertyFloat("Bloom Intensity", volume.BloomIntensity, 0.01f, 0.0f, 10.0f);
                    UI::PropertyInt("Iterations", volume.BloomIterations, 1, 1, 10);
                }

                UI::EndCollapsing();
            }

            // ---- FXAA ----
            const std::string& strFXAAID = std::format("FXAA##{0}", static_cast<uint64_t>(id));
            if (UI::BeginCollapsing(strFXAAID.c_str()))
            {
                UI::PropertyCheckbox("FXAA Enabled", volume.FXAAEnabled);
                
                UI::EndCollapsing();
            }

            // ---- Vignette ----
            const std::string& strVignetteID = std::format("Vignette##{0}", static_cast<uint64_t>(id));
            if (UI::BeginCollapsing(strVignetteID.c_str()))
            {
                UI::PropertyCheckbox("Vignette Enabled", volume.VignetteEnabled);
                if (volume.VignetteEnabled)
                {
                    UI::PropertyFloat("Vignette Intensity", volume.VignetteIntensity, 0.01f, 0.0f, 1.0f);
                    UI::PropertyFloat("Smoothness", volume.VignetteSmoothness, 0.01f, 0.0f, 10.0f);
                }
                
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
