#include "lcpch.h"
#include "Entity.h"

#include "Components/TransformComponent.h"

namespace Lucky
{
    Entity::Entity(entt::entity entityID, Scene* scene)
        : m_EntityID(entityID), m_Scene(scene)
    {

    }

    Entity Entity::GetParent() const
    {
        return m_Scene->TryGetEntityWithUUID(GetParentUUID());
    }

    void Entity::SetParent(Entity parent)
    {
        Entity currentParent = GetParent();
        if (currentParent == parent)
        {
            return;
        }

        auto& transform = GetComponent<TransformComponent>();

        // 保存当前世界矩阵（用于保持世界位置不变）
        glm::mat4 currentWorldTransform = transform.GetWorldTransform();

        // 当前节点的父节点存在 则从父节点移除当前节点
        if (currentParent)
        {
            currentParent.RemoveChild(*this);
        }
        
        // 设置父节点 UUID
        SetParentUUID(parent ? parent.GetUUID() : UUID(0));

        if (parent)
        {
            std::vector<UUID>& parentChildren = parent.GetChildren();    // 新父节点的子节点列表
            UUID id = GetUUID();
            if (std::find(parentChildren.begin(), parentChildren.end(), id) == parentChildren.end())
            {
                parentChildren.emplace_back(GetUUID());    // 将当前节点添加到新父节点的子节点列表
            }

            // 计算新的局部 Transform 以保持世界位置不变
            glm::mat4 parentWorldTransform = parent.GetComponent<TransformComponent>().GetWorldTransform();
            glm::mat4 newLocalTransform = glm::inverse(parentWorldTransform) * currentWorldTransform;
            transform.SetLocalTransform(newLocalTransform);
        }
        else
        {
            // 无父节点：世界矩阵 = 局部矩阵
            transform.SetLocalTransform(currentWorldTransform);
        }
    }

    bool Entity::RemoveChild(Entity child)
    {
        UUID childID = child.GetUUID();
        std::vector<UUID>& children = GetChildren();
        
        auto it = std::find(children.begin(), children.end(), childID);
        if (it != children.end())
        {
            children.erase(it);
            return true;
        }

        return false;
    }
}
