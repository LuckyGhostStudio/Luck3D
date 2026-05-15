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

        return Texture2D::Create(absolutePath);
    }
}
