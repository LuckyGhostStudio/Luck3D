#include "lcpch.h"
#include "ComponentRegistry.h"

#include <unordered_map>

namespace Lucky
{
    static std::vector<ComponentDescriptor>& GetStorage()
    {
        static std::vector<ComponentDescriptor> s_Storage;
        return s_Storage;
    }

    static std::unordered_map<ComponentType, size_t>& GetIndexMap()
    {
        static std::unordered_map<ComponentType, size_t> s_IndexMap;
        return s_IndexMap;
    }

    void ComponentRegistry::RegisterAll()
    {
        RegisterAllCores();
        RegisterAllSerializations();
        RegisterAllInspectors();
    }

    void ComponentRegistry::RegisterCore(ComponentDescriptor desc)
    {
        LF_CORE_ASSERT(desc.Type != ComponentType::None, "ComponentRegistry::RegisterCore - Type must not be None");
        LF_CORE_ASSERT(GetIndexMap().find(desc.Type) == GetIndexMap().end(), "ComponentRegistry::RegisterCore - Duplicate registration");

        GetIndexMap()[desc.Type] = GetStorage().size();
        GetStorage().push_back(std::move(desc));
    }

    void ComponentRegistry::RegisterSerialization(ComponentType type,
                                                  std::string serializedKey,
                                                  ComponentDescriptor::SerializeFn serialize,
                                                  ComponentDescriptor::DeserializeFn deserialize)
    {
        auto it = GetIndexMap().find(type);
        LF_CORE_ASSERT(it != GetIndexMap().end(), "ComponentRegistry::RegisterSerialization - Core must be registered first");

        ComponentDescriptor& desc = GetStorage()[it->second];
        desc.SerializedKey = std::move(serializedKey);
        desc.Serialize = std::move(serialize);
        desc.Deserialize = std::move(deserialize);
    }

    void ComponentRegistry::RegisterInspector(ComponentType type,
                                              ComponentDescriptor::DrawFn draw,
                                              ComponentDescriptor::IconFn getIcon,
                                              std::vector<ComponentAddMenuItem> addMenuItems,
                                              ComponentDescriptor::RemoveFn remove,
                                              bool showInHierarchyIcons,
                                              bool canRemove,
                                              std::vector<ComponentContextMenuItem> extraContextMenuItems)
    {
        auto it = GetIndexMap().find(type);
        LF_CORE_ASSERT(it != GetIndexMap().end(), "ComponentRegistry::RegisterInspector - Core must be registered first");

        ComponentDescriptor& desc = GetStorage()[it->second];
        desc.Draw = std::move(draw);
        desc.GetIcon = std::move(getIcon);
        desc.AddMenuItems = std::move(addMenuItems);
        desc.Remove = std::move(remove);
        desc.ShowInHierarchyIcons = showInHierarchyIcons;
        desc.CanRemove = canRemove;
        desc.ExtraContextMenuItems = std::move(extraContextMenuItems);
    }

    void ComponentRegistry::Clear()
    {
        GetStorage().clear();
        GetIndexMap().clear();
    }

    const ComponentDescriptor* ComponentRegistry::GetByType(ComponentType type)
    {
        auto it = GetIndexMap().find(type);
        if (it == GetIndexMap().end())
        {
            return nullptr;
        }
        return &GetStorage()[it->second];
    }

    void ComponentRegistry::ForEach(const std::function<void(const ComponentDescriptor&)>& callback)
    {
        for (const ComponentDescriptor& desc : GetStorage())
        {
            callback(desc);
        }
    }

    const std::vector<ComponentDescriptor>& ComponentRegistry::All()
    {
        return GetStorage();
    }
}
