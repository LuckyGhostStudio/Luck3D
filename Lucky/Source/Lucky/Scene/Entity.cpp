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

    bool Entity::SetName(const std::string& newName)
    {
        // 校验：名称不能为空（对齐 Unity 表现）
        // 仅判 empty，不做 trim（"   " 视为合法，与 Unity 一致）
        if (newName.empty())
        {
            LF_CORE_WARN("A Entity name cannot be set to an empty string");
            return false;
        }

        NameComponent& nameComp = GetComponent<NameComponent>();

        // 名称无变化时也视作"未修改"，避免上层触发不必要的脏标记
        if (nameComp.Name == newName)
        {
            return false;
        }

        nameComp.Name = newName;
        return true;
    }

    void Entity::SetParent(Entity parent, int insertIndex)
    {
        Entity currentParent = GetParent();

        // 同父节点场景：
        // - insertIndex == -1  → 无变化，直接返回
        // - insertIndex != -1  → 走同级排序分支（避免"从旧父移除→加到新父末尾"的多余扰动）
        if (currentParent == parent)
        {
            if (insertIndex == -1)
            {
                return;
            }
            MoveToIndex(insertIndex);
            return;
        }

        auto& transform = GetComponent<TransformComponent>();

        // 保存当前世界矩阵（用于保持世界位置不变）
        glm::mat4 currentWorldTransform = transform.GetWorldTransform();

        // 从旧父节点移除
        if (currentParent)
        {
            currentParent.RemoveChild(*this);
        }
        else
        {
            // 原来是根节点，从根节点列表移除
            m_Scene->RemoveRootEntity(GetUUID());
        }
        
        // 设置父节点 UUID
        SetParentUUID(parent ? parent.GetUUID() : UUID(0));

        if (parent)
        {
            std::vector<UUID>& parentChildren = parent.GetChildren();    // 新父节点的子节点列表
            UUID id = GetUUID();

            // 保险去重：如果已经存在则先移除，再按 insertIndex 插入
            auto existIt = std::find(parentChildren.begin(), parentChildren.end(), id);
            if (existIt != parentChildren.end())
            {
                parentChildren.erase(existIt);
            }

            if (insertIndex < 0 || insertIndex >= static_cast<int>(parentChildren.size()))
            {
                parentChildren.push_back(id);
            }
            else
            {
                parentChildren.insert(parentChildren.begin() + insertIndex, id);
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

            // 插入到根节点列表指定位置
            m_Scene->InsertRootEntity(GetUUID(), insertIndex);
        }
    }

    void Entity::MoveToIndex(int newIndex)
    {
        UUID id = GetUUID();
        Entity parent = GetParent();

        if (parent)
        {
            std::vector<UUID>& siblings = parent.GetChildren();
            auto it = std::find(siblings.begin(), siblings.end(), id);
            if (it != siblings.end())
            {
                siblings.erase(it);
            }

            if (newIndex < 0 || newIndex >= static_cast<int>(siblings.size()))
            {
                siblings.push_back(id);
            }
            else
            {
                siblings.insert(siblings.begin() + newIndex, id);
            }
        }
        else
        {
            // 根节点排序：InsertRootEntity 内部会先去重再插入
            m_Scene->InsertRootEntity(id, newIndex);
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
