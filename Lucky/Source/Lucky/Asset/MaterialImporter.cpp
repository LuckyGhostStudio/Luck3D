#include "lcpch.h"
#include "MaterialImporter.h"

#include "Lucky/Serialization/MaterialSerializer.h"

#include <filesystem>
#include <yaml-cpp/yaml.h>

namespace Lucky
{
    Ref<void> MaterialImporter::Load(const AssetMetadata& metadata)
    {
        std::string absolutePath = std::filesystem::absolute(metadata.FilePath).string();

        if (!std::filesystem::exists(absolutePath))
        {
            LF_CORE_ERROR("MaterialImporter: File not found: '{0}'", absolutePath);
            return nullptr;
        }

        YAML::Node data = YAML::LoadFile(absolutePath);

        // .mat 文件的根节点就是材质数据
        Ref<Material> material = MaterialSerializer::Deserialize(data);
        if (!material)
        {
            LF_CORE_ERROR("MaterialImporter: Failed to deserialize material: '{0}'", absolutePath);
            return nullptr;
        }

        return material;
    }
}
