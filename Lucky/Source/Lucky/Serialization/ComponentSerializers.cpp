#include "lcpch.h"

#include "Lucky/Scene/ComponentRegistry.h"
#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/Components.h"

#include "Lucky/Serialization/YamlHelpers.h"
#include "Lucky/Serialization/MaterialSerializer.h"

#include "Lucky/Renderer/Renderer3D.h"

#include "Lucky/Asset/AssetManager.h"
#include "Lucky/Scripting/ScriptEngine.h"

#include <yaml-cpp/yaml.h>

namespace Lucky
{
    namespace
    {
        // ======== NameComponent ========

        void Serialize_Name(YAML::Emitter& out, Entity entity)
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

        void Deserialize_Name(Entity /*entity*/, const YAML::Node& /*entityNode*/)
        {
            // NameComponent 由 Scene::CreateEntity(uuid, name) 在实体建立时直接注入
            // 反序列化路径已在 SceneSerializer::Deserialize 中通过 entity["NameComponent"]["Name"] 读取并传给 CreateEntity
        }

        // ======== TransformComponent ========

        void Serialize_Transform(YAML::Emitter& out, Entity entity)
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

        void Deserialize_Transform(Entity entity, const YAML::Node& entityNode)
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

        // ======== RelationshipComponent ========

        void Serialize_Relationship(YAML::Emitter& out, Entity entity)
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

        void Deserialize_Relationship(Entity entity, const YAML::Node& entityNode)
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

        void Serialize_MeshFilter(YAML::Emitter& out, Entity entity)
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

        void Deserialize_MeshFilter(Entity entity, const YAML::Node& entityNode)
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

        // ======== MeshRendererComponent ========

        void Serialize_MeshRenderer(YAML::Emitter& out, Entity entity)
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

        void Deserialize_MeshRenderer(Entity entity, const YAML::Node& entityNode)
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

        // ======== SpriteRendererComponent ========

        void Serialize_SpriteRenderer(YAML::Emitter& out, Entity entity)
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

        void Deserialize_SpriteRenderer(Entity entity, const YAML::Node& entityNode)
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

        // ======== LightComponent ========

        void Serialize_Light(YAML::Emitter& out, Entity entity)
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

        void Deserialize_Light(Entity entity, const YAML::Node& entityNode)
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

        // ======== PostProcessVolumeComponent ========

        void Serialize_PostProcessVolume(YAML::Emitter& out, Entity entity)
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

        void Deserialize_PostProcessVolume(Entity entity, const YAML::Node& entityNode)
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

        // ======== CameraComponent ========

        void Serialize_Camera(YAML::Emitter& out, Entity entity)
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

        void Deserialize_Camera(Entity entity, const YAML::Node& entityNode)
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

        // ======== ScriptComponent ========

        void Serialize_Script(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<ScriptComponent>())
            {
                return;
            }
            const ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

            out << YAML::Key << "ScriptComponent";
            out << YAML::BeginMap;
            if (sc.ScriptAsset)
            {
                out << YAML::Key << "AssetHandle" << YAML::Value << sc.ScriptAsset->GetHandle();
            }
            else
            {
                out << YAML::Key << "AssetHandle" << YAML::Value << static_cast<uint64_t>(0);
            }

            // 按字段名排序输出：unordered_map 迭代顺序不稳定，直写会让每次保存的存档 diff 抖动
            std::vector<std::string> sortedFieldNames;
            sortedFieldNames.reserve(sc.Fields.size());
            for (const auto& [fieldName, fieldValue] : sc.Fields)
            {
                sortedFieldNames.push_back(fieldName);
            }
            std::sort(sortedFieldNames.begin(), sortedFieldNames.end());

            out << YAML::Key << "Fields" << YAML::Value << YAML::BeginMap;
            for (const std::string& fieldName : sortedFieldNames)
            {
                const ScriptFieldValue& fieldValue = sc.Fields.at(fieldName);
                out << YAML::Key << fieldName;
                out << YAML::BeginMap;
                out << YAML::Key << "Type" << YAML::Value << GetScriptFieldTypeInfo(fieldValue.Type).Name;

                switch (fieldValue.Type)
                {
                    case ScriptFieldType::Bool:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<bool>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::SByte:
                    {
                        // yaml-cpp 会把 int8_t 当字符输出，先提升为 int32
                        out << YAML::Key << "Value" << YAML::Value << static_cast<int32_t>(std::get<int8_t>(fieldValue.Data));
                        break;
                    }
                    case ScriptFieldType::Byte:
                    {
                        out << YAML::Key << "Value" << YAML::Value << static_cast<uint32_t>(std::get<uint8_t>(fieldValue.Data));
                        break;
                    }
                    case ScriptFieldType::Short:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<int16_t>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::UShort:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<uint16_t>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Int:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<int32_t>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::UInt:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<uint32_t>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Long:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<int64_t>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::ULong:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<uint64_t>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Float:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<float>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Double:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<double>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::String:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<std::string>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Vector2:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<glm::vec2>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Vector3:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<glm::vec3>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Vector4:
                    case ScriptFieldType::Color:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<glm::vec4>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Quaternion:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<glm::quat>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Entity:
                    {
                        out << YAML::Key << "Value" << YAML::Value << static_cast<uint64_t>(std::get<UUID>(fieldValue.Data));
                        break;
                    }
                    case ScriptFieldType::Material:
                    case ScriptFieldType::Mesh:
                    case ScriptFieldType::Texture2D:
                    case ScriptFieldType::Script:
                    {
                        // 运行时创建的资产没有有效 handle，写出 0（读回是空引用，与 Unity 一致）
                        const Ref<Asset>& asset = std::get<Ref<Asset>>(fieldValue.Data);
                        out << YAML::Key << "Value" << YAML::Value << static_cast<uint64_t>(asset ? asset->GetHandle() : AssetHandle{});
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }

                out << YAML::EndMap;
            }
            out << YAML::EndMap;

            out << YAML::EndMap;
        }

        void Deserialize_Script(Entity entity, const YAML::Node& entityNode)
        {
            YAML::Node node = entityNode["ScriptComponent"];
            if (!node)
            {
                return;
            }
            ScriptComponent& sc = entity.AddComponent<ScriptComponent>();

            YAML::Node handleNode = node["AssetHandle"];
            if (!handleNode)
            {
                return;     // 缺字段：脚本引用保持为空，等价于 Inspector 里的 None
            }

            uint64_t handleValue = handleNode.as<uint64_t>();
            AssetHandle handle(handleValue);
            if (handle.IsValid())
            {
                sc.ScriptAsset = AssetManager::GetAsset<Script>(handle);
            }

            if (!sc.ScriptAsset && handle.IsValid())
            {
                LF_CORE_ERROR("SceneSerializer: Failed to load script asset [{0}]", handleValue);
            }

            YAML::Node fieldsNode = node["Fields"];
            if (fieldsNode && fieldsNode.IsMap())
            {
                for (auto fieldNode : fieldsNode)
                {
                    const std::string fieldName = fieldNode.first.as<std::string>();
                    YAML::Node valueNode = fieldNode.second;
                    if (!valueNode || !valueNode["Type"])
                    {
                        continue;
                    }

                    ScriptFieldValue fieldValue;
                    if (!TryGetScriptFieldTypeByName(valueNode["Type"].as<std::string>("").c_str(), fieldValue.Type))
                    {
                        continue;   // 未知类型：跳过，由 SyncScriptFieldMap 用脚本默认值补齐
                    }

                    YAML::Node scalarNode = valueNode["Value"];
                    switch (fieldValue.Type)
                    {
                        case ScriptFieldType::Bool:   { fieldValue.Data = scalarNode.as<bool>(false); break; }
                        case ScriptFieldType::SByte:  { fieldValue.Data = static_cast<int8_t>(scalarNode.as<int32_t>(0)); break; }
                        case ScriptFieldType::Byte:   { fieldValue.Data = static_cast<uint8_t>(scalarNode.as<uint32_t>(0)); break; }
                        case ScriptFieldType::Short:  { fieldValue.Data = scalarNode.as<int16_t>(0); break; }
                        case ScriptFieldType::UShort: { fieldValue.Data = scalarNode.as<uint16_t>(0); break; }
                        case ScriptFieldType::Int:    { fieldValue.Data = scalarNode.as<int32_t>(0); break; }
                        case ScriptFieldType::UInt:   { fieldValue.Data = scalarNode.as<uint32_t>(0); break; }
                        case ScriptFieldType::Long:   { fieldValue.Data = scalarNode.as<int64_t>(0); break; }
                        case ScriptFieldType::ULong:  { fieldValue.Data = scalarNode.as<uint64_t>(0); break; }
                        case ScriptFieldType::Float:  { fieldValue.Data = scalarNode.as<float>(0.0f); break; }
                        case ScriptFieldType::Double: { fieldValue.Data = scalarNode.as<double>(0.0); break; }
                        case ScriptFieldType::String: { fieldValue.Data = scalarNode.as<std::string>(""); break; }
                        case ScriptFieldType::Vector2: { fieldValue.Data = scalarNode.as<glm::vec2>(glm::vec2(0.0f)); break; }
                        case ScriptFieldType::Vector3: { fieldValue.Data = scalarNode.as<glm::vec3>(glm::vec3(0.0f)); break; }
                        case ScriptFieldType::Vector4:
                        case ScriptFieldType::Color:   { fieldValue.Data = scalarNode.as<glm::vec4>(glm::vec4(0.0f)); break; }
                        case ScriptFieldType::Quaternion: { fieldValue.Data = scalarNode.as<glm::quat>(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)); break; }
                        case ScriptFieldType::Entity: { fieldValue.Data = UUID(scalarNode.as<uint64_t>(0)); break; }
                        case ScriptFieldType::Material:
                        case ScriptFieldType::Mesh:
                        case ScriptFieldType::Texture2D:
                        case ScriptFieldType::Script:
                        {
                            const AssetHandle handle(scalarNode.as<uint64_t>(0));
                            fieldValue.Data = ScriptEngine::ResolveAssetByFieldType(fieldValue.Type, handle);
                            break;
                        }
                        default:
                        {
                            continue;
                        }
                    }

                    sc.Fields[fieldName] = fieldValue;
                }
            }
        }
    }

    void ComponentRegistry::RegisterAllSerializations()
    {
        RegisterSerialization(ComponentType::Name,              "NameComponent",              &Serialize_Name,              &Deserialize_Name);
        RegisterSerialization(ComponentType::Transform,         "TransformComponent",         &Serialize_Transform,         &Deserialize_Transform);
        RegisterSerialization(ComponentType::Relationship,      "RelationshipComponent",      &Serialize_Relationship,      &Deserialize_Relationship);
        RegisterSerialization(ComponentType::Light,             "LightComponent",             &Serialize_Light,             &Deserialize_Light);
        RegisterSerialization(ComponentType::MeshFilter,        "MeshFilterComponent",        &Serialize_MeshFilter,        &Deserialize_MeshFilter);
        RegisterSerialization(ComponentType::MeshRenderer,      "MeshRendererComponent",      &Serialize_MeshRenderer,      &Deserialize_MeshRenderer);
        RegisterSerialization(ComponentType::SpriteRenderer,    "SpriteRendererComponent",    &Serialize_SpriteRenderer,    &Deserialize_SpriteRenderer);
        RegisterSerialization(ComponentType::PostProcessVolume, "PostProcessVolumeComponent", &Serialize_PostProcessVolume, &Deserialize_PostProcessVolume);
        RegisterSerialization(ComponentType::Camera,            "CameraComponent",            &Serialize_Camera,            &Deserialize_Camera);
        RegisterSerialization(ComponentType::Script,            "ScriptComponent",            &Serialize_Script,            &Deserialize_Script);
    }
}
