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
#include "Lucky/Renderer/MeshFactory.h"
#include "Lucky/Renderer/Texture.h"
#include "Lucky/Serialization/MeshSerializer.h"
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
        std::string AssetsDirectory = "Assets";                         // Assets 根目录（相对项目根）
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

        // 初始化内置图元 Mesh 资产
        // 必须先于 Refresh：
        //   若 Builtin/*.lmesh 被外部误删，此步会重生成，避免下一步 Refresh 因"文件缺失"而 Unregister
        //   首次运行时 InitBuiltinMeshAssets 已通过 ImportAsset 注册，Refresh 遇到同路径会命中已注册分支
        InitBuiltinMeshAssets();

        // 启动时自动同步 Registry 与磁盘（外部新增/删除的资产在此处被反映）
        RefreshResult result = Refresh();

        LF_CORE_INFO("AssetManager initialized. Registry: {0} assets ({1} added, {2} removed), Importers: {3} registered.", result.Total, result.Added, result.Removed, s_Data.Importers.size());
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

    RefreshResult AssetManager::Refresh()
    {
        RefreshResult result;

        // 1. 扫描磁盘文件（相对路径，正斜杠格式）
        std::set<std::string> diskPaths;
        ScanDirectory(s_Data.AssetsDirectory, diskPaths);

        // 2. 收集 Registry 中所有路径
        std::set<std::string> registryPaths;
        const auto& allMetadata = s_Data.Registry.GetAllMetadata();
        for (const auto& [handle, metadata] : allMetadata)
        {
            registryPaths.insert(metadata.FilePath);
        }

        // 3. 新增：磁盘有 Registry 无 → 直接 Register（避免 ImportAsset 内部的重复查找）
        for (const std::string& path : diskPaths)
        {
            if (registryPaths.find(path) != registryPaths.end())
            {
                continue;
            }

            std::filesystem::path fsPath(path);
            std::string extension = fsPath.extension().string();
            AssetType type = GetAssetTypeFromExtension(extension);
            if (type == AssetType::None)
            {
                continue;
            }

            AssetMetadata metadata;
            metadata.Type = type;
            metadata.FilePath = path;
            s_Data.Registry.Register(metadata);
            result.Added++;
        }

        // 4. 已删除：Registry 有 磁盘无 → Unregister 并清缓存
        std::vector<AssetHandle> toRemove;
        for (const auto& [handle, metadata] : allMetadata)
        {
            if (diskPaths.find(metadata.FilePath) == diskPaths.end())
            {
                toRemove.push_back(handle);
            }
        }

        for (AssetHandle handle : toRemove)
        {
            auto cacheIt = s_Data.Cache.find(handle);
            if (cacheIt != s_Data.Cache.end())
            {
                LF_CORE_WARN("AssetManager::Refresh - Removing cached asset [{0}] (file deleted)", static_cast<uint64_t>(handle));
                s_Data.Cache.erase(cacheIt);
            }
            s_Data.Registry.Unregister(handle);
            result.Removed++;
        }

        // 5. 有变化则持久化
        if (result.Added > 0 || result.Removed > 0)
        {
            SaveRegistry();
        }

        result.Total = static_cast<uint32_t>(s_Data.Registry.GetAssetCount());
        return result;
    }

    void AssetManager::ScanDirectory(const std::string& directory, std::set<std::string>& outPaths)
    {
        std::filesystem::path dirPath(directory);
        if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath))
        {
            LF_CORE_WARN("AssetManager::ScanDirectory - Directory not found: '{0}'", directory);
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            // 相对项目根目录（cwd）的相对路径，正斜杠格式
            std::filesystem::path relativePath = std::filesystem::relative(entry.path());

            // 跳过隐藏文件/目录（任一路径段以 . 开头）
            bool isHidden = false;
            for (const auto& part : relativePath)
            {
                std::string partStr = part.string();
                if (!partStr.empty() && partStr[0] == '.')
                {
                    isHidden = true;
                    break;
                }
            }
            if (isHidden)
            {
                continue;
            }

            // 仅收集可识别扩展名的文件
            std::string extension = entry.path().extension().string();
            AssetType type = GetAssetTypeFromExtension(extension);
            if (type == AssetType::None)
            {
                continue;
            }

            outPaths.insert(relativePath.generic_string());
        }
    }

    bool AssetManager::DeleteAsset(AssetHandle handle)
    {
        const AssetMetadata* metadata = s_Data.Registry.GetMetadata(handle);
        if (!metadata)
        {
            LF_CORE_ERROR("AssetManager::DeleteAsset - Handle not found: {0}", static_cast<uint64_t>(handle));
            return false;
        }

        // 拷贝一份路径，避免 Unregister 后 metadata 指针失效
        std::string relativePath = metadata->FilePath;
        std::string absolutePath = std::filesystem::absolute(relativePath).string();

        // 1. 删磁盘文件（文件已被外部删掉时 remove 不视为错误，仅当出错才失败）
        std::error_code ec;
        std::filesystem::remove(absolutePath, ec);
        if (ec)
        {
            LF_CORE_ERROR("AssetManager::DeleteAsset - Failed to remove file '{0}': {1}", absolutePath, ec.message());
            return false;
        }

        // 2. 清缓存
        s_Data.Cache.erase(handle);

        // 3. 从 Registry 移除
        s_Data.Registry.Unregister(handle);

        // 4. 持久化
        SaveRegistry();

        LF_CORE_INFO("AssetManager::DeleteAsset - Removed '{0}'", relativePath);
        return true;
    }

    bool AssetManager::MoveAsset(AssetHandle handle, const std::string& newFilePath)
    {
        const AssetMetadata* metadata = s_Data.Registry.GetMetadata(handle);
        if (!metadata)
        {
            LF_CORE_ERROR("AssetManager::MoveAsset - Handle not found: {0}", static_cast<uint64_t>(handle));
            return false;
        }

        std::filesystem::path newPath(newFilePath);
        std::string normalizedNewPath = newPath.generic_string();

        // 同路径直接成功
        if (metadata->FilePath == normalizedNewPath)
        {
            return true;
        }

        std::string oldRelative = metadata->FilePath;
        std::string oldAbs = std::filesystem::absolute(oldRelative).string();
        std::string newAbs = std::filesystem::absolute(normalizedNewPath).string();

        // 目标路径已存在则拒绝（避免覆盖）
        if (std::filesystem::exists(newAbs))
        {
            LF_CORE_ERROR("AssetManager::MoveAsset - Target path already exists: '{0}'", normalizedNewPath);
            return false;
        }

        // 1. 确保目标目录存在
        std::error_code ec;
        std::filesystem::create_directories(newPath.parent_path(), ec);

        // 2. rename 磁盘文件
        std::filesystem::rename(oldAbs, newAbs, ec);
        if (ec)
        {
            LF_CORE_ERROR("AssetManager::MoveAsset - Failed to rename '{0}' -> '{1}': {2}", oldAbs, newAbs, ec.message());
            return false;
        }

        // 3. 更新 Registry（Handle 不变）
        if (!s_Data.Registry.UpdatePath(handle, normalizedNewPath))
        {
            // 回滚磁盘
            std::filesystem::rename(newAbs, oldAbs, ec);
            LF_CORE_ERROR("AssetManager::MoveAsset - Registry update failed, rolled back");
            return false;
        }

        // 4. 持久化
        SaveRegistry();

        LF_CORE_INFO("AssetManager::MoveAsset - '{0}' -> '{1}' (handle {2} preserved)", oldRelative, normalizedNewPath, static_cast<uint64_t>(handle));
        return true;
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

    void AssetManager::InitBuiltinMeshAssets()
    {
        const std::string builtinDir = "Assets/Meshes/Builtin/";

        struct BuiltinMeshDef
        {
            PrimitiveType Type;
            const char* Name;
        };

        BuiltinMeshDef builtins[] = {
            { PrimitiveType::Cube, "Cube" },
            { PrimitiveType::Sphere, "Sphere" },
            { PrimitiveType::Plane, "Plane" },
            { PrimitiveType::Cylinder, "Cylinder" },
            { PrimitiveType::Capsule, "Capsule" },
        };

        for (const auto& def : builtins)
        {
            std::string filepath = builtinDir + def.Name + ".lmesh";
            std::string absolutePath = std::filesystem::absolute(filepath).string();

            // 如果文件不存在则生成
            if (!std::filesystem::exists(absolutePath))
            {
                Ref<Mesh> mesh = MeshFactory::CreatePrimitive(def.Type);
                mesh->SetName(def.Name);
                MeshSerializer::Serialize(mesh, absolutePath);
                LF_CORE_INFO("AssetManager: Generated builtin mesh '{0}'", def.Name);
            }

            // 注册到资产系统
            std::filesystem::path path(filepath);
            std::string normalizedPath = path.generic_string();
            ImportAsset(normalizedPath, AssetType::Mesh);
        }
    }
}
