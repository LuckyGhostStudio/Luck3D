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

    void ComponentRegistry::Register(ComponentDescriptor desc)
    {
        LF_CORE_ASSERT(desc.Type != ComponentType::None, "ComponentRegistry::Register - Type must not be None");
        LF_CORE_ASSERT(GetIndexMap().find(desc.Type) == GetIndexMap().end(), "ComponentRegistry::Register - Duplicate registration");

        GetIndexMap()[desc.Type] = GetStorage().size();
        GetStorage().push_back(std::move(desc));
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
