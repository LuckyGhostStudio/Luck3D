#pragma once

#include "AssetHandle.h"
#include "AssetMetadata.h"

#include <unordered_map>
#include <string>

namespace Lucky
{
    /// <summary>
    /// 资产注册表：维护 AssetHandle 文件路径的双向映射
    /// 负责持久化到磁盘（.lreg 文件）
    /// </summary>
    class AssetRegistry
    {
    public:
        AssetRegistry() = default;

        // ---- 注册/注销 ----

        /// <summary>
        /// 注册新资产，分配 Handle 并记录元数据
        /// </summary>
        /// <param name="metadata">资产元数据（Handle 字段会被自动填充）</param>
        /// <returns>分配的 AssetHandle</returns>
        AssetHandle Register(AssetMetadata metadata);

        /// <summary>
        /// 使用指定 Handle 注册资产（用于从磁盘恢复）
        /// </summary>
        /// <param name="metadata">资产元数据（Handle 字段必须有效）</param>
        void RegisterWithHandle(const AssetMetadata& metadata);

        /// <summary>
        /// 注销资产
        /// </summary>
        /// <param name="handle">要注销的资产 Handle</param>
        void Unregister(AssetHandle handle);

        /// <summary>
        /// 更新指定 Handle 对应资产的文件路径（Handle 保持不变）
        /// 用于编辑器内重命名/移动资产，保持跨资产引用（如 Scene 中的 Mesh/Material Handle）不断裂
        /// </summary>
        /// <param name="handle">要更新的 Handle</param>
        /// <param name="newFilePath">新的相对路径（正斜杠格式）</param>
        /// <returns>是否成功（Handle 不存在或 newFilePath 已被其他 Handle 占用时失败）</returns>
        bool UpdatePath(AssetHandle handle, const std::string& newFilePath);

        // ---- 查询 ----

        /// <summary>
        /// 通过 Handle 获取元数据指针（未找到返回 nullptr）
        /// </summary>
        const AssetMetadata* GetMetadata(AssetHandle handle) const;

        /// <summary>
        /// 通过 Handle 获取元数据指针（可修改）
        /// </summary>
        AssetMetadata* GetMetadata(AssetHandle handle);

        /// <summary>
        /// 通过文件路径获取 Handle（未找到返回 NullAssetHandle）
        /// </summary>
        AssetHandle GetHandle(const std::string& filepath) const;

        /// <summary>
        /// 是否包含指定 Handle
        /// </summary>
        bool Contains(AssetHandle handle) const;

        /// <summary>
        /// 是否包含指定文件路径
        /// </summary>
        bool ContainsPath(const std::string& filepath) const;

        /// <summary>
        /// 获取已注册资产数量
        /// </summary>
        size_t GetAssetCount() const { return m_Registry.size(); }

        // ---- 持久化 ----

        /// <summary>
        /// 保存 Registry 到文件
        /// </summary>
        /// <param name="filepath">输出文件路径（如 "AssetRegistry.lcr"）</param>
        void Save(const std::string& filepath) const;

        /// <summary>
        /// 从文件加载 Registry
        /// </summary>
        /// <param name="filepath">Registry 文件路径</param>
        /// <returns>加载是否成功</returns>
        bool Load(const std::string& filepath);

        // ---- 迭代 ----

        /// <summary>
        /// 获取所有已注册的元数据（只读）
        /// </summary>
        const std::unordered_map<AssetHandle, AssetMetadata>& GetAllMetadata() const { return m_Registry; }

    private:
        std::unordered_map<AssetHandle, AssetMetadata> m_Registry;      // Handle -> Metadata
        std::unordered_map<std::string, AssetHandle> m_PathToHandle;    // FilePath -> Handle（反向索引）
    };
}
