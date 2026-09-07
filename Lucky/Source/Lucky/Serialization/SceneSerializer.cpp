#include "lcpch.h"
#include "SceneSerializer.h"

#include "Lucky/Scene/Entity.h"

#include "Lucky/Scene/Components/Components.h"
#include "Lucky/Scene/Components/ComponentRegistry.h"

#include "Lucky/Asset/AssetManager.h"
#include "YamlHelpers.h"

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace Lucky
{
    /// <summary>
    /// 序列化实体：遍历 ComponentRegistry，逐个组件写入 YAML
    /// </summary>
    /// <param name="out">发射器</param>
    /// <param name="entity">实体</param>
    static void SerializeEntity(YAML::Emitter& out, Entity entity)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

        ComponentRegistry::ForEach([&](const ComponentDescriptor& desc)
        {
            if (desc.Serialize)
            {
                desc.Serialize(out, entity);
            }
        });

        out << YAML::EndMap;
    }

    void SceneSerializer::Serialize(const Ref<Scene>& scene, const std::string& filepath)
    {
        YAML::Emitter out;

        out << YAML::BeginMap;
        
        out << YAML::Key << "Scene" << YAML::Value << scene->GetName();
        out << YAML::Key << "Handle" << YAML::Value << static_cast<uint64_t>(scene->GetHandle());
        
        // ---- 环境设置 ----
        {
            const EnvironmentSettings& env = scene->GetEnvironmentSettings();
            out << YAML::Key << "EnvironmentSettings" << YAML::Value;
            out << YAML::BeginMap;
            
            out << YAML::Key << "SkyboxMaterial" << YAML::Value;
            if (env.SkyboxMaterial && env.SkyboxMaterial->GetHandle().IsValid())
            {
                out << static_cast<uint64_t>(env.SkyboxMaterial->GetHandle());
            }
            else
            {
                out << static_cast<uint64_t>(0);
            }
            
            out << YAML::Key << "AmbientSource" << YAML::Value << static_cast<int>(env.Source);
            out << YAML::Key << "AmbientColor" << YAML::Value << env.AmbientColor;
            out << YAML::Key << "DiffuseIntensity" << YAML::Value << env.DiffuseIntensity;
            out << YAML::Key << "SpecularIntensity" << YAML::Value << env.SpecularIntensity;
            out << YAML::Key << "ReflectionResolution" << YAML::Value << env.ReflectionResolution;
            out << YAML::EndMap;
        }
        
        // ---- 根节点顺序列表（Hierarchy 拖拽排序依据） ----
        out << YAML::Key << "RootEntityOrder" << YAML::Value << YAML::Flow << YAML::BeginSeq;
        for (UUID rootID : scene->m_RootEntityOrder)
        {
            out << static_cast<uint64_t>(rootID);
        }
        out << YAML::EndSeq;

        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

        scene->m_Registry.each([&](auto entityID)
        {
            Entity entity = { entityID, scene.get() };
            if (!entity)
            {
                return;
            }

            SerializeEntity(out, entity);
        });

        out << YAML::EndSeq;
        
        out << YAML::EndMap;

        std::ofstream fout(filepath);
        fout << out.c_str();
    }

    void SceneSerializer::SerializeRuntime(const Ref<Scene>& scene, const std::string& filepath)
    {
        LF_CORE_ASSERT(false, "Not implemented!");
    }

    bool SceneSerializer::Deserialize(const Ref<Scene>& scene, const std::string& filepath)
    {
        YAML::Node data = YAML::LoadFile(filepath);

        if (!data["Scene"])
        {
            return false;
        }

        std::string sceneName = data["Scene"].as<std::string>();
        scene->SetName(sceneName);

        LF_CORE_TRACE("Deserializing scene '{0}'", sceneName);

        // ---- 反序列化环境设置 ----
        YAML::Node envNode = data["EnvironmentSettings"];
        if (envNode)
        {
            EnvironmentSettings& env = scene->GetEnvironmentSettings();
            
            YAML::Node skyboxMatNode = envNode["SkyboxMaterial"];
            if (skyboxMatNode && !skyboxMatNode.IsNull())
            {
                uint64_t skyboxHandleValue = skyboxMatNode.as<uint64_t>();
                AssetHandle skyboxHandle(skyboxHandleValue);
                if (skyboxHandle.IsValid())
                {
                    Ref<Material> skyboxMat = AssetManager::GetAsset<Material>(skyboxHandle);
                    if (skyboxMat)
                    {
                        env.SkyboxMaterial = skyboxMat;
                    }
                    else
                    {
                        LF_CORE_WARN("SceneSerializer: Failed to load skybox material asset [{0}]", skyboxHandleValue);
                    }
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

        YAML::Node entities = data["Entities"];
        
        if (entities)
        {
            for (auto entity : entities)
            {
                uint64_t uuid = entity["Entity"].as<uint64_t>();
                
                std::string entityName;
                YAML::Node nameComponentNode = entity["NameComponent"];
                if (nameComponentNode)
                {
                    entityName = nameComponentNode["Name"].as<std::string>();
                }

                LF_CORE_TRACE("Deserialized Entity: [UUID = {0}, Name = {1}]", uuid, entityName);

                Entity deserializedEntity = scene->CreateEntity(uuid, entityName);

                ComponentRegistry::ForEach([&](const ComponentDescriptor& desc)
                {
                    if (desc.Deserialize)
                    {
                        desc.Deserialize(deserializedEntity, entity);
                    }
                });
            }
        }

        // ---- 根节点顺序列表 ----
        // 注意：CreateEntity(uuid, name) 已经将每个实体 push 到了 m_RootEntityOrder
        // 这里需要：
        // 1. 清空反序列化过程中产生的默认顺序（无论实体是否为根）
        // 2. 根据 RootEntityOrder 字段重新构造
        scene->m_RootEntityOrder.clear();

        YAML::Node rootOrderNode = data["RootEntityOrder"];
        if (rootOrderNode && rootOrderNode.IsSequence())
        {
            for (auto node : rootOrderNode)
            {
                UUID id = node.as<uint64_t>();
                Entity entity = scene->TryGetEntityWithUUID(id);
                if (entity && entity.GetParentUUID() == 0)
                {
                    scene->m_RootEntityOrder.push_back(id);
                }
            }
        }

        return true;
    }

    bool SceneSerializer::DeserializeRuntime(const Ref<Scene>& scene, const std::string& filepath)
    {
        LF_CORE_ASSERT(false, "Not implemented!");

        return false;
    }
}