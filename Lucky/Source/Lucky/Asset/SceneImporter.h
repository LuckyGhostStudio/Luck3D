#pragma once

#include "AssetImporter.h"

namespace Lucky
{
    /// <summary>
    /// 场景资产导入器：从 .luck3d 文件加载场景
    /// </summary>
    class SceneImporter : public AssetImporter
    {
    public:
        Ref<void> Load(const AssetMetadata& metadata) override;
        bool Save(const Ref<Asset>& asset, const std::string& filepath) override;
    };
}
