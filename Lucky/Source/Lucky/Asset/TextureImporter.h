#pragma once

#include "AssetImporter.h"

namespace Lucky
{
    /// <summary>
    /// 纹理导入器：从图片文件（.png/.jpg/.tga 等）加载 2D 纹理
    /// </summary>
    class TextureImporter : public AssetImporter
    {
    public:
        Ref<void> Load(const AssetMetadata& metadata) override;
    };
}
