#include "lcpch.h"
#include "SceneSerializer.h"

#include "Entity.h"

#include "Components/IDComponent.h"
#include "Components/NameComponent.h"
#include "Components/TransformComponent.h"
#include "Components/RelationshipComponent.h"
#include "Components/MeshFilterComponent.h"
#include "Components/MeshRendererComponent.h"
#include "Components/DirectionalLightComponent.h"

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace YAML
{
    /// <summary>
    /// vec2 转换
    /// </summary>
    template<>
    struct convert<glm::vec2>
    {
        /// <summary>
        /// 将 vec2 转换为 YAML 的节点
        /// </summary>
        /// <param name="rhs">vec2 类型</param>
        /// <returns>结点</returns>
        static Node encode(const glm::vec2& rhs)
        {
            Node node;

            node.push_back(rhs.x);
            node.push_back(rhs.y);

            node.SetStyle(EmitterStyle::Flow);

            return node;
        }

        /// <summary>
        /// 将 YAML 结点类型转换为 vec2
        /// </summary>
        /// <param name="node">结点</param>
        /// <param name="rhs">vec2</param>
        /// <returns>是否转换成功</returns>
        static bool decode(const Node& node, glm::vec2& rhs)
        {
            if (!node.IsSequence() || node.size() != 2)
            {
                return false;
            }

            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();

            return true;
        }
    };

    /// <summary>
    /// vec3 转换
    /// </summary>
    template<>
    struct convert<glm::vec3>
    {
        /// <summary>
        /// 将 vec3 转换为 YAML 的节点
        /// </summary>
        /// <param name="rhs">vec3 类型</param>
        /// <returns>结点</returns>
        static Node encode(const glm::vec3& rhs)
        {
            Node node;

            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);

            node.SetStyle(EmitterStyle::Flow);

            return node;
        }

        /// <summary>
        /// 将 YAML 结点类型转换为 vec3
        /// </summary>
        /// <param name="node">结点</param>
        /// <param name="rhs">vec3</param>
        /// <returns>是否转换成功</returns>
        static bool decode(const Node& node, glm::vec3& rhs)
        {
            if (!node.IsSequence() || node.size() != 3)
            {
                return false;
            }

            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();

            return true;
        }
    };

    /// <summary>
    /// vec4 转换
    /// </summary>
    template<>
    struct convert<glm::vec4>
    {
        static Node encode(const glm::vec4& rhs)
        {
            Node node;

            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);

            node.SetStyle(EmitterStyle::Flow);

            return node;
        }

        static bool decode(const Node& node, glm::vec4& rhs)
        {
            if (!node.IsSequence() || node.size() != 4)
            {
                return false;
            }

            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            rhs.w = node[3].as<float>();

            return true;
        }
    };
    
    /// <summary>
    /// quat 转换
    /// </summary>
    template<>
    struct convert<glm::quat>
    {
        static Node encode(const glm::quat& rhs)
        {
            Node node;
            
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);
            
            node.SetStyle(EmitterStyle::Flow);
            return node;
        }
        
        static bool decode(const Node& node, glm::quat& rhs)
        {
            if (!node.IsSequence() || node.size() != 4)
            {
                return false;
            }
            
            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            rhs.w = node[3].as<float>();
            
            return true;
        }
    };

    /// <summary>
    /// UUID 转换
    /// </summary>
    template<>
    struct convert<Lucky::UUID>
    {
        static Node encode(const Lucky::UUID& uuid)
        {
            Node node;
            node.push_back((uint64_t)uuid);

            return node;
        }

        static bool decode(const Node& node, Lucky::UUID& uuid)
        {
            uuid = node.as<uint64_t>();

            return true;
        }
    };
}

namespace Lucky
{
    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
    {
        out << YAML::Flow;    // 流 [x,y]
        out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;

        return out;
    }

    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;

        return out;
    }

    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;

        return out;
    }
    
    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::quat& q)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << q.x << q.y << q.z << q.w << YAML::EndSeq;

        return out;
    };

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
            out << YAML::Key << "NameComponent";
            
            out << YAML::BeginMap;

            const auto& nameComponent = entity.GetComponent<NameComponent>(); // Name 组件

            out << YAML::Key << "Name" << YAML::Value << nameComponent.Name;

            out << YAML::EndMap;
        }

        // Transform 组件
        if (entity.HasComponent<TransformComponent>())
        {
            out << YAML::Key << "TransformComponent";
            
            out << YAML::BeginMap;
            
            const auto& transformComponent = entity.GetComponent<TransformComponent>();

            out << YAML::Key << "Position" << YAML::Value << transformComponent.Translation;
            out << YAML::Key << "Rotation" << YAML::Value << transformComponent.GetRotation();
            out << YAML::Key << "Scale" << YAML::Value << transformComponent.Scale;

            out << YAML::EndMap;
        }
        
        // Relationship 组件
        if (entity.HasComponent<RelationshipComponent>())
        {
            out << YAML::Key << "RelationshipComponent";
            
            out << YAML::BeginMap;
            
            const auto& relationshipComponent = entity.GetComponent<RelationshipComponent>();
            
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
            out << YAML::Key << "DirectionalLightComponent";
            
            out << YAML::BeginMap;
            
            const auto& directionalLightComponent = entity.GetComponent<DirectionalLightComponent>();
            
            out << YAML::Key << "Direction" << YAML::Value << entity.GetComponent<TransformComponent>().GetRotation();
            out << YAML::Key << "Color" << YAML::Value << directionalLightComponent.Color;
            out << YAML::Key << "Intensity" << YAML::Value << directionalLightComponent.Intensity;
            
            out << YAML::EndMap;
        }
        
        // MeshFilter 组件
        if (entity.HasComponent<MeshFilterComponent>())
        {
            out << YAML::Key << "MeshFilterComponent";
            
            out << YAML::BeginMap;
            
            const auto& meshFilterComponent = entity.GetComponent<MeshFilterComponent>();
            
            out << YAML::Key << "Name" << YAML::Value << meshFilterComponent.Mesh->GetName();
            out << YAML::Key << "PrimitiveType" << YAML::Value << (int)meshFilterComponent.Primitive;
            
            out << YAML::EndMap;
        }
        
        // MeshRenderer 组件
        if (entity.HasComponent<MeshRendererComponent>())
        {
            // out << YAML::Key << "MeshRendererComponent";
            //
            // out << YAML::BeginMap;
            //
            // const auto& meshRendererComponent = entity.GetComponent<MeshRendererComponent>();
            //
            // // TODO 序列化材质
            //
            // out << YAML::EndMap;
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
                    auto& directionalLight = deserializedEntity.AddComponent<DirectionalLightComponent>();
                    
                    deserializedEntity.GetComponent<TransformComponent>().SetRotation(directionalLightComponentNode["Direction"].as<glm::quat>());
                    directionalLight.Color = directionalLightComponentNode["Color"].as<glm::vec3>();
                    directionalLight.Intensity = directionalLightComponentNode["Intensity"].as<float>();
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
                    auto& meshRendererComponent = deserializedEntity.GetComponent<MeshRendererComponent>();
                    
                    // TODO 反序列化材质
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