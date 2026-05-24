#pragma once

#include "AssetImporter.h"

namespace Lucky
{
    /// <summary>
    /// Mesh 资产导入器：从模型文件（.obj/.fbx/.gltf 等）加载网格
    /// 内部委托给 ModelLoader（Assimp 封装）
    /// </summary>
    class MeshImporter : public AssetImporter
    {
    public:
        Ref<void> Load(const AssetMetadata& metadata) override;
    };
}