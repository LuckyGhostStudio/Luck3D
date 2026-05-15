#pragma once

#include "AssetImporter.h"

namespace Lucky
{
    /// <summary>
    /// 网格导入器：从模型文件（.obj/.fbx/.gltf 等）加载网格
    /// 内部委托给已有的 MeshImporter（Assimp 封装）
    /// </summary>
    class MeshAssetImporter : public AssetImporter
    {
    public:
        Ref<void> Load(const AssetMetadata& metadata) override;
    };
}
