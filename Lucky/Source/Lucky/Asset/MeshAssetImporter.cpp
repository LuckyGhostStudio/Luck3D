#include "lcpch.h"
#include "MeshAssetImporter.h"

#include "MeshImporter.h"

#include <filesystem>

namespace Lucky
{
    Ref<void> MeshAssetImporter::Load(const AssetMetadata& metadata)
    {
        std::string absolutePath = std::filesystem::absolute(metadata.FilePath).string();

        MeshImportResult result = MeshImporter::Import(absolutePath);
        if (result.Success)
        {
            return result.MeshData;
        }

        LF_CORE_ERROR("MeshAssetImporter: Failed to load mesh: '{0}' - {1}", absolutePath, result.ErrorMessage);
        return nullptr;
    }
}
