#include "lcpch.h"
#include "TextureImporter.h"

#include "Lucky/Renderer/Texture.h"

#include <filesystem>

namespace Lucky
{
    Ref<void> TextureImporter::Load(const AssetMetadata& metadata)
    {
        std::string absolutePath = std::filesystem::absolute(metadata.FilePath).string();

        if (!std::filesystem::exists(absolutePath))
        {
            LF_CORE_ERROR("TextureImporter: File not found: '{0}'", absolutePath);
            return nullptr;
        }

        Ref<Texture2D> texture = Texture2D::Create(absolutePath);
        if (texture)
        {
            // 从文件路径 stem（文件名不含扩展名）设置资产名称
            // 图片文件本身不携带 Asset 名字信息，此处兜底以供 Inspector / AssetField 显示
            texture->SetName(std::filesystem::path(metadata.FilePath).stem().string());
        }
        return texture;
    }
}
