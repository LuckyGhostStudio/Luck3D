#include "lcpch.h"
#include "AssetManager.h"

#include "Asset.h"
#include "AssetImporter.h"
#include "MaterialImporter.h"
#include "MeshImporter.h"
#include "TextureImporter.h"
#include "SceneImporter.h"

#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/Mesh.h"
#include "Lucky/Renderer/Texture.h"
#include "Lucky/Scene/Scene.h"

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
        template<> AssetType GetExpectedAssetType<Scene>() { return AssetType::Scene; }
    }

    // ---- 公有接口实现 ----

    void AssetManager::Init()
    {
        // 注册 Importers
        s_Data.Importers[AssetType::Material] = CreateScope<MaterialImporter>();
s_Data.Importers[AssetType::Mesh] = CreateScope<MeshImporter>();
        s_Data.Importers[AssetType::Texture2D] = CreateScope<TextureImporter>();
        s_Data.Importers[AssetType::Scene] = CreateScope<SceneImporter>();

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
        if (!asset)
        {
            LF_CORE_ERROR("AssetManager::CreateAsset - Asset is null!");
            return NullAssetHandle;
        }

        // filepath 应为相对路径，规范化（统一正斜杠）
        std::filesystem::path path(filepath);
        std::string normalizedPath = path.generic_string();
        std::string absolutePath = std::filesystem::absolute(path).string();
        std::string assetName = path.stem().string();
        
        AssetType assetType = asset->GetAssetType();
        
        // 1. 注册到 Registry（如果已注册则返回已有 Handle）
        AssetHandle assetHandle = ImportAsset(normalizedPath, assetType);
        if (!assetHandle.IsValid())
        {
            LF_CORE_ERROR("AssetManager::CreateAsset - Failed to register asset: '{0}'", normalizedPath);
            return NullAssetHandle;
        }
        
        asset->SetHandle(assetHandle);
        
        // 2. 通过 Importer 序列化到文件
        if (!SaveAssetToFile(asset, absolutePath))
        {
            LF_CORE_ERROR("AssetManager::CreateAsset - Failed to save asset file: '{0}'", absolutePath);
            return NullAssetHandle;
        }
        
        // 3. 放入缓存（避免后续 GetAsset 时重复加载）
        s_Data.Cache[assetHandle] = std::static_pointer_cast<void>(asset);
        
        // 4. 保存 Registry
        SaveRegistry();
        
        LF_CORE_INFO("AssetManager::CreateAsset - Created '{0}' ({1}) at '{2}'", assetName, AssetTypeToString(assetType), normalizedPath);
        
        return assetHandle;
    }

    AssetHandle AssetManager::EnsureAsset(const Ref<Asset>& asset, const std::string& filepath)
    {
        if (!asset)
        {
            LF_CORE_ERROR("AssetManager::EnsureAsset - Asset is null!");
            return NullAssetHandle;
        }

        // filepath 应为相对路径，规范化（统一正斜杠）
        std::filesystem::path path(filepath);
        std::string normalizedPath = path.generic_string();
        std::string absolutePath = std::filesystem::absolute(path).string();

        AssetType assetType = asset->GetAssetType();

        // 如果文件已存在，仅注册并缓存，不重新写入文件
        if (std::filesystem::exists(absolutePath))
        {
            AssetHandle handle = ImportAsset(normalizedPath, assetType);
            if (!handle.IsValid())
            {
                LF_CORE_ERROR("AssetManager::EnsureAsset - Failed to register existing asset: '{0}'", normalizedPath);
                return NullAssetHandle;
            }

            asset->SetHandle(handle);
            s_Data.Cache[handle] = std::static_pointer_cast<void>(asset);
            return handle;
        }

        // 文件不存在，走正常创建流程
        return CreateAsset(asset, filepath);
    }

    bool AssetManager::SaveAssetToFile(const Ref<Asset>& asset, const std::string& absolutePath)
    {
        AssetType type = asset->GetAssetType();

        auto it = s_Data.Importers.find(type);
        if (it == s_Data.Importers.end())
        {
            // 没有注册 Importer 的类型（如 Texture2D、Shader），不需要序列化
            LF_CORE_WARN("AssetManager::SaveAssetToFile - No importer registered for type: {0}, skipping save.", AssetTypeToString(type));
            return true;
        }

        return it->second->Save(asset, absolutePath);
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

    AssetHandle AssetManager::GetAssetHandle(const std::string& filepath)
    {
        return s_Data.Registry.GetHandle(filepath);
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
    template Ref<Scene> AssetManager::GetAsset<Scene>(AssetHandle handle);

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
