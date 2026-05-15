#include "lcpch.h"
#include "MaterialImporter.h"

#include "Lucky/Serialization/MaterialSerializer.h"

#include <filesystem>

namespace Lucky
{
    Ref<void> MaterialImporter::Load(const AssetMetadata& metadata)
    {
        std::string absolutePath = std::filesystem::absolute(metadata.FilePath).string();

        Ref<Material> material = MaterialSerializer::DeserializeFromFile(absolutePath);

        if (!material)
        {
            LF_CORE_ERROR("MaterialImporter: Failed to load material from '{0}'", metadata.FilePath);
            return nullptr;
        }

        // 设置 Handle（确保与 Registry 中一致）
        material->SetHandle(metadata.Handle);

        return material;
    }
}
