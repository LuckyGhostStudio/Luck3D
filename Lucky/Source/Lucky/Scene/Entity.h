#pragma once

#include "Components/IDComponent.h"
#include "Components/NameComponent.h"
#include "Components/RelationshipComponent.h"

#include "Scene.h"

#include "entt.hpp"

namespace Lucky
{
    /// <summary>
    /// 实体
    /// </summary>
    class Entity
    {
    public:
        Entity() {}
        Entity(entt::entity entityID, Scene* scene);
        Entity(const Entity& other) = default;

        /// <summary>
        /// 添加 T 类型组件
        /// </summary>
        /// <typeparam name="T">组件类型</typeparam>
        /// <typeparam name="Args">组件参数类型</typeparam>
        /// <param name="args">组件参数列表</param>
        /// <returns>组件</returns>
        template<typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            LF_CORE_ASSERT(!HasComponent<T>(), "Entity already has component!");    // 该组件已存在

            T& component = m_Scene->m_Registry.emplace<T>(m_EntityID, std::forward<Args>(args)...); // 向 m_Scene 场景的实体注册表添加 T 类型组件
            m_Scene->OnComponentAdded<T>(*this, component); // m_Scene 向 this 物体添加 T 组件时调用

            return component;
        }

        /// <summary>
        /// 添加或替换 T 类型组件
        /// </summary>
        /// <typeparam name="T">组件类型</typeparam>
        /// <typeparam name="Args">组件参数类型</typeparam>
        /// <param name="args">组件参数列表</param>
        /// <returns></returns>
        template<typename T, typename... Args>
        T& AddOrReplaceComponent(Args&&... args)
        {
            T& component = m_Scene->m_Registry.emplace_or_replace<T>(m_EntityID, std::forward<Args>(args)...);
            m_Scene->OnComponentAdded<T>(*this, component);
            return component;
        }

        /// <summary>
        /// 返回 T 类型组件
        /// </summary>
        /// <typeparam name="T">组件类型</typeparam>
        /// <returns>组件</returns>
        template<typename T>
        T& GetComponent()
        {
            LF_CORE_ASSERT(HasComponent<T>(), "Entity dose not have component!");   // 该组件不存在

            return m_Scene->m_Registry.get<T>(m_EntityID);  // 从 m_Scene 场景的实体注册表获得 T 类型组件
        }
        
        template<typename T>
        const T& GetComponent() const
        {
            LF_CORE_ASSERT(HasComponent<T>(), "Entity dose not have component!");   // 该组件不存在

            return m_Scene->m_Registry.get<T>(m_EntityID);  // 从 m_Scene 场景的实体注册表获得 T 类型组件
        }

        /// <summary>
        /// 查询是否拥有 T 类型组件
        /// </summary>
        /// <typeparam name="T">组件类型</typeparam>
        /// <returns>查询结果</returns>
        template<typename T>
        bool HasComponent()
        {
            return m_Scene->m_Registry.has<T>(m_EntityID);  // 查找 m_Scene 场景中 m_EntityID 的 T 类型组件
        }
        
        template<typename T>
        bool HasComponent() const
        {
            return m_Scene->m_Registry.has<T>(m_EntityID);  // 查找 m_Scene 场景中 m_EntityID 的 T 类型组件
        }

        /// <summary>
        /// 移除 T 类型组件
        /// </summary>
        /// <typeparam name="T">组件类型</typeparam>
        template<typename T>
        void RemoveComponent()
        {
            LF_CORE_ASSERT(HasComponent<T>(), "Entity dose not have component!");   // 该组件不存在

            m_Scene->m_Registry.remove<T>(m_EntityID);  // 移除 m_Scene 场景中 m_EntityID 的 T 类型组件
        }

        operator bool() const { return m_EntityID != entt::null; }
        operator entt::entity() const { return m_EntityID; }
        operator uint32_t() const { return (uint32_t)m_EntityID; }

        UUID GetUUID() { return GetComponent<IDComponent>().ID; }
        const std::string& GetName() { return GetComponent<NameComponent>().Name; }
        
        Entity GetParent() const;
        void SetParent(Entity parent);

        void SetParentUUID(UUID parent) { GetComponent<RelationshipComponent>().Parent = parent; }
        UUID GetParentUUID() const { return GetComponent<RelationshipComponent>().Parent; }

        std::vector<UUID>& GetChildren() { return GetComponent<RelationshipComponent>().Children; }
        const std::vector<UUID>& GetChildren() const { return GetComponent<RelationshipComponent>().Children; }

        bool RemoveChild(Entity child);
        
        bool operator==(const Entity& other)
        {
            return m_EntityID == other.m_EntityID && m_Scene == other.m_Scene;  // 物体 id 相同 && 所属场景相同
        }

        bool operator!=(const Entity& other)
        {
            return !(*this == other);
        }
    private:
        entt::entity m_EntityID{ entt::null };  // 实体 ID
        Scene* m_Scene = nullptr;               // 实体所属场景
    };
}