#pragma once

#include "AssetImporter.h"

namespace Lucky
{
    /// <summary>
    /// Mesh 资产导入器：加载 .lmesh 格式的引擎内部网格资产
    /// 不兼容外部模型格式（.fbx/.obj 等），外部模型由 ModelLoader 在导入阶段转换
    /// </summary>
    class MeshImporter : public AssetImporter
    {
    public:
        Ref<void> Load(const AssetMetadata& metadata) override;
    };
}