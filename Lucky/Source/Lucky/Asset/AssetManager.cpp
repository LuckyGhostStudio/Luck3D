#include "lcpch.h"
#include "AssetManager.h"

#include "Asset.h"
#include "AssetImporter.h"
#include "MaterialImporter.h"
#include "MeshAssetImporter.h"
#include "TextureImporter.h"

#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/Mesh.h"
#include "Lucky/Renderer/Texture.h"

#include "Lucky/Serialization/MaterialSerializer.h"

#include <filesystem>

namespace Lucky
{
    // ---- 静态数据 ----
    struct AssetManagerData
    {
        AssetRegistry Registry;
        std::unordered_map<AssetHandle, Ref<void>> Cache;               // 强引用缓存
        std::unordered_map<AssetType, Scope<AssetImporter>> Importers;  // Importer 注册表

        std::string RegistryFilePath = "AssetRegistry.lcr";             // Registry 文件路径
    };

    static AssetManagerData s_Data;

    // ---- 类型映射辅助 ----
    namespace
    {
        template<typename T>
        AssetType GetExpectedAssetType()
        {
            static_assert(sizeof(T) == 0, "Unsupported asset type");
            return AssetType::None;
        }

        template<> AssetType GetExpectedAssetType<Material>() { return AssetType::Material; }
        template<> AssetType GetExpectedAssetType<Mesh>() { return AssetType::Mesh; }
        template<> AssetType GetExpectedAssetType<Texture2D>() { return AssetType::Texture2D; }
    }

    // ---- 公有接口实现 ----

    void AssetManager::Init()
    {
        // 注册 Importers
        s_Data.Importers[AssetType::Material] = CreateScope<MaterialImporter>();
        s_Data.Importers[AssetType::Mesh] = CreateScope<MeshAssetImporter>();
        s_Data.Importers[AssetType::Texture2D] = CreateScope<TextureImporter>();

        // 加载 Registry
        s_Data.Registry.Load(s_Data.RegistryFilePath);

        LF_CORE_INFO("AssetManager initialized. Registry: {0} assets, Importers: {1} registered.", s_Data.Registry.GetAssetCount(), s_Data.Importers.size());
    }

    void AssetManager::Shutdown()
    {
        SaveRegistry();
        s_Data.Cache.clear();
        s_Data.Importers.clear();
        LF_CORE_INFO("AssetManager shutdown.");
    }

    AssetHandle AssetManager::CreateAsset(const Ref<Asset>& asset, const std::string& filepath)
    {
        std::filesystem::path relPath = std::filesystem::relative(filepath);
        std::string assetName = relPath.stem().string();
        std::string normalizedPath = relPath.generic_string();
        
        AssetType assetType = asset->GetAssetType();
        
        // 注册到资产系统
        AssetHandle assetHandle = ImportAsset(normalizedPath, assetType);
        
        asset->SetHandle(assetHandle);
        
        // 序列化到文件
        switch (assetType)
        {
            case AssetType::Material:
            {
                const Ref<Material>& material = RefAs<Asset, Material>(asset);
                
                // 序列化材质
                if (!MaterialSerializer::SerializeToFile(material, filepath))
                {
                    LF_CORE_ERROR("Failed to create material file: '{0}'", filepath);
                    return NullAssetHandle;
                }
                    
                break;
            }
            case AssetType::Mesh:
                break;
            case AssetType::Texture2D:
                break;
            case AssetType::Scene:
                break;
            case AssetType::Shader:
                break;
        }
        
        // 保存资产注册表到文件
        SaveRegistry();
        
        LF_CORE_INFO("Created asset: '{0} ({1})' at '{2}'", assetName, AssetTypeToString(assetType), normalizedPath);
        
        return assetHandle;
    }

    AssetHandle AssetManager::ImportAsset(const std::string& filepath)
    {
        // 规范化路径（使用正斜杠）
        std::filesystem::path path(filepath);
        std::string normalizedPath = path.generic_string();

        // 推断资产类型
        std::string extension = path.extension().string();
        AssetType type = GetAssetTypeFromExtension(extension);

        if (type == AssetType::None)
        {
            LF_CORE_ERROR("AssetManager::ImportAsset - Unknown file type: '{0}'", extension);
            return NullAssetHandle;
        }

        return ImportAsset(normalizedPath, type);
    }

    AssetHandle AssetManager::ImportAsset(const std::string& filepath, AssetType type)
    {
        // 检查是否已注册
        AssetHandle existingHandle = s_Data.Registry.GetHandle(filepath);
        if (existingHandle.IsValid())
        {
            return existingHandle;
        }

        // 注册新资产
        AssetMetadata metadata;
        metadata.Type = type;
        metadata.FilePath = filepath;

        return s_Data.Registry.Register(metadata);
    }

    template<typename T>
    Ref<T> AssetManager::GetAsset(AssetHandle handle)
    {
        if (!handle.IsValid())
        {
            return nullptr;
        }

        // 类型检查
        AssetType expectedType = GetExpectedAssetType<T>();
        const AssetMetadata* metadata = s_Data.Registry.GetMetadata(handle);

        if (!metadata)
        {
            LF_CORE_ERROR("AssetManager::GetAsset - Handle not found in registry: {0}", static_cast<uint64_t>(handle));
            return nullptr;
        }

        if (metadata->Type != expectedType)
        {
            LF_CORE_ERROR("AssetManager::GetAsset - Type mismatch! Expected {0}, got {1}", AssetTypeToString(expectedType), AssetTypeToString(metadata->Type));
            return nullptr;
        }

        // 缓存命中
        auto it = s_Data.Cache.find(handle);
        if (it != s_Data.Cache.end())
        {
            return std::static_pointer_cast<T>(it->second);
        }

        // 缓存未命中，通过 Importer 加载资产
        Ref<void> asset = LoadAsset(*metadata);
        if (asset)
        {
            // 设置资产的 Handle（Asset 基类提供）
            std::static_pointer_cast<Asset>(asset)->SetHandle(handle);
            s_Data.Cache[handle] = asset;
            return std::static_pointer_cast<T>(asset);
        }

        return nullptr;
    }

    // 显式实例化模板（避免链接错误）
    template Ref<Material> AssetManager::GetAsset<Material>(AssetHandle handle);
    template Ref<Mesh> AssetManager::GetAsset<Mesh>(AssetHandle handle);
    template Ref<Texture2D> AssetManager::GetAsset<Texture2D>(AssetHandle handle);

    bool AssetManager::IsAssetLoaded(AssetHandle handle)
    {
        return s_Data.Cache.find(handle) != s_Data.Cache.end();
    }

    bool AssetManager::IsAssetRegistered(AssetHandle handle)
    {
        return s_Data.Registry.Contains(handle);
    }

    AssetType AssetManager::GetAssetType(AssetHandle handle)
    {
        const AssetMetadata* metadata = s_Data.Registry.GetMetadata(handle);
        return metadata ? metadata->Type : AssetType::None;
    }

    const std::string& AssetManager::GetAssetFilePath(AssetHandle handle)
    {
        static const std::string s_EmptyString;
        const AssetMetadata* metadata = s_Data.Registry.GetMetadata(handle);
        return metadata ? metadata->FilePath : s_EmptyString;
    }

    AssetRegistry& AssetManager::GetRegistry()
    {
        return s_Data.Registry;
    }

    void AssetManager::UnloadAsset(AssetHandle handle)
    {
        s_Data.Cache.erase(handle);
    }

    void AssetManager::ClearCache()
    {
        s_Data.Cache.clear();
        LF_CORE_INFO("AssetManager: Cache cleared.");
    }

    void AssetManager::SaveRegistry()
    {
        s_Data.Registry.Save(s_Data.RegistryFilePath);
    }

    Ref<void> AssetManager::LoadAsset(const AssetMetadata& metadata)
    {
        // 通过 Importer 注册表分发加载
        auto it = s_Data.Importers.find(metadata.Type);
        if (it == s_Data.Importers.end())
        {
            LF_CORE_ERROR("AssetManager::LoadAsset - No importer registered for type: {0}",
                          AssetTypeToString(metadata.Type));
            return nullptr;
        }

        return it->second->Load(metadata);
    }
}
