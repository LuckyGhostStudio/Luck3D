#include "lcpch.h"
#include "ComponentContextMenuRegistry.h"

#include "Lucky/Scene/Entity.h"

namespace Lucky
{
    static std::vector<ComponentContextMenuItem>& GetItems()
    {
        static std::vector<ComponentContextMenuItem> s_Items;
        return s_Items;
    }

    static std::vector<std::function<void()>>& GetDeferredActions()
    {
        static std::vector<std::function<void()>> s_Actions;
        return s_Actions;
    }

    void ComponentContextMenuRegistry::RegisterBuiltins()
    {
        // ---- Remove Component ----
        // 复用 desc.CanRemove + desc.Remove 作为可见性判定与执行入口
        // Execute 中通过 EnqueueDeferredAction 延后到帧末，避免当前帧后续 Draw 访问已删除的组件
        ComponentContextMenuItem removeItem;
        removeItem.Label = "Remove Component";
        removeItem.IsVisible = [](Entity, const ComponentDescriptor& desc)
        {
            return desc.CanRemove && desc.Remove != nullptr;
        };
        removeItem.Execute = [](Entity entity, const ComponentDescriptor& desc)
        {
            EnqueueDeferredAction([entity, remove = desc.Remove]()
            {
                remove(entity);
            });
        };
        Register(std::move(removeItem));
    }

    void ComponentContextMenuRegistry::Clear()
    {
        GetItems().clear();
        GetDeferredActions().clear();
    }

    void ComponentContextMenuRegistry::Register(ComponentContextMenuItem item)
    {
        GetItems().push_back(std::move(item));
    }

    const std::vector<ComponentContextMenuItem>& ComponentContextMenuRegistry::All()
    {
        return GetItems();
    }

    void ComponentContextMenuRegistry::EnqueueDeferredAction(std::function<void()> action)
    {
        if (action)
        {
            GetDeferredActions().push_back(std::move(action));
        }
    }

    void ComponentContextMenuRegistry::FlushDeferredActions()
    {
        std::vector<std::function<void()>>& actions = GetDeferredActions();
        if (actions.empty())
        {
            return;
        }

        // 先 swap 到局部再执行，避免 action 内部再次 Enqueue 导致迭代器失效
        std::vector<std::function<void()>> local;
        local.swap(actions);
        for (const std::function<void()>& action : local)
        {
            action();
        }
    }
}
