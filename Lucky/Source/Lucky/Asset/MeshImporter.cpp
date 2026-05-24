#include "lcpch.h"
#include "MeshImporter.h"

#include "ModelLoader.h"

#include <filesystem>

namespace Lucky
{
    Ref<void> MeshImporter::Load(const AssetMetadata& metadata)
    {
        std::string absolutePath = std::filesystem::absolute(metadata.FilePath).string();

        ModelLoadResult result = ModelLoader::Load(absolutePath);
        if (result.Success)
        {
            return result.MeshData;
        }

        LF_CORE_ERROR("MeshImporter: Failed to load mesh: '{0}' - {1}", absolutePath, result.ErrorMessage);
        return nullptr;
    }
}