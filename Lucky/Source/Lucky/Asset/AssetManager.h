#pragma once

#include "Asset.h"
#include "AssetHandle.h"
#include "AssetType.h"
#include "AssetRegistry.h"

#include "Lucky/Core/Base.h"

namespace Lucky
{
    /// <summary>
    /// 资产管理器：资产系统的统一入口
    /// 负责资产的注册、加载、缓存和查询
    /// 采用全静态方法模式
    /// </summary>
    class AssetManager
    {
    public:
        /// <summary>
        /// 初始化资产系统（加载 Registry 文件）
        /// </summary>
        static void Init();

        /// <summary>
        /// 关闭资产系统（保存 Registry，清空缓存）
        /// </summary>
        static void Shutdown();
        
        // ---- 资产创建 ----
        
        /// <summary>
        /// 创建资产：将内存中的资产实例序列化到文件，并注册到资产系统
        /// 无条件创建/覆盖（类似 Unity AssetDatabase.CreateAsset）
        /// 创建后资产自动放入缓存，后续 GetAsset 可直接获取
        /// </summary>
        /// <param name="asset">资产引用</param>
        /// <param name="filepath">相对于项目根目录的路径（如 "Assets/Materials/Metal.lmat"）</param>
        /// <returns>资产 Handle（失败返回 NullAssetHandle）</returns>
        static AssetHandle CreateAsset(const Ref<Asset>& asset, const std::string& filepath);

        /// <summary>
        /// 确保资产存在（幂等操作）：
        /// - 如果文件已存在：仅注册到 Registry 并放入缓存，不重新写入文件
        /// - 如果文件不存在：创建资产文件并注册
        /// 适用于引擎内部默认资产等"只需创建一次"的场景
        /// </summary>
        /// <param name="asset">资产引用</param>
        /// <param name="filepath">相对于项目根目录的路径</param>
        /// <returns>资产 Handle（失败返回 NullAssetHandle）</returns>
        static AssetHandle EnsureAsset(const Ref<Asset>& asset, const std::string& filepath);

        // ---- 资产导入 ----

        /// <summary>
        /// 导入资产：将文件注册到资产系统
        /// 如果文件已注册，返回已有的 Handle
        /// 资产类型通过文件扩展名自动推断
        /// </summary>
        /// <param name="filepath">相对于项目根目录的文件路径</param>
        /// <returns>资产 Handle（失败返回 NullAssetHandle）</returns>
        static AssetHandle ImportAsset(const std::string& filepath);

        /// <summary>
        /// 导入资产（指定类型）
        /// </summary>
        /// <param name="filepath">文件路径</param>
        /// <param name="type">资产类型</param>
        /// <returns>资产 Handle</returns>
        static AssetHandle ImportAsset(const std::string& filepath, AssetType type);

        // ---- 资产获取 ----

        static AssetHandle GetAssetHandle(const std::string& filepath);
        
        /// <summary>
        /// 获取资产：通过 Handle 获取资产实例
        /// 如果未加载，自动触发加载；如果已缓存，直接返回缓存
        /// </summary>
        /// <typeparam name="T">资产类型（Material / Mesh / Texture2D）</typeparam>
        /// <param name="handle">资产 Handle</param>
        /// <returns>资产实例（失败返回 nullptr）</returns>
        template<typename T>
        static Ref<T> GetAsset(AssetHandle handle);

        // ---- 查询接口 ----

        /// <summary>
        /// 资产是否已加载到内存（在缓存中）
        /// </summary>
        static bool IsAssetLoaded(AssetHandle handle);

        /// <summary>
        /// 资产是否已注册（在 Registry 中）
        /// </summary>
        static bool IsAssetRegistered(AssetHandle handle);

        /// <summary>
        /// 获取资产类型
        /// </summary>
        static AssetType GetAssetType(AssetHandle handle);

        /// <summary>
        /// 获取资产文件路径
        /// </summary>
        static const std::string& GetAssetFilePath(AssetHandle handle);

        /// <summary>
        /// 获取 Registry 引用（供编辑器 UI 使用）
        /// </summary>
        static AssetRegistry& GetRegistry();

        // ---- 缓存管理 ----

        /// <summary>
        /// 从缓存中移除指定资产（下次获取时重新加载）
        /// </summary>
        static void UnloadAsset(AssetHandle handle);

        /// <summary>
        /// 清空所有缓存（不影响 Registry）
        /// </summary>
        static void ClearCache();

        /// <summary>
        /// 保存 Registry 到磁盘
        /// </summary>
        static void SaveRegistry();

    private:
        /// <summary>
        /// 初始化内置图元 Mesh 资产（启动时自动生成 .lmesh 文件）
        /// </summary>
        static void InitBuiltinMeshAssets();

        /// <summary>
        /// 加载资产到内存（内部方法）
        /// </summary>
        static Ref<void> LoadAsset(const AssetMetadata& metadata);

        /// <summary>
        /// 将资产序列化到磁盘文件（内部方法）
        /// 通过 Importer 注册表分发保存逻辑
        /// </summary>
        /// <param name="asset">资产实例</param>
        /// <param name="absolutePath">绝对文件路径</param>
        /// <returns>是否成功</returns>
        static bool SaveAssetToFile(const Ref<Asset>& asset, const std::string& absolutePath);
    };
}
