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
        operator uint32_t() const { return static_cast<uint32_t>(m_EntityID); }

        UUID GetUUID() { return GetComponent<IDComponent>().ID; }
        const std::string& GetName() { return GetComponent<NameComponent>().Name; }

        /// <summary>
        /// 设置实体名称（所有交互式重命名入口的统一通道）
        /// 
        /// 校验规则（对齐 Unity）：
        /// - newName 为空字符串：输出警告并保留原名，返回 false
        /// - newName 与当前名一致：不做修改，返回 false（避免上层触发不必要的脏标记）
        /// - 否则：写入 NameComponent::Name，返回 true
        /// 
        /// 注意：仅做空字符串校验，不做 trim（"   " 视为合法，与 Unity 一致）
        /// 反序列化 / 引擎内部直接读写 NameComponent::Name 时不受此接口约束。
        /// </summary>
        /// <param name="newName">新的名称</param>
        /// <returns>是否真的修改了名称</returns>
        bool SetName(const std::string& newName);

        Entity GetParent() const;

        /// <summary>
        /// 设置父节点，并可指定在新父节点 Children 列表中的插入位置
        /// 保持世界位置不变
        /// 
        /// 特殊情况：
        /// - 同父 + insertIndex == -1：无变化，直接返回
        /// - 同父 + insertIndex != -1：转发到 MoveToIndex，避免"从旧父移除→加到新父末尾"的多余扰动
        /// </summary>
        /// <param name="parent">新父节点（无效 Entity 表示设为根节点）</param>
        /// <param name="insertIndex">在新父节点 Children 列表中的插入位置（-1 表示末尾）</param>
        void SetParent(Entity parent, int insertIndex = -1);

        /// <summary>
        /// 在同一父节点下移动到指定位置（不改变父子关系，不改变 Transform）
        /// 如果是根节点，则在 Scene::m_RootEntityOrder 中调整位置
        /// </summary>
        /// <param name="newIndex">新的位置索引（-1 或越界表示末尾）</param>
        void MoveToIndex(int newIndex);

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