#include "lcpch.h"

#include "ComponentRegistry.h"
#include "Components.h"

#include "Lucky/Scene/Entity.h"

#include "Lucky/Serialization/YamlHelpers.h"
#include "Lucky/Serialization/MaterialSerializer.h"

#include "Lucky/Renderer/Renderer3D.h"

#include "Lucky/Asset/AssetManager.h"

#include "Lucky/Editor/EditorIconManager.h"

#include "Lucky/UI/PropertyGrid.h"
#include "Lucky/UI/Widgets.h"

#include <yaml-cpp/yaml.h>
#include <imgui/imgui.h>

#include <format>

namespace Lucky
{
    namespace
    {
        // ======== 通用 helper ========

        /// <summary>
        /// 通用组件深拷贝：从 srcEntity 拷贝 T 类型组件到 dstEntity（若存在）
        /// 直接走 entt::registry::emplace_or_replace，不触发 OnComponentAdded
        /// </summary>
        template<typename TComponent>
        void CopyComponentValue(entt::registry& dst, entt::entity dstEnt,
                                entt::registry& src, entt::entity srcEnt)
        {
            if (src.has<TComponent>(srcEnt))
            {
                const TComponent& srcComp = src.get<TComponent>(srcEnt);
                dst.emplace_or_replace<TComponent>(dstEnt, srcComp);
            }
        }

        // ======== NameComponent ========

        static void Serialize_Name(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<NameComponent>())
            {
                return;
            }
            const NameComponent& n = entity.GetComponent<NameComponent>();

            out << YAML::Key << "NameComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << n.Name;
            out << YAML::EndMap;
        }

        static void Deserialize_Name(Entity /*entity*/, const YAML::Node& /*entityNode*/)
        {
            // NameComponent 由 Scene::CreateEntity(uuid, name) 在实体建立时直接注入
            // 反序列化路径已在 SceneSerializer::Deserialize 中通过 entity["NameComponent"]["Name"] 读取并传给 CreateEntity
        }

        // ======== TransformComponent ========

        static void Serialize_Transform(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<TransformComponent>())
            {
                return;
            }
            const TransformComponent& t = entity.GetComponent<TransformComponent>();

            out << YAML::Key << "TransformComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "Position" << YAML::Value << t.Translation;
            out << YAML::Key << "Rotation" << YAML::Value << t.GetRotation();
            out << YAML::Key << "Scale" << YAML::Value << t.Scale;
            out << YAML::EndMap;
        }

        static void Deserialize_Transform(Entity entity, const YAML::Node& entityNode)
        {
            YAML::Node node = entityNode["TransformComponent"];
            if (!node)
            {
                return;
            }
            TransformComponent& t = entity.GetComponent<TransformComponent>();
            t.Translation = node["Position"].as<glm::vec3>();
            t.SetRotation(node["Rotation"].as<glm::quat>());
            t.Scale = node["Scale"].as<glm::vec3>();
        }

        static void Draw_Transform(Entity entity)
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

        // ======== RelationshipComponent ========

        static void Serialize_Relationship(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<RelationshipComponent>())
            {
                return;
            }
            const RelationshipComponent& r = entity.GetComponent<RelationshipComponent>();

            out << YAML::Key << "RelationshipComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "Parent" << YAML::Value << r.Parent;

            out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
            for (const UUID& child : r.Children)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "Child" << YAML::Value << child;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;

            out << YAML::EndMap;
        }

        static void Deserialize_Relationship(Entity entity, const YAML::Node& entityNode)
        {
            YAML::Node node = entityNode["RelationshipComponent"];
            if (!node)
            {
                return;
            }
            RelationshipComponent& r = entity.GetComponent<RelationshipComponent>();
            r.Parent = node["Parent"].as<UUID>();

            r.Children.clear();
            YAML::Node childrenNode = node["Children"];
            if (childrenNode)
            {
                for (auto childEntry : childrenNode)
                {
                    uint64_t child = childEntry["Child"].as<uint64_t>();
                    r.Children.push_back(child);
                }
            }
        }

        // ======== MeshFilterComponent ========

        static void Serialize_MeshFilter(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<MeshFilterComponent>())
            {
                return;
            }
            const MeshFilterComponent& mf = entity.GetComponent<MeshFilterComponent>();

            out << YAML::Key << "MeshFilterComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "PrimitiveType" << YAML::Value << static_cast<int>(mf.Primitive);

            if (mf.Mesh && mf.Primitive == PrimitiveType::None)
            {
                out << YAML::Key << "MeshAsset" << YAML::Value << static_cast<uint64_t>(mf.Mesh->GetHandle());
            }
            else
            {
                out << YAML::Key << "MeshAsset" << YAML::Value << static_cast<uint64_t>(0);
            }
            out << YAML::EndMap;
        }

        static void Deserialize_MeshFilter(Entity entity, const YAML::Node& entityNode)
        {
            YAML::Node node = entityNode["MeshFilterComponent"];
            if (!node)
            {
                return;
            }
            PrimitiveType primitiveType = static_cast<PrimitiveType>(node["PrimitiveType"].as<int>());

            if (primitiveType != PrimitiveType::None)
            {
                entity.AddComponent<MeshFilterComponent>(primitiveType);
            }
            else if (node["MeshAsset"])
            {
                uint64_t meshHandleValue = node["MeshAsset"].as<uint64_t>();
                AssetHandle meshHandle(meshHandleValue);

                MeshFilterComponent& mf = entity.AddComponent<MeshFilterComponent>();

                if (meshHandle.IsValid())
                {
                    Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(meshHandle);
                    if (mesh)
                    {
                        mf.Mesh = mesh;
                    }
                    else
                    {
                        LF_CORE_ERROR("SceneSerializer: Failed to load mesh asset [{0}]", meshHandleValue);
                    }
                }
            }
            else
            {
                entity.AddComponent<MeshFilterComponent>();
            }
        }

        static void Draw_MeshFilter(Entity entity)
        {
            MeshFilterComponent& mf = entity.GetComponent<MeshFilterComponent>();
            UI::PropertyAsset("Mesh", mf.Mesh);
        }

        // ======== MeshRendererComponent ========

        static void Serialize_MeshRenderer(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<MeshRendererComponent>())
            {
                return;
            }
            const MeshRendererComponent& mr = entity.GetComponent<MeshRendererComponent>();

            out << YAML::Key << "MeshRendererComponent";
            out << YAML::BeginMap;

            out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
            for (const Ref<Material>& material : mr.Materials)
            {
                out << YAML::BeginMap;
                if (material)
                {
                    out << YAML::Key << "AssetHandle" << YAML::Value << material->GetHandle();
                }
                else
                {
                    out << YAML::Key << "AssetHandle" << YAML::Value << static_cast<uint64_t>(0);
                }
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;

            out << YAML::EndMap;
        }

        static void Deserialize_MeshRenderer(Entity entity, const YAML::Node& entityNode)
        {
            YAML::Node node = entityNode["MeshRendererComponent"];
            if (!node)
            {
                return;
            }
            MeshRendererComponent& mr = entity.AddComponent<MeshRendererComponent>();

            YAML::Node materialsNode = node["Materials"];
            if (materialsNode && materialsNode.IsSequence())
            {
                mr.Materials.clear();
                mr.Materials.reserve(materialsNode.size());

                for (auto materialNode : materialsNode)
                {
                    if (materialNode["AssetHandle"])
                    {
                        // 新格式：通过 AssetHandle 从 AssetManager 获取材质
                        uint64_t handleValue = materialNode["AssetHandle"].as<uint64_t>();
                        AssetHandle handle(handleValue);

                        Ref<Material> material = nullptr;
                        if (handle.IsValid())
                        {
                            material = AssetManager::GetAsset<Material>(handle);
                        }

                        if (!material)
                        {
                            LF_CORE_ERROR("SceneSerializer: Failed to load material asset [{0}]", handleValue);
                            material = Renderer3D::GetInternalErrorMaterial();
                        }

                        mr.Materials.push_back(material);
                    }
                    else
                    {
                        // 兼容旧格式：内嵌材质数据
                        Ref<Material> material = MaterialSerializer::Deserialize(materialNode);
                        if (!material)
                        {
                            material = Renderer3D::GetInternalErrorMaterial();
                        }
                        mr.Materials.push_back(material);
                    }
                }
            }
        }

        static void Draw_MeshRenderer(Entity entity)
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

        static void Serialize_SpriteRenderer(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<SpriteRendererComponent>())
            {
                return;
            }
            const SpriteRendererComponent& sprite = entity.GetComponent<SpriteRendererComponent>();

            out << YAML::Key << "SpriteRendererComponent";
            out << YAML::BeginMap;

            if (sprite.Texture)
            {
                out << YAML::Key << "Texture" << YAML::Value << static_cast<uint64_t>(sprite.Texture->GetHandle());
            }
            else
            {
                out << YAML::Key << "Texture" << YAML::Value << static_cast<uint64_t>(0);
            }

            out << YAML::Key << "Color" << YAML::Value << sprite.Color;
            out << YAML::Key << "FlipX" << YAML::Value << sprite.FlipX;
            out << YAML::Key << "FlipY" << YAML::Value << sprite.FlipY;
            out << YAML::Key << "UVRect" << YAML::Value << sprite.UVRect;
            out << YAML::Key << "TilingFactor" << YAML::Value << sprite.TilingFactor;

            if (sprite.Material)
            {
                out << YAML::Key << "Material" << YAML::Value << static_cast<uint64_t>(sprite.Material->GetHandle());
            }
            else
            {
                out << YAML::Key << "Material" << YAML::Value << static_cast<uint64_t>(0);
            }

            out << YAML::Key << "SortingOrder" << YAML::Value << sprite.SortingOrder;

            out << YAML::EndMap;
        }

        static void Deserialize_SpriteRenderer(Entity entity, const YAML::Node& entityNode)
        {
            YAML::Node node = entityNode["SpriteRendererComponent"];
            if (!node)
            {
                return;
            }
            SpriteRendererComponent& sprite = entity.AddComponent<SpriteRendererComponent>();

            if (node["Texture"])
            {
                uint64_t handleValue = node["Texture"].as<uint64_t>();
                AssetHandle handle(handleValue);
                if (handle.IsValid())
                {
                    Ref<Texture2D> tex = AssetManager::GetAsset<Texture2D>(handle);
                    if (!tex)
                    {
                        LF_CORE_WARN("SceneSerializer: Failed to load sprite texture asset [{0}]", handleValue);
                    }
                    sprite.Texture = tex;
                }
            }

            if (node["Color"])
            {
                sprite.Color = node["Color"].as<glm::vec4>();
            }
            if (node["FlipX"])
            {
                sprite.FlipX = node["FlipX"].as<bool>();
            }
            if (node["FlipY"])
            {
                sprite.FlipY = node["FlipY"].as<bool>();
            }
            if (node["UVRect"])
            {
                sprite.UVRect = node["UVRect"].as<glm::vec4>();
            }
            if (node["TilingFactor"])
            {
                sprite.TilingFactor = node["TilingFactor"].as<float>();
            }

            if (node["Material"])
            {
                uint64_t handleValue = node["Material"].as<uint64_t>();
                AssetHandle handle(handleValue);
                if (handle.IsValid())
                {
                    Ref<Material> mat = AssetManager::GetAsset<Material>(handle);
                    if (!mat)
                    {
                        LF_CORE_WARN("SceneSerializer: Failed to load sprite material asset [{0}]", handleValue);
                    }
                    sprite.Material = mat;
                }
            }

            if (node["SortingOrder"])
            {
                sprite.SortingOrder = node["SortingOrder"].as<int>();
            }
        }

        static void Draw_SpriteRenderer(Entity entity)
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

        static void Serialize_Light(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<LightComponent>())
            {
                return;
            }
            const LightComponent& light = entity.GetComponent<LightComponent>();

            out << YAML::Key << "LightComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "Type" << YAML::Value << static_cast<int>(light.Type);
            out << YAML::Key << "Color" << YAML::Value << light.Color;
            out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;

            if (light.Type == LightType::Point || light.Type == LightType::Spot)
            {
                out << YAML::Key << "Range" << YAML::Value << light.Range;
            }

            if (light.Type == LightType::Spot)
            {
                out << YAML::Key << "InnerCutoffAngle" << YAML::Value << light.InnerCutoffAngle;
                out << YAML::Key << "OuterCutoffAngle" << YAML::Value << light.OuterCutoffAngle;
            }

            out << YAML::Key << "Shadows" << YAML::Value << static_cast<int>(light.Shadows);
            out << YAML::Key << "ShadowBias" << YAML::Value << light.ShadowBias;
            out << YAML::Key << "ShadowStrength" << YAML::Value << light.ShadowStrength;

            if (light.Type == LightType::Directional)
            {
                out << YAML::Key << "CascadeCount" << YAML::Value << light.CascadeCount;
                out << YAML::Key << "ShadowDistance" << YAML::Value << light.ShadowDistance;
                out << YAML::Key << "ShadowMapResolution" << YAML::Value << light.ShadowMapResolution;
                out << YAML::Key << "CascadeSplits" << YAML::Value << YAML::Flow << YAML::BeginSeq;
                for (int i = 0; i < 4; ++i)
                {
                    out << light.CascadeSplits[i];
                }
                out << YAML::EndSeq;
            }

            out << YAML::EndMap;
        }

        static void Deserialize_Light(Entity entity, const YAML::Node& entityNode)
        {
            YAML::Node node = entityNode["LightComponent"];
            if (!node)
            {
                return;
            }
            LightType type = static_cast<LightType>(node["Type"].as<int>());
            LightComponent& light = entity.AddComponent<LightComponent>(type);

            light.Color = node["Color"].as<glm::vec3>();
            light.Intensity = node["Intensity"].as<float>();

            if (node["Range"])
            {
                light.Range = node["Range"].as<float>();
            }
            if (node["InnerCutoffAngle"])
            {
                light.InnerCutoffAngle = node["InnerCutoffAngle"].as<float>();
            }
            if (node["OuterCutoffAngle"])
            {
                light.OuterCutoffAngle = node["OuterCutoffAngle"].as<float>();
            }

            light.Shadows = static_cast<ShadowType>(node["Shadows"].as<int>());
            light.ShadowBias = node["ShadowBias"].as<float>();
            light.ShadowStrength = node["ShadowStrength"].as<float>();

            if (node["CascadeCount"])
            {
                light.CascadeCount = node["CascadeCount"].as<int>();
            }
            if (node["ShadowDistance"])
            {
                light.ShadowDistance = node["ShadowDistance"].as<float>();
            }
            if (node["ShadowMapResolution"])
            {
                light.ShadowMapResolution = node["ShadowMapResolution"].as<int>();
            }
            if (node["CascadeSplits"])
            {
                auto splitSeq = node["CascadeSplits"].as<std::vector<float>>();
                for (int i = 0; i < 4 && i < static_cast<int>(splitSeq.size()); ++i)
                {
                    light.CascadeSplits[i] = splitSeq[i];
                }
            }
        }

        static void Draw_Light(Entity entity)
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

        static void Serialize_PostProcessVolume(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<PostProcessVolumeComponent>())
            {
                return;
            }
            const PostProcessVolumeComponent& volume = entity.GetComponent<PostProcessVolumeComponent>();

            out << YAML::Key << "PostProcessVolumeComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "IsGlobal" << YAML::Value << volume.IsGlobal;
            out << YAML::Key << "Priority" << YAML::Value << volume.Priority;

            out << YAML::Key << "TonemapMode" << YAML::Value << static_cast<int>(volume.Tonemap);
            out << YAML::Key << "Exposure" << YAML::Value << volume.Exposure;

            out << YAML::Key << "BloomEnabled" << YAML::Value << volume.BloomEnabled;
            out << YAML::Key << "BloomThreshold" << YAML::Value << volume.BloomThreshold;
            out << YAML::Key << "BloomIntensity" << YAML::Value << volume.BloomIntensity;
            out << YAML::Key << "BloomIterations" << YAML::Value << volume.BloomIterations;

            out << YAML::Key << "FXAAEnabled" << YAML::Value << volume.FXAAEnabled;

            out << YAML::Key << "VignetteEnabled" << YAML::Value << volume.VignetteEnabled;
            out << YAML::Key << "VignetteIntensity" << YAML::Value << volume.VignetteIntensity;
            out << YAML::Key << "VignetteSmoothness" << YAML::Value << volume.VignetteSmoothness;

            out << YAML::EndMap;
        }

        static void Deserialize_PostProcessVolume(Entity entity, const YAML::Node& entityNode)
        {
            YAML::Node node = entityNode["PostProcessVolumeComponent"];
            if (!node)
            {
                return;
            }
            PostProcessVolumeComponent& volume = entity.AddComponent<PostProcessVolumeComponent>();

            volume.IsGlobal = node["IsGlobal"].as<bool>();
            volume.Priority = node["Priority"].as<float>();

            volume.Tonemap = static_cast<TonemapMode>(node["TonemapMode"].as<int>());
            volume.Exposure = node["Exposure"].as<float>();

            volume.BloomEnabled = node["BloomEnabled"].as<bool>();
            volume.BloomThreshold = node["BloomThreshold"].as<float>();
            volume.BloomIntensity = node["BloomIntensity"].as<float>();
            volume.BloomIterations = node["BloomIterations"].as<int>();

            volume.FXAAEnabled = node["FXAAEnabled"].as<bool>();

            volume.VignetteEnabled = node["VignetteEnabled"].as<bool>();
            volume.VignetteIntensity = node["VignetteIntensity"].as<float>();
            volume.VignetteSmoothness = node["VignetteSmoothness"].as<float>();
        }

        static void Draw_PostProcessVolume(Entity entity)
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

        static void Serialize_Camera(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<CameraComponent>())
            {
                return;
            }
            const CameraComponent& cc = entity.GetComponent<CameraComponent>();
            const SceneCamera& sc = cc.Camera;

            out << YAML::Key << "CameraComponent";
            out << YAML::BeginMap;

            out << YAML::Key << "Projection" << YAML::Value << static_cast<int>(sc.GetProjectionType());
            out << YAML::Key << "PerspectiveFOV" << YAML::Value << sc.GetPerspectiveVerticalFOV();
            out << YAML::Key << "PerspectiveNear" << YAML::Value << sc.GetPerspectiveNearClip();
            out << YAML::Key << "PerspectiveFar" << YAML::Value << sc.GetPerspectiveFarClip();
            out << YAML::Key << "OrthographicSize" << YAML::Value << sc.GetOrthographicSize();
            out << YAML::Key << "OrthographicNear" << YAML::Value << sc.GetOrthographicNearClip();
            out << YAML::Key << "OrthographicFar" << YAML::Value << sc.GetOrthographicFarClip();
            out << YAML::Key << "Primary" << YAML::Value << cc.Primary;
            out << YAML::Key << "FixedAspectRatio" << YAML::Value << cc.FixedAspectRatio;

            out << YAML::EndMap;
        }

        static void Deserialize_Camera(Entity entity, const YAML::Node& entityNode)
        {
            YAML::Node node = entityNode["CameraComponent"];
            if (!node)
            {
                return;
            }
            CameraComponent& cc = entity.AddComponent<CameraComponent>();
            SceneCamera& sc = cc.Camera;

            sc.SetProjectionType(static_cast<ProjectionType>(node["Projection"].as<int>()));
            sc.SetPerspectiveVerticalFOV(node["PerspectiveFOV"].as<float>());
            sc.SetPerspectiveNearClip(node["PerspectiveNear"].as<float>());
            sc.SetPerspectiveFarClip(node["PerspectiveFar"].as<float>());
            sc.SetOrthographicSize(node["OrthographicSize"].as<float>());
            sc.SetOrthographicNearClip(node["OrthographicNear"].as<float>());
            sc.SetOrthographicFarClip(node["OrthographicFar"].as<float>());

            cc.Primary = node["Primary"].as<bool>();
            cc.FixedAspectRatio = node["FixedAspectRatio"].as<bool>();
        }

        static void Draw_Camera(Entity entity)
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

        // ======== 通用图标解析器 ========

        template<typename TComponent>
        static const Ref<Texture2D>& DefaultIcon(Entity /*entity*/)
        {
            return EditorIconManager::GetComponentIcon(ComponentTrait<TComponent>::Type);
        }
    }

    // ======== RegisterAll ========

    void ComponentRegistry::RegisterAll()
    {
        // 注册顺序即 Inspector / Serializer 输出顺序
        // 保持与改造前 SceneSerializer / InspectorPanel 中的枚举顺序完全一致，避免 YAML 键序变化

        // ---- NameComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::Name;
            desc.Name = "Name";
            desc.SerializedKey = "NameComponent";
            desc.Copy = &CopyComponentValue<NameComponent>;
            desc.Serialize = &Serialize_Name;
            desc.Deserialize = &Deserialize_Name;
            desc.Has = [](Entity e) { return e.HasComponent<NameComponent>(); };
            desc.GetIcon = &DefaultIcon<NameComponent>;
            desc.ShowInHierarchyIcons = false;
            desc.CanRemove = false;
            Register(std::move(desc));
        }

        // ---- TransformComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::Transform;
            desc.Name = "Transform";
            desc.SerializedKey = "TransformComponent";
            desc.Copy = &CopyComponentValue<TransformComponent>;
            desc.Serialize = &Serialize_Transform;
            desc.Deserialize = &Deserialize_Transform;
            desc.Draw = &Draw_Transform;
            desc.Has = [](Entity e) { return e.HasComponent<TransformComponent>(); };
            desc.GetIcon = &DefaultIcon<TransformComponent>;
            desc.ShowInHierarchyIcons = false;
            desc.CanRemove = false;
            Register(std::move(desc));
        }

        // ---- RelationshipComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::Relationship;
            desc.Name = "Relationship";
            desc.SerializedKey = "RelationshipComponent";
            desc.Copy = &CopyComponentValue<RelationshipComponent>;
            desc.Serialize = &Serialize_Relationship;
            desc.Deserialize = &Deserialize_Relationship;
            desc.Has = [](Entity e) { return e.HasComponent<RelationshipComponent>(); };
            desc.GetIcon = &DefaultIcon<RelationshipComponent>;
            desc.ShowInHierarchyIcons = false;
            desc.CanRemove = false;
            Register(std::move(desc));
        }

        // ---- LightComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::Light;
            desc.Name = "Light";
            desc.SerializedKey = "LightComponent";
            desc.Copy = &CopyComponentValue<LightComponent>;
            desc.Serialize = &Serialize_Light;
            desc.Deserialize = &Deserialize_Light;
            desc.Draw = &Draw_Light;
            desc.Has = [](Entity e) { return e.HasComponent<LightComponent>(); };
            desc.GetIcon = [](Entity e) -> const Ref<Texture2D>&
            {
                return EditorIconManager::GetLightIcon(e.GetComponent<LightComponent>().Type);
            };
            desc.Remove = [](Entity e) { e.RemoveComponent<LightComponent>(); };
            desc.AddMenuItems = {
                { "Directional Light",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetLightIcon(LightType::Directional); },
                  [](Entity e) { e.AddComponent<LightComponent>(LightType::Directional); } },
                { "Point Light",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetLightIcon(LightType::Point); },
                  [](Entity e) { e.AddComponent<LightComponent>(LightType::Point); } },
                { "Spot Light",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetLightIcon(LightType::Spot); },
                  [](Entity e) { e.AddComponent<LightComponent>(LightType::Spot); } },
            };
            Register(std::move(desc));
        }

        // ---- MeshFilterComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::MeshFilter;
            desc.Name = "Mesh Filter";
            desc.SerializedKey = "MeshFilterComponent";
            desc.Copy = &CopyComponentValue<MeshFilterComponent>;
            desc.Serialize = &Serialize_MeshFilter;
            desc.Deserialize = &Deserialize_MeshFilter;
            desc.Draw = &Draw_MeshFilter;
            desc.Has = [](Entity e) { return e.HasComponent<MeshFilterComponent>(); };
            desc.GetIcon = &DefaultIcon<MeshFilterComponent>;
            desc.Remove = [](Entity e) { e.RemoveComponent<MeshFilterComponent>(); };
            desc.AddMenuItems = {
                { "Mesh Filter",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetComponentIcon(ComponentType::MeshFilter); },
                  [](Entity e) { e.AddComponent<MeshFilterComponent>(); } },
            };
            Register(std::move(desc));
        }

        // ---- MeshRendererComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::MeshRenderer;
            desc.Name = "Mesh Renderer";
            desc.SerializedKey = "MeshRendererComponent";
            desc.Copy = &CopyComponentValue<MeshRendererComponent>;
            desc.Serialize = &Serialize_MeshRenderer;
            desc.Deserialize = &Deserialize_MeshRenderer;
            desc.Draw = &Draw_MeshRenderer;
            desc.Has = [](Entity e) { return e.HasComponent<MeshRendererComponent>(); };
            desc.GetIcon = &DefaultIcon<MeshRendererComponent>;
            desc.Remove = [](Entity e) { e.RemoveComponent<MeshRendererComponent>(); };
            desc.AddMenuItems = {
                { "Mesh Renderer",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetComponentIcon(ComponentType::MeshRenderer); },
                  [](Entity e) { e.AddComponent<MeshRendererComponent>(); } },
            };
            Register(std::move(desc));
        }

        // ---- SpriteRendererComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::SpriteRenderer;
            desc.Name = "Sprite Renderer";
            desc.SerializedKey = "SpriteRendererComponent";
            desc.Copy = &CopyComponentValue<SpriteRendererComponent>;
            desc.Serialize = &Serialize_SpriteRenderer;
            desc.Deserialize = &Deserialize_SpriteRenderer;
            desc.Draw = &Draw_SpriteRenderer;
            desc.Has = [](Entity e) { return e.HasComponent<SpriteRendererComponent>(); };
            desc.GetIcon = &DefaultIcon<SpriteRendererComponent>;
            desc.Remove = [](Entity e) { e.RemoveComponent<SpriteRendererComponent>(); };
            desc.AddMenuItems = {
                { "Sprite Renderer",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetComponentIcon(ComponentType::SpriteRenderer); },
                  [](Entity e) { e.AddComponent<SpriteRendererComponent>(); } },
            };
            Register(std::move(desc));
        }

        // ---- PostProcessVolumeComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::PostProcessVolume;
            desc.Name = "Post Process Volume";
            desc.SerializedKey = "PostProcessVolumeComponent";
            desc.Copy = &CopyComponentValue<PostProcessVolumeComponent>;
            desc.Serialize = &Serialize_PostProcessVolume;
            desc.Deserialize = &Deserialize_PostProcessVolume;
            desc.Draw = &Draw_PostProcessVolume;
            desc.Has = [](Entity e) { return e.HasComponent<PostProcessVolumeComponent>(); };
            desc.GetIcon = &DefaultIcon<PostProcessVolumeComponent>;
            desc.Remove = [](Entity e) { e.RemoveComponent<PostProcessVolumeComponent>(); };
            desc.AddMenuItems = {
                { "Post Process Volume",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetComponentIcon(ComponentType::PostProcessVolume); },
                  [](Entity e) { e.AddComponent<PostProcessVolumeComponent>(); } },
            };
            Register(std::move(desc));
        }

        // ---- CameraComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::Camera;
            desc.Name = "Camera";
            desc.SerializedKey = "CameraComponent";
            desc.Copy = &CopyComponentValue<CameraComponent>;
            desc.Serialize = &Serialize_Camera;
            desc.Deserialize = &Deserialize_Camera;
            desc.Draw = &Draw_Camera;
            desc.Has = [](Entity e) { return e.HasComponent<CameraComponent>(); };
            desc.GetIcon = &DefaultIcon<CameraComponent>;
            desc.Remove = [](Entity e) { e.RemoveComponent<CameraComponent>(); };
            desc.AddMenuItems = {
                { "Camera",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetComponentIcon(ComponentType::Camera); },
                  [](Entity e) { e.AddComponent<CameraComponent>(); } },
            };
            Register(std::move(desc));
        }
    }
}
