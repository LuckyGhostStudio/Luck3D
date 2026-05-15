#include "lcpch.h"
#include "SceneSerializer.h"

#include "Lucky/Scene/Entity.h"

#include "Lucky/Scene/Components/Components.h"

#include "Lucky/Serialization/MaterialSerializer.h"
#include "Lucky/Renderer/Renderer3D.h"

#include "Lucky/Asset/MeshImporter.h"
#include "Lucky/Asset/AssetManager.h"
#include "YamlHelpers.h"

#include <fstream>
#include <yaml-cpp/yaml.h>
#include <filesystem>

namespace Lucky
{
    SceneSerializer::SceneSerializer(const Ref<Scene> scene)
        : m_Scene(scene)
    {

    }

    /// <summary>
    /// 序列化实体
    /// </summary>
    /// <param name="out">发射器</param>
    /// <param name="entity">实体</param>
    static void SerializeEntity(YAML::Emitter& out, Entity entity)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

        // Name 组件
        if (entity.HasComponent<NameComponent>())
        {
            const auto& nameComponent = entity.GetComponent<NameComponent>(); // Name 组件
            
            out << YAML::Key << "NameComponent";
            
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << nameComponent.Name;
            out << YAML::EndMap;
        }

        // Transform 组件
        if (entity.HasComponent<TransformComponent>())
        {
            const auto& transformComponent = entity.GetComponent<TransformComponent>();
            
            out << YAML::Key << "TransformComponent";
            
            out << YAML::BeginMap;
            out << YAML::Key << "Position" << YAML::Value << transformComponent.Translation;
            out << YAML::Key << "Rotation" << YAML::Value << transformComponent.GetRotation();
            out << YAML::Key << "Scale" << YAML::Value << transformComponent.Scale;
            out << YAML::EndMap;
        }
        
        // Relationship 组件
        if (entity.HasComponent<RelationshipComponent>())
        {
            const auto& relationshipComponent = entity.GetComponent<RelationshipComponent>();
            
            out << YAML::Key << "RelationshipComponent";
            
            out << YAML::BeginMap;
            out << YAML::Key << "Parent" << YAML::Value << relationshipComponent.Parent;
            
            out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
            for (const auto& child : relationshipComponent.Children)
            {                
                out << YAML::BeginMap;
                out << YAML::Key << "Child" << YAML::Value << child;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
            
            out << YAML::EndMap;
        }
        
        // Light 组件
        if (entity.HasComponent<LightComponent>())
        {
            const auto& light = entity.GetComponent<LightComponent>();

            out << YAML::Key << "LightComponent";

            out << YAML::BeginMap;
            out << YAML::Key << "Type" << YAML::Value << static_cast<int>(light.Type);
            out << YAML::Key << "Color" << YAML::Value << light.Color;
            out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;

            // Point / Spot 属性
            if (light.Type == LightType::Point || light.Type == LightType::Spot)
            {
                out << YAML::Key << "Range" << YAML::Value << light.Range;
            }

            // Spot 属性
            if (light.Type == LightType::Spot)
            {
                out << YAML::Key << "InnerCutoffAngle" << YAML::Value << light.InnerCutoffAngle;
                out << YAML::Key << "OuterCutoffAngle" << YAML::Value << light.OuterCutoffAngle;
            }

            // 阴影属性（所有类型）
            out << YAML::Key << "Shadows" << YAML::Value << static_cast<int>(light.Shadows);
            out << YAML::Key << "ShadowBias" << YAML::Value << light.ShadowBias;
            out << YAML::Key << "ShadowStrength" << YAML::Value << light.ShadowStrength;

            // CSM 属性（仅方向光）
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
        
        // MeshFilter 组件
        if (entity.HasComponent<MeshFilterComponent>())
        {
            const auto& meshFilterComponent = entity.GetComponent<MeshFilterComponent>();
            
            out << YAML::Key << "MeshFilterComponent";
            out << YAML::BeginMap;
            
            out << YAML::Key << "PrimitiveType" << YAML::Value << static_cast<int>(meshFilterComponent.Primitive);
            
            // 外部模型：通过 Mesh 实例获取 AssetHandle
            if (meshFilterComponent.Mesh && meshFilterComponent.Primitive == PrimitiveType::None)
            {
                out << YAML::Key << "MeshAsset" << YAML::Value << static_cast<uint64_t>(meshFilterComponent.Mesh->GetHandle());
            }
            else
            {
                out << YAML::Key << "MeshAsset" << YAML::Value << static_cast<uint64_t>(0);
            }
            
            out << YAML::EndMap;
        }
        
        // MeshRenderer 组件
        if (entity.HasComponent<MeshRendererComponent>())
        {
            const auto& meshRendererComponent = entity.GetComponent<MeshRendererComponent>();
            
            out << YAML::Key << "MeshRendererComponent";
            
            out << YAML::BeginMap;
            
            // 序列化材质列表（使用 AssetHandle 引用）
            out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;

            for (const auto& material : meshRendererComponent.Materials)
            {
                out << YAML::BeginMap;
                if (material)
                {
                    // 通过 Material 实例获取 AssetHandle（Material 继承 Asset，自带 Handle）
                    out << YAML::Key << "AssetHandle" << YAML::Value << static_cast<uint64_t>(material->GetHandle());
                }
                else
                {
                    out << YAML::Key << "AssetHandle" << YAML::Value << static_cast<uint64_t>(0);
                }
                out << YAML::EndMap;
            }

            out << YAML::EndSeq;    // 材质列表结束
            
            out << YAML::EndMap;
        }

        // PostProcessVolume 组件
        if (entity.HasComponent<PostProcessVolumeComponent>())
        {
            const auto& volume = entity.GetComponent<PostProcessVolumeComponent>();

            out << YAML::Key << "PostProcessVolumeComponent";

            out << YAML::BeginMap;
            out << YAML::Key << "IsGlobal" << YAML::Value << volume.IsGlobal;
            out << YAML::Key << "Priority" << YAML::Value << volume.Priority;

            // Tonemapping
            out << YAML::Key << "TonemapMode" << YAML::Value << static_cast<int>(volume.Tonemap);
            out << YAML::Key << "Exposure" << YAML::Value << volume.Exposure;

            // Bloom
            out << YAML::Key << "BloomEnabled" << YAML::Value << volume.BloomEnabled;
            out << YAML::Key << "BloomThreshold" << YAML::Value << volume.BloomThreshold;
            out << YAML::Key << "BloomIntensity" << YAML::Value << volume.BloomIntensity;
            out << YAML::Key << "BloomIterations" << YAML::Value << volume.BloomIterations;

            // FXAA
            out << YAML::Key << "FXAAEnabled" << YAML::Value << volume.FXAAEnabled;

            // Vignette
            out << YAML::Key << "VignetteEnabled" << YAML::Value << volume.VignetteEnabled;
            out << YAML::Key << "VignetteIntensity" << YAML::Value << volume.VignetteIntensity;
            out << YAML::Key << "VignetteSmoothness" << YAML::Value << volume.VignetteSmoothness;

            out << YAML::EndMap;
        }

        out << YAML::EndMap;    // 结束实体 Map
    }

    void SceneSerializer::Serialize(const std::string& filepath)
    {
        YAML::Emitter out;      // 发射器

        out << YAML::BeginMap;
        
        out << YAML::Key << "Scene" << YAML::Value << m_Scene->GetName();   // 场景：场景名
        
        // ---- 环境设置 ----
        {
            const EnvironmentSettings& env = m_Scene->GetEnvironmentSettings();
            out << YAML::Key << "EnvironmentSettings" << YAML::Value;
            out << YAML::BeginMap;
            
            // 天空盒材质
            out << YAML::Key << "SkyboxMaterial" << YAML::Value;
            if (env.SkyboxMaterial)
            {
                MaterialSerializer::Serialize(out, env.SkyboxMaterial);
            }
            else
            {
                out << YAML::Null;
            }
            
            out << YAML::Key << "AmbientSource" << YAML::Value << static_cast<int>(env.Source);
            out << YAML::Key << "AmbientColor" << YAML::Value << env.AmbientColor;
            out << YAML::Key << "DiffuseIntensity" << YAML::Value << env.DiffuseIntensity;
            out << YAML::Key << "SpecularIntensity" << YAML::Value << env.SpecularIntensity;
            out << YAML::Key << "ReflectionResolution" << YAML::Value << env.ReflectionResolution;
            out << YAML::EndMap;
        }
        
        out << YAML::Key << "Entitys" << YAML::Value << YAML::BeginSeq;     // 实体序列：开始实体序列

        // 遍历场景注册表所有实体
        m_Scene->m_Registry.each([&](auto entityID)
        {
            Entity entity = { entityID, m_Scene.get() };
            if (!entity)
            {
                return;
            }

            SerializeEntity(out, entity);   // 序列化实体
        });

        out << YAML::EndSeq;    // 结束实体序列
        
        out << YAML::EndMap;

        std::ofstream fout(filepath);   // 输出流
        fout << out.c_str();            // 输出序列化结果到输出流文件
    }

    void SceneSerializer::SerializeRuntime(const std::string& filepath)
    {
        LF_CORE_ASSERT(false, "Not implemented!");
    }

    bool SceneSerializer::Deserialize(const std::string& filepath)
    {
        YAML::Node data = YAML::LoadFile(filepath); // 加载到 YMAL 结点

        // Scene 节点不存在
        if (!data["Scene"])
        {
            return false;
        }

        std::string sceneName = data["Scene"].as<std::string>();    // 场景名
        m_Scene->SetName(sceneName);

        LF_CORE_TRACE("Deserializing scene '{0}'", sceneName);

        // ---- 反序列化环境设置 ----
        YAML::Node envNode = data["EnvironmentSettings"];
        if (envNode)
        {
            EnvironmentSettings& env = m_Scene->GetEnvironmentSettings();
            
            // 反序列化天空盒材质
            YAML::Node skyboxMatNode = envNode["SkyboxMaterial"];
            if (skyboxMatNode && !skyboxMatNode.IsNull())
            {
                Ref<Material> skyboxMat = MaterialSerializer::Deserialize(skyboxMatNode);
                if (skyboxMat)
                {
                    env.SkyboxMaterial = skyboxMat;
                }
            }
            
            env.Source = static_cast<AmbientSource>(envNode["AmbientSource"].as<int>());
            env.AmbientColor = envNode["AmbientColor"].as<glm::vec3>();
            env.DiffuseIntensity = envNode["DiffuseIntensity"].as<float>();
            env.SpecularIntensity = envNode["SpecularIntensity"].as<float>();
            if (envNode["ReflectionResolution"])
            {
                env.ReflectionResolution = envNode["ReflectionResolution"].as<int>();
            }
        }

        YAML::Node entitys = data["Entitys"];   // 实体序列结点
        
        if (entitys)
        {
            // 遍历结点下所有实体
            for (auto entity : entitys)
            {
                uint64_t uuid = entity["Entity"].as<uint64_t>();    // UUID
                
                std::string entityName;

                // Name 组件结点
                YAML::Node nameComponentNode = entity["NameComponent"];
                if (nameComponentNode)
                {
                    entityName = nameComponentNode["Name"].as<std::string>(); // 实体名
                }

                LF_CORE_TRACE("Deserialized Entity: [UUID = {0}, Name = {1}]", uuid, entityName);

                Entity deserializedEntity = m_Scene->CreateEntity(uuid, entityName);  // 创建实体

                // Transform 组件
                YAML::Node transformComponentNode = entity["TransformComponent"];
                if (transformComponentNode)
                {
                    auto& transformComponent = deserializedEntity.GetComponent<TransformComponent>();  // 获取 Transform 组件

                    transformComponent.Translation = transformComponentNode["Position"].as<glm::vec3>();
                    transformComponent.SetRotation(transformComponentNode["Rotation"].as<glm::quat>());
                    transformComponent.Scale = transformComponentNode["Scale"].as<glm::vec3>();
                }

                // Relationship 组件
                YAML::Node relationshipComponentNode = entity["RelationshipComponent"];
                if (relationshipComponentNode)
                {
                    auto& relationshipComponent = deserializedEntity.GetComponent<RelationshipComponent>();
                    
                    relationshipComponent.Parent = relationshipComponentNode["Parent"].as<UUID>();
                    
                    relationshipComponent.Children.clear();
                    YAML::Node childrenNode = relationshipComponentNode["Children"];
                    if (childrenNode)
                    {
                        for (auto children : childrenNode)
                        {
                            uint64_t child = children["Child"].as<uint64_t>();
                            relationshipComponent.Children.push_back(child);
                        }
                    }
                }
                
                // Light 组件
                YAML::Node lightComponentNode = entity["LightComponent"];
                if (lightComponentNode)
                {
                    LightType type = static_cast<LightType>(lightComponentNode["Type"].as<int>());
                    auto& light = deserializedEntity.AddComponent<LightComponent>(type);

                    light.Color = lightComponentNode["Color"].as<glm::vec3>();
                    light.Intensity = lightComponentNode["Intensity"].as<float>();

                    if (lightComponentNode["Range"])
                    {
                        light.Range = lightComponentNode["Range"].as<float>();
                    }
                    if (lightComponentNode["InnerCutoffAngle"])
                    {
                        light.InnerCutoffAngle = lightComponentNode["InnerCutoffAngle"].as<float>();
                    }
                    if (lightComponentNode["OuterCutoffAngle"])
                    {
                        light.OuterCutoffAngle = lightComponentNode["OuterCutoffAngle"].as<float>();
                    }

                    light.Shadows = static_cast<ShadowType>(lightComponentNode["Shadows"].as<int>());
                    light.ShadowBias = lightComponentNode["ShadowBias"].as<float>();
                    light.ShadowStrength = lightComponentNode["ShadowStrength"].as<float>();

                    // CSM 属性
                    if (lightComponentNode["CascadeCount"])
                    {
                        light.CascadeCount = lightComponentNode["CascadeCount"].as<int>();
                    }
                    if (lightComponentNode["ShadowDistance"])
                    {
                        light.ShadowDistance = lightComponentNode["ShadowDistance"].as<float>();
                    }
                    if (lightComponentNode["ShadowMapResolution"])
                    {
                        light.ShadowMapResolution = lightComponentNode["ShadowMapResolution"].as<int>();
                    }
                    if (lightComponentNode["CascadeSplits"])
                    {
                        auto splitSeq = lightComponentNode["CascadeSplits"].as<std::vector<float>>();
                        for (int i = 0; i < 4 && i < (int)splitSeq.size(); ++i)
                        {
                            light.CascadeSplits[i] = splitSeq[i];
                        }
                    }
                }
                
                // MeshFilter 组件
                YAML::Node meshFilterComponentNode = entity["MeshFilterComponent"];
                if (meshFilterComponentNode)
                {
                    PrimitiveType primitiveType = static_cast<PrimitiveType>(meshFilterComponentNode["PrimitiveType"].as<int>());
                    
                    if (primitiveType != PrimitiveType::None)
                    {
                        // 内置图元：通过 MeshFactory 创建
                        deserializedEntity.AddComponent<MeshFilterComponent>(primitiveType);
                    }
                    else if (meshFilterComponentNode["MeshAsset"])
                    {
                        // 外部模型：通过 AssetManager 加载
                        uint64_t meshHandleValue = meshFilterComponentNode["MeshAsset"].as<uint64_t>();
                        AssetHandle meshHandle(meshHandleValue);
                        
                        auto& meshFilterComponent = deserializedEntity.AddComponent<MeshFilterComponent>();
                        
                        if (meshHandle.IsValid())
                        {
                            Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(meshHandle);
                            if (mesh)
                            {
                                meshFilterComponent.Mesh = mesh;
                            }
                            else
                            {
                                LF_CORE_ERROR("SceneSerializer: Failed to load mesh asset [{0}]", meshHandleValue);
                            }
                        }
                    }
                    else
                    {
                        // 无图元也无资产引用，添加空组件
                        deserializedEntity.AddComponent<MeshFilterComponent>();
                    }
                }
                
                // MeshRenderer 组件
                YAML::Node meshRendererComponentNode = entity["MeshRendererComponent"];
                if (meshRendererComponentNode)
                {
                    auto& meshRendererComponent = deserializedEntity.AddComponent<MeshRendererComponent>();
                    
                    // 反序列化材质列表（通过 AssetHandle 引用）
                    YAML::Node materialsNode = meshRendererComponentNode["Materials"];
                    if (materialsNode && materialsNode.IsSequence())
                    {
                        meshRendererComponent.Materials.clear();
                        meshRendererComponent.Materials.reserve(materialsNode.size());

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
                                
                                meshRendererComponent.Materials.push_back(material);
                            }
                            else
                            {
                                // 兼容旧格式：内嵌材质数据
                                Ref<Material> material = MaterialSerializer::Deserialize(materialNode);
                                if (!material)
                                {
                                    material = Renderer3D::GetInternalErrorMaterial();
                                }
                                meshRendererComponent.Materials.push_back(material);
                            }
                        }
                    }
                }

                // PostProcessVolume 组件
                YAML::Node postProcessVolumeNode = entity["PostProcessVolumeComponent"];
                if (postProcessVolumeNode)
                {
                    auto& volume = deserializedEntity.AddComponent<PostProcessVolumeComponent>();

                    volume.IsGlobal = postProcessVolumeNode["IsGlobal"].as<bool>();
                    volume.Priority = postProcessVolumeNode["Priority"].as<float>();

                    // Tonemapping
                    volume.Tonemap = static_cast<TonemapMode>(postProcessVolumeNode["TonemapMode"].as<int>());
                    volume.Exposure = postProcessVolumeNode["Exposure"].as<float>();

                    // Bloom
                    volume.BloomEnabled = postProcessVolumeNode["BloomEnabled"].as<bool>();
                    volume.BloomThreshold = postProcessVolumeNode["BloomThreshold"].as<float>();
                    volume.BloomIntensity = postProcessVolumeNode["BloomIntensity"].as<float>();
                    volume.BloomIterations = postProcessVolumeNode["BloomIterations"].as<int>();

                    // FXAA
                    volume.FXAAEnabled = postProcessVolumeNode["FXAAEnabled"].as<bool>();

                    // Vignette
                    volume.VignetteEnabled = postProcessVolumeNode["VignetteEnabled"].as<bool>();
                    volume.VignetteIntensity = postProcessVolumeNode["VignetteIntensity"].as<float>();
                    volume.VignetteSmoothness = postProcessVolumeNode["VignetteSmoothness"].as<float>();
                }
            }
        }

        return true;
    }

    bool SceneSerializer::DeserializeRuntime(const std::string& filepath)
    {
        LF_CORE_ASSERT(false, "Not implemented!");

        return false;
    }
}