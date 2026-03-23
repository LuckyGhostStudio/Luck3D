#include "lcpch.h"
#include "Scene.h"

#include "Lucky/Renderer/Renderer3D.h"

// 组件
#include "Components/IDComponent.h"
#include "Components/NameComponent.h"
#include "Components/TransformComponent.h"
#include "Components/MeshFilterComponent.h"

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
        return CreateEntity(UUID(), name);
    }

    Entity Scene::CreateEntity(UUID uuid, const std::string& name)
    {
        Entity entity = { m_Registry.create(), this };  // 创建实体

        entity.AddComponent<IDComponent>(uuid);     // 添加 ID 组件（默认组件）
        entity.AddComponent<NameComponent>(name);   // 添加 Name 组件（默认组件）
        entity.AddComponent<TransformComponent>();  // 添加 Transform 组件（默认组件）

        m_EnttMap[uuid] = entity;   // 添加到 EnttMap

        LF_TRACE("Created Entity：[ENTT = {0}, UUID {1}, Name {2}]", (uint32_t)entity, uuid, name);

        return entity;
    }

    void Scene::DeleteEntity(Entity entity)
    {
        LF_TRACE("Removed Entity：[ENTT = {0}, UUID {1}, Name {2}]", (uint32_t)entity, entity.GetUUID(), entity.GetName());

        m_Registry.destroy(entity);
        m_EnttMap.erase(entity.GetUUID());  // 从 EnttMap 移除
    }
    
    void Scene::OnUpdate(DeltaTime dt, EditorCamera& camera)
    {
        Renderer3D::BeginScene(camera);
        {
            // TODO 添加 MeshRenderer
            auto meshGroup = m_Registry.group<TransformComponent>(entt::get<MeshFilterComponent>);

            for (auto entity : meshGroup)
            {
                auto [transform, meshFilter] = meshGroup.get<TransformComponent, MeshFilterComponent>(entity);

                Renderer3D::DrawMesh(transform.GetTransform(), meshFilter.Mesh);
            }
        }
        Renderer3D::EndScene();
    }

    void Scene::OnViewportResize(uint32_t width, uint32_t height)
    {
        
    }

    Entity Scene::GetEntityByUUID(UUID uuid)
    {
        // TODO Assert
        if (m_EnttMap.find(uuid) != m_EnttMap.end())
        {
            return { m_EnttMap.at(uuid), this };
        }

        return {};
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

    // TODO 添加新组件
}