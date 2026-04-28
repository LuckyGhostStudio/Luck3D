#include "lcpch.h"
#include "Scene.h"

#include "Lucky/Renderer/Renderer3D.h"

#include "Components/Components.h"

#include "Entity.h"

namespace Lucky
{
    Scene::Scene(const std::string& name)
        : m_Name(name)
    {

    }

    Scene::~Scene()
    {
        
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        return CreateEntity({}, name);
    }

    Entity Scene::CreateEntity(UUID uuid, const std::string& name)
    {
        Entity entity = { m_Registry.create(), this };  // 创建实体

        entity.AddComponent<IDComponent>(uuid);     // 添加 ID 组件（默认组件）
        entity.AddComponent<NameComponent>(name);   // 添加 Name 组件（默认组件）
        entity.AddComponent<TransformComponent>();  // 添加 Transform 组件（默认组件）
        entity.AddComponent<RelationshipComponent>();   // 添加 Relationship 组件（默认组件）

        m_EntityIDMap[uuid] = entity;   // 添加到 m_EntityIDMap

        LF_TRACE("Created Entity: [ENTT = {0}, UUID {1}, Name {2}]", static_cast<uint32_t>(entity), uuid, name);

        return entity;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if (!entity)
        {
            return;
        }

        // 拷贝子节点列表后再遍历：避免遍历过程中原始列表被修改导致跳过子节点
        std::vector<UUID> children = entity.GetChildren();
        for (UUID childID : children)
        {
            Entity child = GetEntityWithUUID(childID);
            DestroyEntity(child);
        }
        
        // 将当前节点从父节点移除
        Entity parent = entity.GetParent();
        if (parent)
        {
            parent.RemoveChild(entity);
        }
        
        UUID id = entity.GetUUID();
        
        LF_TRACE("Removed Entity: [ENTT = {0}, UUID {1}, Name {2}]", static_cast<uint32_t>(entity), id, entity.GetName());
        
        m_Registry.destroy(entity);
        m_EntityIDMap.erase(id);    // 从 m_EntityIDMap 移除
    }
    
    void Scene::OnUpdate(DeltaTime dt, EditorCamera& camera)
    {
        // 收集所有光源数据
        SceneLightData sceneLightData;
        
        // 收集方向光实体
        {
            auto lightView = m_Registry.view<TransformComponent, DirectionalLightComponent>();
            for (auto entity : lightView)
            {
                if (sceneLightData.DirectionalLightCount >= s_MaxDirectionalLights)
                {
                    break;
                }
                
                auto [transform, light] = lightView.get<TransformComponent, DirectionalLightComponent>(entity);
                
                DirectionalLightData& dirlight = sceneLightData.DirectionalLights[sceneLightData.DirectionalLightCount];
                dirlight.Direction = transform.GetForward();
                dirlight.Color = light.Color;
                dirlight.Intensity = light.Intensity;
                
                // 收集第一个方向光的阴影参数（当前仅支持单光源阴影）
                if (sceneLightData.DirectionalLightCount == 0)
                {
                    sceneLightData.DirLightShadowType = light.Shadows;
                    sceneLightData.DirLightShadowBias = light.ShadowBias;
                    sceneLightData.DirLightShadowStrength = light.ShadowStrength;
                }
                
                sceneLightData.DirectionalLightCount++;
            }
        }
        
        // 收集点光源实体
        {
            auto lightView = m_Registry.view<TransformComponent, PointLightComponent>();
            for (auto entity : lightView)
            {
                if (sceneLightData.PointLightCount >= s_MaxPointLights)
                {
                    break;
                }
                
                auto [transform, light] = lightView.get<TransformComponent, PointLightComponent>(entity);
                
                PointLightData& pointLight = sceneLightData.PointLights[sceneLightData.PointLightCount];
                pointLight.Position = transform.Translation;
                pointLight.Color = light.Color;
                pointLight.Intensity = light.Intensity;
                pointLight.Range = light.Range;
                
                sceneLightData.PointLightCount++;
            }
        }
        
        // 收集聚光灯实体
        {
            auto lightView = m_Registry.view<TransformComponent, SpotLightComponent>();
            for (auto entity : lightView)
            {
                if (sceneLightData.SpotLightCount >= s_MaxSpotLights)
                {
                    break;
                }
                
                auto [transform, light] = lightView.get<TransformComponent, SpotLightComponent>(entity);
                
                SpotLightData& spotLight = sceneLightData.SpotLights[sceneLightData.SpotLightCount];
                spotLight.Position = transform.Translation;
                spotLight.Direction = transform.GetForward();
                spotLight.Color = light.Color;
                spotLight.Intensity = light.Intensity;
                spotLight.Range = light.Range;
                spotLight.InnerCutoff = glm::cos(glm::radians(light.InnerCutoffAngle));
                spotLight.OuterCutoff = glm::cos(glm::radians(light.OuterCutoffAngle));
                
                sceneLightData.SpotLightCount++;
            }
        }
        
        Renderer3D::BeginScene(camera, sceneLightData);
        {
            // 获取同时拥有 TransformComponent MeshFilterComponent MeshRendererComponent 的实体
            auto meshGroup = m_Registry.group<TransformComponent>(entt::get<MeshFilterComponent, MeshRendererComponent>);

            for (auto entity : meshGroup)
            {
                auto [transform, meshFilter, meshRenderer] = meshGroup.get<TransformComponent, MeshFilterComponent, MeshRendererComponent>(entity);

                Renderer3D::DrawMesh(transform.GetTransform(), meshFilter.Mesh, meshRenderer.Materials, static_cast<int>(static_cast<uint32_t>(entity)));    // 绘制网格
            }
        }
        Renderer3D::EndScene();
    }

    void Scene::OnViewportResize(uint32_t width, uint32_t height)
    {
        
    }

    Entity Scene::GetEntityWithUUID(UUID id)
    {
        // Temp Debug
        if (m_EntityIDMap.find(id) == m_EntityIDMap.end())
        {
	        LF_CORE_ERROR("Invalid entity ID {0} or entity doesn't exist in scene", id);
        }
        
        LF_CORE_ASSERT(m_EntityIDMap.find(id) != m_EntityIDMap.end(), "Invalid entity ID or entity doesn't exist in scene!");
        return m_EntityIDMap.at(id);
    }

    Entity Scene::TryGetEntityWithUUID(UUID id)
    {
        if (const auto it = m_EntityIDMap.find(id); it != m_EntityIDMap.end())
        {
            return it->second;
        }
        return Entity{};
    }

    void Scene::ClearAllEntities()
    {
        for (auto& [uuid, entity] : m_EntityIDMap)
        {
            DestroyEntity(entity);
        }
    }

    template<typename TComponent>
    void Scene::OnComponentAdded(Entity entity, TComponent& component)
    {
        static_assert(sizeof(TComponent) == 0);
    }

    template<>
    void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent& component)
    {

    }
    
    template<>
    void Scene::OnComponentAdded<NameComponent>(Entity entity, NameComponent& component)
    {

    }
    
    template<>
    void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component)
    {

    }
    
    template<>
    void Scene::OnComponentAdded<MeshFilterComponent>(Entity entity, MeshFilterComponent& component)
    {

    }

    template<>
    void Scene::OnComponentAdded<RelationshipComponent>(Entity entity, RelationshipComponent& component)
    {

    }
    
    template<>
    void Scene::OnComponentAdded<MeshRendererComponent>(Entity entity, MeshRendererComponent& component)
    {
        // 根据 SubMesh 数量初始化材质列表
        if (entity.HasComponent<MeshFilterComponent>())
        {
            auto& meshFilter = entity.GetComponent<MeshFilterComponent>();
            if (meshFilter.Mesh)
            {
                // 查找最大的 MaterialIndex
                uint32_t requiredCount = 0;
                for (const SubMesh& sm : meshFilter.Mesh->GetSubMeshes())
                {
                    requiredCount = std::max(requiredCount, sm.MaterialIndex + 1);
                }

                // 不存在的材质使用默认材质
                component.Materials.resize(requiredCount);
                for (auto& mat : component.Materials)
                {
                    if (!mat)
                    {
                        mat = Renderer3D::GetDefaultMaterial();
                    }
                }
            }
        }
    }
    
    template<>
    void Scene::OnComponentAdded<DirectionalLightComponent>(Entity entity, DirectionalLightComponent& component)
    {
        
    }
    
    template<>
    void Scene::OnComponentAdded<PointLightComponent>(Entity entity, PointLightComponent& component)
    {
        
    }
    
    template<>
    void Scene::OnComponentAdded<SpotLightComponent>(Entity entity, SpotLightComponent& component)
    {
        
    }
    
    // TODO 添加新组件
}