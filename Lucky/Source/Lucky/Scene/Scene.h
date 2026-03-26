#pragma once

#include "entt.hpp"

#include "Lucky/Core/DeltaTime.h"
#include "Lucky/Core/UUID.h"
#include "Lucky/Renderer/EditorCamera.h"

namespace Lucky
{
    class Entity;

    /// <summary>
    /// 场景
    /// </summary>
    class Scene
    {
    public:
        Scene(const std::string& name = "New Scene");
        ~Scene();

        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }

        bool IsRunning() const { return m_IsRunning; }
        
        /// <summary>
        /// 创建实体
        /// </summary>
        /// <param name="name">实体名</param>
        /// <returns>实体</returns>
        Entity CreateEntity(const std::string& name = "Entity");
        Entity CreateEntity(UUID uuid, const std::string& name = "Entity");

        /// <summary>
        /// 销毁实体
        /// </summary>
        /// <param name="entity">实体</param>
        void DestroyEntity(Entity entity);

        /// <summary>
        /// 更新：每帧调用
        /// </summary>
        /// <param name="dt">帧间隔</param>
        /// <param name="camera">编辑器相机</param>
        void OnUpdate(DeltaTime dt, EditorCamera& camera);
        
        /// <summary>
        /// 重置视口大小：视口改变时调用
        /// </summary>
        /// <param name="width">宽</param>
        /// <param name="height">高</param>
        void OnViewportResize(uint32_t width, uint32_t height);

        /// <summary>
        /// 获取 Entity
        /// </summary>
        /// <param name="uuid">UUID</param>
        /// <returns></returns>
        Entity GetEntityByUUID(UUID uuid);

        /// <summary>
        /// 返回具有 TComponents 类型组件的所有 Entt
        /// </summary>
        /// <typeparam name="...TComponents">组件类型列表</typeparam>
        /// <returns>Entts</returns>
        template<typename... TComponents>
        auto GetAllEntitiesWith()
        {
            return m_Registry.view<TComponents...>();
        }
    private:
        /// <summary>
        /// entity 添加 TComponent 组件时调用
        /// </summary>
        /// <typeparam name="TComponent">组件类型</typeparam>
        /// <param name="entity">实体</param>
        /// <param name="component">组件</param>
        template<typename TComponent>
        void OnComponentAdded(Entity entity, TComponent& component);
    private:
        friend class Entity;                // 友元类 Entity
        friend class SceneHierarchyPanel;   // 友元类 SceneHierarchyPanel
        friend class SceneSerializer;       // 友元类 SceneSerializer

        std::unordered_map<UUID, entt::entity> m_EnttMap;   // UUID - entt 映射表

        entt::registry m_Registry;          // 实体集合：实体 id 集合（unsigned int 集合）
        std::string m_Name;                 // 场景名

        uint32_t m_ViewportWidth = 1280;    // 场景视口宽
        uint32_t m_ViewportHeight = 720;    // 场景视口高

        bool m_IsRunning = false;
    };
}