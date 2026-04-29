#include "lcpch.h"
#include "SceneSerializer.h"

#include "Lucky/Scene/Entity.h"

#include "Lucky/Scene/Components/Components.h"

#include "Lucky/Serialization/MaterialSerializer.h"
#include "Lucky/Renderer/Renderer3D.h"

#include "Lucky/Asset/MeshImporter.h"
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

            out << YAML::EndMap;
        }
        
        // MeshFilter 组件
        if (entity.HasComponent<MeshFilterComponent>())
        {
            const auto& meshFilterComponent = entity.GetComponent<MeshFilterComponent>();
            
            out << YAML::Key << "MeshFilterComponent";
            out << YAML::BeginMap;
            
            out << YAML::Key << "PrimitiveType" << YAML::Value << static_cast<int>(meshFilterComponent.Primitive);
            out << YAML::Key << "MeshFilePath" << YAML::Value << meshFilterComponent.MeshFilePath;
            
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
                }
                
                // MeshFilter 组件
                YAML::Node meshFilterComponentNode = entity["MeshFilterComponent"];
                if (meshFilterComponentNode)
                {
                    
                    PrimitiveType primitiveType = static_cast<PrimitiveType>(meshFilterComponentNode["PrimitiveType"].as<int>());
                    auto& meshFilterComponent = deserializedEntity.AddComponent<MeshFilterComponent>(primitiveType);
                    
                    std::string path = meshFilterComponentNode["MeshFilePath"].as<std::string>();
                    if (!path.empty())
                    {
                        std::string absolutePath = std::filesystem::absolute(path).string();    // 绝对路径
                        MeshImportResult result = MeshImporter::Import(absolutePath);
                        if (result.Success)
                        {
                            meshFilterComponent.MeshFilePath = path;
                            meshFilterComponent.Mesh = result.MeshData;
                        }
                    }
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