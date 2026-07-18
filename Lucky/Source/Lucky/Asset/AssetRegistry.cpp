#include "lcpch.h"
#include "AssetRegistry.h"

#include "Lucky/Serialization/YamlHelpers.h"

#include <fstream>
#include <yaml-cpp/yaml.h>
#include <filesystem>

namespace Lucky
{
    AssetHandle AssetRegistry::Register(AssetMetadata metadata)
    {
        // 检查是否已注册（通过路径）
        AssetHandle existingHandle = GetHandle(metadata.FilePath);
        if (existingHandle.IsValid())
        {
            LF_CORE_WARN("AssetRegistry: Asset already registered at path '{0}', returning existing handle.", metadata.FilePath);
            return existingHandle;
        }

        // 生成新 Handle
        metadata.Handle = AssetHandle::Generate();

        // 注册
        m_Registry[metadata.Handle] = metadata;
        m_PathToHandle[metadata.FilePath] = metadata.Handle;

        LF_CORE_INFO("AssetRegistry: Registered asset [{0}] type={1} path='{2}'",
                     static_cast<uint64_t>(metadata.Handle),
                     AssetTypeToString(metadata.Type),
                     metadata.FilePath);

        return metadata.Handle;
    }

    void AssetRegistry::RegisterWithHandle(const AssetMetadata& metadata)
    {
        if (!metadata.Handle.IsValid())
        {
            LF_CORE_ERROR("AssetRegistry::RegisterWithHandle - Invalid handle!");
            return;
        }

        m_Registry[metadata.Handle] = metadata;
        m_PathToHandle[metadata.FilePath] = metadata.Handle;
    }

    void AssetRegistry::Unregister(AssetHandle handle)
    {
        auto it = m_Registry.find(handle);
        if (it != m_Registry.end())
        {
            m_PathToHandle.erase(it->second.FilePath);
            m_Registry.erase(it);
            LF_CORE_INFO("AssetRegistry: Unregistered asset [{0}]", static_cast<uint64_t>(handle));
        }
    }

    bool AssetRegistry::UpdatePath(AssetHandle handle, const std::string& newFilePath)
    {
        auto it = m_Registry.find(handle);
        if (it == m_Registry.end())
        {
            LF_CORE_ERROR("AssetRegistry::UpdatePath - Handle not found: {0}", static_cast<uint64_t>(handle));
            return false;
        }

        // 新路径必须唯一（若已被其他 Handle 占用则拒绝）
        auto pathIt = m_PathToHandle.find(newFilePath);
        if (pathIt != m_PathToHandle.end() && pathIt->second != handle)
        {
            LF_CORE_ERROR("AssetRegistry::UpdatePath - Target path '{0}' already used by another handle {1}", newFilePath, static_cast<uint64_t>(pathIt->second));
            return false;
        }

        // 同路径视为成功（无操作）
        if (it->second.FilePath == newFilePath)
        {
            return true;
        }

        // 同步反向索引
        m_PathToHandle.erase(it->second.FilePath);
        it->second.FilePath = newFilePath;
        m_PathToHandle[newFilePath] = handle;

        LF_CORE_INFO("AssetRegistry: UpdatePath [{0}] -> '{1}'", static_cast<uint64_t>(handle), newFilePath);
        return true;
    }

    const AssetMetadata* AssetRegistry::GetMetadata(AssetHandle handle) const
    {
        auto it = m_Registry.find(handle);
        if (it != m_Registry.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    AssetMetadata* AssetRegistry::GetMetadata(AssetHandle handle)
    {
        auto it = m_Registry.find(handle);
        if (it != m_Registry.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    AssetHandle AssetRegistry::GetHandle(const std::string& filepath) const
    {
        auto it = m_PathToHandle.find(filepath);
        if (it != m_PathToHandle.end())
        {
            return it->second;
        }
        return NullAssetHandle;
    }

    bool AssetRegistry::Contains(AssetHandle handle) const
    {
        return m_Registry.find(handle) != m_Registry.end();
    }

    bool AssetRegistry::ContainsPath(const std::string& filepath) const
    {
        return m_PathToHandle.find(filepath) != m_PathToHandle.end();
    }

    void AssetRegistry::Save(const std::string& filepath) const
    {
        YAML::Emitter out;

        out << YAML::BeginMap;
        out << YAML::Key << "Assets" << YAML::Value << YAML::BeginSeq;

        for (const auto& [handle, metadata] : m_Registry)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Handle" << YAML::Value << static_cast<uint64_t>(handle);
            out << YAML::Key << "Type" << YAML::Value << AssetTypeToString(metadata.Type);
            out << YAML::Key << "FilePath" << YAML::Value << metadata.FilePath;
            out << YAML::EndMap;
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream fout(filepath);
        fout << out.c_str();

        LF_CORE_INFO("AssetRegistry: Saved {0} assets to '{1}'", m_Registry.size(), filepath);
    }

    bool AssetRegistry::Load(const std::string& filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            LF_CORE_WARN("AssetRegistry: File not found '{0}', starting with empty registry.", filepath);
            return false;
        }

        YAML::Node data = YAML::LoadFile(filepath);

        if (!data["Assets"])
        {
            LF_CORE_ERROR("AssetRegistry: Invalid registry file '{0}'", filepath);
            return false;
        }

        m_Registry.clear();
        m_PathToHandle.clear();

        YAML::Node assetsNode = data["Assets"];
        for (auto assetNode : assetsNode)
        {
            AssetMetadata metadata;
            metadata.Handle = AssetHandle(assetNode["Handle"].as<uint64_t>());
            metadata.Type = StringToAssetType(assetNode["Type"].as<std::string>());
            metadata.FilePath = assetNode["FilePath"].as<std::string>();

            if (metadata.IsValid())
            {
                m_Registry[metadata.Handle] = metadata;
                m_PathToHandle[metadata.FilePath] = metadata.Handle;
            }
        }

        LF_CORE_INFO("AssetRegistry: Loaded {0} assets from '{1}'", m_Registry.size(), filepath);
        return true;
    }
}
