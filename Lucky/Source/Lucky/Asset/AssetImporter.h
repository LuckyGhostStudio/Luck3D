#pragma once

#include "AssetMetadata.h"
#include "Asset.h"

#include "Lucky/Core/Base.h"

namespace Lucky
{
    /// <summary>
    /// 资产导入器基类：定义资产加载和保存的统一接口
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

        /// <summary>
        /// 将资产保存到磁盘（可选实现）
        /// 不是所有资产类型都支持保存（如 Texture 由外部工具生成）
        /// </summary>
        /// <param name="asset">要保存的资产实例</param>
        /// <param name="filepath">保存的绝对文件路径</param>
        /// <returns>是否保存成功</returns>
        virtual bool Save(const Ref<Asset>& asset, const std::string& filepath) { return false; }
    };
}
