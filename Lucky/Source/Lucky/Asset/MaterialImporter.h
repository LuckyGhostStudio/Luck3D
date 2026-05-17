#pragma once

#include "AssetImporter.h"

namespace Lucky
{
    /// <summary>
    /// 材质导入器：从 .lmat 文件加载材质
    /// </summary>
    class MaterialImporter : public AssetImporter
    {
    public:
        Ref<void> Load(const AssetMetadata& metadata) override;
    };
}
