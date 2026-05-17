#pragma once

#include "AssetHandle.h"
#include "AssetType.h"

namespace Lucky
{
    /// <summary>
    /// 资产基类：所有可被资产系统管理的资源类型的基类
    /// Material、Mesh、Texture 等均继承自此类
    /// 提供统一的 Handle 访问和类型查询能力
    /// </summary>
    class Asset
    {
    public:
        virtual ~Asset() = default;

        /// <summary>
        /// 获取资产 Handle
        /// </summary>
        AssetHandle GetHandle() const { return m_Handle; }

        /// <summary>
        /// 设置资产 Handle（由 AssetManager 在加载时调用）
        /// </summary>
        void SetHandle(AssetHandle handle) { m_Handle = handle; }

        /// <summary>
        /// 获取资产类型（纯虚函数，派生类必须实现）
        /// </summary>
        virtual AssetType GetAssetType() const = 0;
    private:
        AssetHandle m_Handle;
    };
}
