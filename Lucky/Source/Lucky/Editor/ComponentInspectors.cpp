#include "lcpch.h"

#include "Lucky/Scene/ComponentRegistry.h"
#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/Components.h"

#include "Lucky/Editor/EditorIconManager.h"

#include "Lucky/UI/PropertyGrid.h"
#include "Lucky/UI/Widgets.h"

#include <imgui/imgui.h>

#include <format>

namespace Lucky
{
    namespace
    {
        /// <summary>
        /// 通用图标解析：按 ComponentTrait<T>::Type 从 EditorIconManager 取默认图标
        /// </summary>
        template<typename TComponent>
        const Ref<Texture2D>& DefaultIcon(Entity /*entity*/)
        {
            return EditorIconManager::GetComponentIcon(ComponentTrait<TComponent>::Type);
        }

        // ======== TransformComponent ========

        void Draw_Transform(Entity entity)
        {
            TransformComponent& t = entity.GetComponent<TransformComponent>();

            UI::PropertyFloat3("Position", t.Translation, 0.01f);

            glm::vec3 rotationEuler = glm::degrees(t.GetRotationEuler());
            if (UI::PropertyFloat3("Rotation", rotationEuler, 1.0f))
            {
                t.SetRotationEuler(glm::radians(rotationEuler));
            }

            UI::PropertyFloat3("Scale", t.Scale, 0.01f);
        }

        // ======== MeshFilterComponent ========

        void Draw_MeshFilter(Entity entity)
        {
            MeshFilterComponent& mf = entity.GetComponent<MeshFilterComponent>();
            UI::PropertyAsset("Mesh", mf.Mesh);
        }

        // ======== MeshRendererComponent ========

        void Draw_MeshRenderer(Entity entity)
        {
            MeshRendererComponent& mr = entity.GetComponent<MeshRendererComponent>();
            UUID id = entity.GetUUID();

            const std::string& strID = std::format("Materials##{0}", static_cast<uint64_t>(id));
            if (UI::BeginCollapsing(strID.c_str()))
            {
                int materialSize = static_cast<int>(mr.Materials.size());
                UI::PropertyInt("Size", materialSize);

                for (int i = 0; i < materialSize; i++)
                {
                    const std::string& label = std::format("Element {0}", i);
                    UI::PropertyAsset(label.c_str(), mr.Materials[i]);
                }

                UI::EndCollapsing();
            }
        }

        // ======== SpriteRendererComponent ========

        void Draw_SpriteRenderer(Entity entity)
        {
            SpriteRendererComponent& sprite = entity.GetComponent<SpriteRendererComponent>();
            UI::PropertyAsset("Sprite", sprite.Texture);
            UI::PropertyColor("Color", sprite.Color);
            UI::PropertyCheckbox("Flip X", sprite.FlipX);
            UI::PropertyCheckbox("Flip Y", sprite.FlipY);
            UI::PropertyFloat4("UV Rect", sprite.UVRect, 0.01f);
            UI::PropertyFloat("Tiling", sprite.TilingFactor, 0.1f, 0.0f, 100.0f);
            UI::PropertyAsset("Material", sprite.Material);
            UI::PropertyInt("Sorting Order", sprite.SortingOrder);
        }

        // ======== LightComponent ========

        void Draw_Light(Entity entity)
        {
            LightComponent& light = entity.GetComponent<LightComponent>();

            const char* lightTypes[] = { "Directional", "Point", "Spot" };
            int currentType = static_cast<int>(light.Type);
            if (UI::PropertyCombo("Type", currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
            {
                light.Type = static_cast<LightType>(currentType);
            }

            UI::PropertyColor("Color", light.Color);
            UI::PropertyFloat("Intensity", light.Intensity, 0.01f, 0.0f, 100.0f);

            if (light.Type == LightType::Point || light.Type == LightType::Spot)
            {
                UI::PropertyFloat("Range", light.Range, 0.1f, 0.1f, 1000.0f);
            }

            if (light.Type == LightType::Spot)
            {
                UI::PropertyFloat("Inner Cutoff", light.InnerCutoffAngle, 0.5f, 0.0f, light.OuterCutoffAngle);
                UI::PropertyFloat("Outer Cutoff", light.OuterCutoffAngle, 0.5f, light.InnerCutoffAngle, 90.0f);
            }

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

                if (light.Type == LightType::Directional)
                {
                    UI::PropertyFloat("Shadow Distance", light.ShadowDistance, 1.0f, 1.0f, 1000.0f);
                    UI::PropertyInt("Cascade Count", light.CascadeCount, 1.0f, 1, 4);

                    const char* resolutionOptions[] = { "512", "1024", "2048", "4096" };
                    int resolutionValues[] = { 512, 1024, 2048, 4096 };
                    int currentResIdx = 2;
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

                    for (int i = 0; i < light.CascadeCount; ++i)
                    {
                        std::string label = "Cascade " + std::to_string(i);
                        float minVal = (i == 0) ? 0.001f : light.CascadeSplits[i - 1];
                        UI::PropertyFloat(label.c_str(), light.CascadeSplits[i], 0.001f, minVal, 1.0f);
                    }

                    light.CascadeSplits[light.CascadeCount - 1] = 1.0f;

                    for (int i = 1; i < light.CascadeCount; ++i)
                    {
                        if (light.CascadeSplits[i] <= light.CascadeSplits[i - 1])
                        {
                            light.CascadeSplits[i] = light.CascadeSplits[i - 1] + 0.001f;
                        }
                    }
                }
            }
        }

        // ======== PostProcessVolumeComponent ========

        void Draw_PostProcessVolume(Entity entity)
        {
            PostProcessVolumeComponent& volume = entity.GetComponent<PostProcessVolumeComponent>();
            UUID id = entity.GetUUID();

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
        }

        // ======== CameraComponent ========

        void Draw_Camera(Entity entity)
        {
            CameraComponent& cc = entity.GetComponent<CameraComponent>();
            SceneCamera& sc = cc.Camera;

            const char* projectionTypes[] = { "Perspective", "Orthographic" };
            int currentProj = static_cast<int>(sc.GetProjectionType());
            if (UI::PropertyCombo("Projection", currentProj, projectionTypes, IM_ARRAYSIZE(projectionTypes)))
            {
                sc.SetProjectionType(static_cast<ProjectionType>(currentProj));
            }

            if (sc.GetProjectionType() == ProjectionType::Perspective)
            {
                float fov = sc.GetPerspectiveVerticalFOV();
                if (UI::PropertyFloat("Field of View", fov, 0.1f, 1.0f, 179.0f))
                {
                    sc.SetPerspectiveVerticalFOV(fov);
                }

                float nearClip = sc.GetPerspectiveNearClip();
                if (UI::PropertyFloat("Near Clip", nearClip, 0.001f, 0.001f, 1000.0f))
                {
                    sc.SetPerspectiveNearClip(nearClip);
                }

                float farClip = sc.GetPerspectiveFarClip();
                if (UI::PropertyFloat("Far Clip", farClip, 1.0f, 0.1f, 100000.0f))
                {
                    sc.SetPerspectiveFarClip(farClip);
                }
            }
            else
            {
                float size = sc.GetOrthographicSize();
                if (UI::PropertyFloat("Size", size, 0.1f, 0.1f, 1000.0f))
                {
                    sc.SetOrthographicSize(size);
                }

                float nearClip = sc.GetOrthographicNearClip();
                if (UI::PropertyFloat("Near Clip", nearClip, 0.1f, -1000.0f, 1000.0f))
                {
                    sc.SetOrthographicNearClip(nearClip);
                }

                float farClip = sc.GetOrthographicFarClip();
                if (UI::PropertyFloat("Far Clip", farClip, 1.0f, 0.1f, 100000.0f))
                {
                    sc.SetOrthographicFarClip(farClip);
                }
            }

            UI::PropertyCheckbox("Primary", cc.Primary);
            UI::PropertyCheckbox("Fixed Aspect Ratio", cc.FixedAspectRatio);
        }
    }

    // ======== RegisterAllInspectors ========

    void ComponentRegistry::RegisterAllInspectors()
    {
        // ---- NameComponent ----
        // 无 Draw；Name 在 Inspector 顶部有独立的裸 InputText，不套 TreeNode 外壳
        RegisterInspector(ComponentType::Name,
            nullptr,
            &DefaultIcon<NameComponent>,
            {},
            nullptr,
            /* showInHierarchyIcons = */ false,
            /* canRemove = */ false);

        // ---- TransformComponent ----
        RegisterInspector(ComponentType::Transform,
            &Draw_Transform,
            &DefaultIcon<TransformComponent>,
            {},
            nullptr,
            /* showInHierarchyIcons = */ false,
            /* canRemove = */ false);

        // ---- RelationshipComponent ----
        // 系统组件：无 Draw，不出现在 Inspector 组件列表 / Hierarchy 图标条 / AddMenu
        RegisterInspector(ComponentType::Relationship,
            nullptr,
            &DefaultIcon<RelationshipComponent>,
            {},
            nullptr,
            /* showInHierarchyIcons = */ false,
            /* canRemove = */ false);

        // ---- LightComponent ----
        // Light 三子类型对应 AddMenu 中三条项，Hierarchy 图标 / Inspector 图标按 LightType 动态取值
        RegisterInspector(ComponentType::Light,
            &Draw_Light,
            [](Entity e) -> const Ref<Texture2D>&
            {
                return EditorIconManager::GetLightIcon(e.GetComponent<LightComponent>().Type);
            },
            {
                { "Directional Light",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetLightIcon(LightType::Directional); },
                  [](Entity e) { e.AddComponent<LightComponent>(LightType::Directional); } },
                { "Point Light",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetLightIcon(LightType::Point); },
                  [](Entity e) { e.AddComponent<LightComponent>(LightType::Point); } },
                { "Spot Light",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetLightIcon(LightType::Spot); },
                  [](Entity e) { e.AddComponent<LightComponent>(LightType::Spot); } },
            },
            [](Entity e) { e.RemoveComponent<LightComponent>(); });

        // ---- MeshFilterComponent ----
        RegisterInspector(ComponentType::MeshFilter,
            &Draw_MeshFilter,
            &DefaultIcon<MeshFilterComponent>,
            {
                { "Mesh Filter",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetComponentIcon(ComponentType::MeshFilter); },
                  [](Entity e) { e.AddComponent<MeshFilterComponent>(); } },
            },
            [](Entity e) { e.RemoveComponent<MeshFilterComponent>(); });

        // ---- MeshRendererComponent ----
        RegisterInspector(ComponentType::MeshRenderer,
            &Draw_MeshRenderer,
            &DefaultIcon<MeshRendererComponent>,
            {
                { "Mesh Renderer",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetComponentIcon(ComponentType::MeshRenderer); },
                  [](Entity e) { e.AddComponent<MeshRendererComponent>(); } },
            },
            [](Entity e) { e.RemoveComponent<MeshRendererComponent>(); });

        // ---- SpriteRendererComponent ----
        RegisterInspector(ComponentType::SpriteRenderer,
            &Draw_SpriteRenderer,
            &DefaultIcon<SpriteRendererComponent>,
            {
                { "Sprite Renderer",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetComponentIcon(ComponentType::SpriteRenderer); },
                  [](Entity e) { e.AddComponent<SpriteRendererComponent>(); } },
            },
            [](Entity e) { e.RemoveComponent<SpriteRendererComponent>(); });

        // ---- PostProcessVolumeComponent ----
        RegisterInspector(ComponentType::PostProcessVolume,
            &Draw_PostProcessVolume,
            &DefaultIcon<PostProcessVolumeComponent>,
            {
                { "Post Process Volume",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetComponentIcon(ComponentType::PostProcessVolume); },
                  [](Entity e) { e.AddComponent<PostProcessVolumeComponent>(); } },
            },
            [](Entity e) { e.RemoveComponent<PostProcessVolumeComponent>(); });

        // ---- CameraComponent ----
        RegisterInspector(ComponentType::Camera,
            &Draw_Camera,
            &DefaultIcon<CameraComponent>,
            {
                { "Camera",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetComponentIcon(ComponentType::Camera); },
                  [](Entity e) { e.AddComponent<CameraComponent>(); } },
            },
            [](Entity e) { e.RemoveComponent<CameraComponent>(); });
    }
}
