#pragma once

#include "Lucky/Core/UUID.h"

#include <yaml-cpp/yaml.h>

namespace Lucky
{
    /// <summary>
    /// 资产句柄：用于唯一标识一个资产
    /// 本质是 64 位 UUID，但与 Entity UUID 类型隔离，提供类型安全
    /// </summary>
    class AssetHandle
    {
    public:
        AssetHandle() : m_Handle(0) {}
        explicit AssetHandle(UUID uuid) : m_Handle(static_cast<uint64_t>(uuid)) {}
        explicit AssetHandle(uint64_t handle) : m_Handle(handle) {}

        /// <summary>
        /// 生成新的随机 AssetHandle
        /// </summary>
        static AssetHandle Generate() { return AssetHandle(UUID()); }

        /// <summary>
        /// 句柄是否有效（非零）
        /// </summary>
        bool IsValid() const { return m_Handle != 0; }

        operator uint64_t() const { return m_Handle; }

        bool operator==(const AssetHandle& other) const { return m_Handle == other.m_Handle; }
        bool operator!=(const AssetHandle& other) const { return m_Handle != other.m_Handle; }
        bool operator<(const AssetHandle& other) const { return m_Handle < other.m_Handle; }

    private:
        uint64_t m_Handle;
    };

    /// <summary>
    /// 无效 Handle 常量
    /// </summary>
    inline const AssetHandle NullAssetHandle{};
}

namespace std
{
    /// <summary>
    /// AssetHandle 哈希特化
    /// </summary>
    template<>
    struct hash<Lucky::AssetHandle>
    {
        std::size_t operator()(const Lucky::AssetHandle& handle) const
        {
            return hash<uint64_t>()(static_cast<uint64_t>(handle));
        }
    };
}

/// <summary>
/// AssetHandle YAML 转换器
/// </summary>
namespace YAML
{
    template<>
    struct convert<Lucky::AssetHandle>
    {
        static Node encode(const Lucky::AssetHandle& handle)
        {
            Node node;
            node.push_back(static_cast<uint64_t>(handle));
            return node;
        }

        static bool decode(const Node& node, Lucky::AssetHandle& handle)
        {
            handle = Lucky::AssetHandle(node.as<uint64_t>());
            return true;
        }
    };
}
