#pragma once

#include "AssetMetadata.h"

#include "Lucky/Core/Base.h"

namespace Lucky
{
    /// <summary>
    /// 资产导入器基类：定义资产加载的统一接口
    /// 每种资产类型实现一个具体的 Importer 派生类
    /// </summary>
    class AssetImporter
    {
    public:
        virtual ~AssetImporter() = default;

        /// <summary>
        /// 从磁盘加载资产
        /// </summary>
        /// <param name="metadata">资产元数据（包含文件路径和类型）</param>
        /// <returns>加载的资产实例（通过 Ref<void> 类型擦除），失败返回 nullptr</returns>
        virtual Ref<void> Load(const AssetMetadata& metadata) = 0;
    };
}
