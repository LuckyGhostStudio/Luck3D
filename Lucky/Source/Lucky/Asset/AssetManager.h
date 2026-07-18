#pragma once

#include "Asset.h"
#include "AssetHandle.h"
#include "AssetType.h"
#include "AssetRegistry.h"

#include "Lucky/Core/Base.h"

#include <set>

namespace Lucky
{
    /// <summary>
    /// Refresh 操作的结果统计
    /// </summary>
    struct RefreshResult
    {
        uint32_t Added = 0;     // 新注册的资产数量
        uint32_t Removed = 0;   // 移除的资产数量
        uint32_t Total = 0;     // 当前 Registry 中的总资产数量
    };

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
        /// 无条件创建/覆盖，创建后资产自动放入缓存，后续 GetAsset 可直接获取
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

        // ---- 目录扫描与同步（被动同步） ----

        /// <summary>
        /// 扫描 Assets 目录并同步 Registry（增量对比）
        /// 新增文件自动注册，已删除文件自动移除并清理缓存
        /// 用于启动时对齐磁盘状态，以及运行时手动 Refresh（Ctrl+R / 面板刷新按钮）
        /// </summary>
        /// <returns>同步结果统计</returns>
        static RefreshResult Refresh();

        // ---- 编辑器内 CRUD（主动同步） ----

        /// <summary>
        /// 删除资产：从磁盘删除文件、清缓存、从 Registry 移除，并持久化 Registry
        /// 用于 Project 面板右键 Delete 或 Del 键
        /// </summary>
        /// <param name="handle">要删除的资产 Handle</param>
        /// <returns>是否成功</returns>
        static bool DeleteAsset(AssetHandle handle);

        /// <summary>
        /// 重命名/移动资产：磁盘 rename + Registry 更新路径，Handle 保持不变
        /// 用于 Project 面板 F2 重命名或拖拽移动，保证跨资产引用不断裂
        /// </summary>
        /// <param name="handle">要移动的资产 Handle</param>
        /// <param name="newFilePath">新的相对路径（相对项目根目录，如 "Assets/Materials/Metal_v2.lmat"）</param>
        /// <returns>是否成功（目标已存在、磁盘 rename 失败或 Handle 无效时返回 false）</returns>
        static bool MoveAsset(AssetHandle handle, const std::string& newFilePath);

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
        /// 递归扫描目录，收集所有可识别资产文件的相对路径（正斜杠格式）
        /// 跳过隐藏目录（以 . 开头）和不可识别扩展名的文件
        /// </summary>
        /// <param name="directory">要扫描的目录（相对项目根目录）</param>
        /// <param name="outPaths">输出：收集到的文件相对路径集合</param>
        static void ScanDirectory(const std::string& directory, std::set<std::string>& outPaths);

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
