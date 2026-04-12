#include "lcpch.h"
#include "SceneSerializer.h"

#include "Lucky/Scene/Entity.h"

#include "Lucky/Scene/Components/Components.h"

#include "Lucky/Serialization/MaterialSerializer.h"
#include "Lucky/Renderer/Renderer3D.h"

#include "YamlHelpers.h"

#include <fstream>
#include <yaml-cpp/yaml.h>

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
        
        // DirectionalLight 组件
        if (entity.HasComponent<DirectionalLightComponent>())
        {
            const auto& light = entity.GetComponent<DirectionalLightComponent>();
            
            out << YAML::Key << "DirectionalLightComponent";
            
            out << YAML::BeginMap;
            out << YAML::Key << "Direction" << YAML::Value << entity.GetComponent<TransformComponent>().GetRotation();
            out << YAML::Key << "Color" << YAML::Value << light.Color;
            out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
            out << YAML::EndMap;
        }
        
        // PointLight 组件
        if (entity.HasComponent<PointLightComponent>())
        {
            const auto& light = entity.GetComponent<PointLightComponent>();

            out << YAML::Key << "PointLightComponent";
            
            out << YAML::BeginMap;
            out << YAML::Key << "Color" << YAML::Value << light.Color;
            out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
            out << YAML::Key << "Range" << YAML::Value << light.Range;
            out << YAML::EndMap;
        }

        // SpotLight 组件
        if (entity.HasComponent<SpotLightComponent>())
        {
            const auto& light = entity.GetComponent<SpotLightComponent>();

            out << YAML::Key << "SpotLightComponent";
            
            out << YAML::BeginMap;
            out << YAML::Key << "Color" << YAML::Value << light.Color;
            out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
            out << YAML::Key << "Range" << YAML::Value << light.Range;
            out << YAML::Key << "InnerCutoffAngle" << YAML::Value << light.InnerCutoffAngle;
            out << YAML::Key << "OuterCutoffAngle" << YAML::Value << light.OuterCutoffAngle;
            out << YAML::EndMap;
        }
        
        // MeshFilter 组件
        if (entity.HasComponent<MeshFilterComponent>())
        {
            const auto& meshFilterComponent = entity.GetComponent<MeshFilterComponent>();
            
            out << YAML::Key << "MeshFilterComponent";
            
            out << YAML::BeginMap;
            out << YAML::Key << "PrimitiveType" << YAML::Value << (int)meshFilterComponent.Primitive;
            out << YAML::EndMap;
        }
        
        // MeshRenderer 组件
        if (entity.HasComponent<MeshRendererComponent>())
        {
            const auto& meshRendererComponent = entity.GetComponent<MeshRendererComponent>();
            
            out << YAML::Key << "MeshRendererComponent";
            
            out << YAML::BeginMap;
            
            // 序列化材质列表
            out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;

            for (const auto& material : meshRendererComponent.Materials)
            {
                if (material)
                {
                    MaterialSerializer::Serialize(out, material);   // 序列化材质
                }
                else
                {
                    // 空材质占位（保持索引对应关系）
                    out << YAML::BeginMap;
                    out << YAML::Key << "Name" << YAML::Value << "";
                    out << YAML::Key << "Shader" << YAML::Value << "";
                    out << YAML::Key << "Properties" << YAML::Value << YAML::BeginSeq << YAML::EndSeq;
                    out << YAML::EndMap;
                }
            }

            out << YAML::EndSeq;    // 材质列表结束
            
            out << YAML::EndMap;
        }

        out << YAML::EndMap;    // 结束实体 Map
    }

    void SceneSerializer::Serialize(const std::string& filepath)
    {
        YAML::Emitter out;      // 发射器

        out << YAML::BeginMap;
        
        out << YAML::Key << "Scene" << YAML::Value << m_Scene->GetName();   // 场景：场景名
        
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
                
                // DirectionalLight 组件
                YAML::Node directionalLightComponentNode = entity["DirectionalLightComponent"];
                if (directionalLightComponentNode)
                {
                    auto& light = deserializedEntity.AddComponent<DirectionalLightComponent>();
                    
                    deserializedEntity.GetComponent<TransformComponent>().SetRotation(directionalLightComponentNode["Direction"].as<glm::quat>());
                    light.Color = directionalLightComponentNode["Color"].as<glm::vec3>();
                    light.Intensity = directionalLightComponentNode["Intensity"].as<float>();
                }
                
                // PointLight 组件
                YAML::Node pointLightComponent = entity["PointLightComponent"];
                if (pointLightComponent)
                {
                    auto& light = deserializedEntity.AddComponent<PointLightComponent>();
                    
                    light.Color = pointLightComponent["Color"].as<glm::vec3>();
                    light.Intensity = pointLightComponent["Intensity"].as<float>();
                    light.Range = pointLightComponent["Range"].as<float>();
                }

                // SpotLight 组件
                YAML::Node spotLightComponent = entity["SpotLightComponent"];
                if (spotLightComponent)
                {
                    auto& light = deserializedEntity.AddComponent<SpotLightComponent>();
                    
                    light.Color = spotLightComponent["Color"].as<glm::vec3>();
                    light.Intensity = spotLightComponent["Intensity"].as<float>();
                    light.Range = spotLightComponent["Range"].as<float>();
                    light.InnerCutoffAngle = spotLightComponent["InnerCutoffAngle"].as<float>();
                    light.OuterCutoffAngle = spotLightComponent["OuterCutoffAngle"].as<float>();
                }
                
                // MeshFilter 组件
                YAML::Node meshFilterComponentNode = entity["MeshFilterComponent"];
                if (meshFilterComponentNode)
                {
                    PrimitiveType primitiveType = (PrimitiveType)meshFilterComponentNode["PrimitiveType"].as<int>();
                    
                    auto& meshFilterComponent = deserializedEntity.AddComponent<MeshFilterComponent>(primitiveType);
                }
                
                // MeshRenderer 组件
                YAML::Node meshRendererComponentNode = entity["MeshRendererComponent"];
                if (meshRendererComponentNode)
                {
                    auto& meshRendererComponent = deserializedEntity.AddComponent<MeshRendererComponent>();
                    
                    // 反序列化材质列表
                    YAML::Node materialsNode = meshRendererComponentNode["Materials"];
                    if (materialsNode && materialsNode.IsSequence())
                    {
                        meshRendererComponent.Materials.clear();
                        meshRendererComponent.Materials.reserve(materialsNode.size());

                        for (auto materialNode : materialsNode)
                        {
                            Ref<Material> material = MaterialSerializer::Deserialize(materialNode);  // 反序列化材质

                            if (!material)
                            {
                                // 反序列化失败，材质丢失 使用错误材质
                                material = Renderer3D::GetInternalErrorMaterial();
                            }

                            meshRendererComponent.Materials.push_back(material);
                        }
                    }
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