#include "lcpch.h"
#include "AssetInspectorRegistry.h"

#include "Lucky/Asset/AssetManager.h"

#include <imgui.h>

namespace Lucky
{
    // 单例存储（静态局部保证初始化顺序安全）
    static std::unordered_map<AssetType, AssetInspectorRegistry::DrawFn>& GetRegistry()
    {
        static std::unordered_map<AssetType, AssetInspectorRegistry::DrawFn> s_Registry;
        return s_Registry;
    }

    void AssetInspectorRegistry::Register(AssetType type, DrawFn fn)
    {
        GetRegistry()[type] = std::move(fn);
    }

    void AssetInspectorRegistry::Unregister(AssetType type)
    {
        GetRegistry().erase(type);
    }

    void AssetInspectorRegistry::Draw(AssetHandle handle)
    {
        if (!handle.IsValid())
        {
            return;
        }

        AssetType type = AssetManager::GetAssetType(handle);
        if (type == AssetType::None)
        {
            ImGui::TextDisabled("Asset not registered (handle=%llu).", static_cast<uint64_t>(handle));
            return;
        }

        auto& registry = GetRegistry();
        auto it = registry.find(type);
        if (it == registry.end())
        {
            // 未注册对应类型的 Inspector：给出占位提示（例如 Shader 类型暂无 Inspector）
            ImGui::TextDisabled("No inspector registered for AssetType '%s'.", AssetTypeToString(type));
            return;
        }

        it->second(handle);
    }

    void AssetInspectorRegistry::Clear()
    {
        GetRegistry().clear();
    }
}
