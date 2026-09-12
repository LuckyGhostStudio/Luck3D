#include "lcpch.h"
#include "MeshImporter.h"

#include "Lucky/Project/Project.h"
#include "Lucky/Serialization/MeshSerializer.h"

#include <filesystem>

namespace Lucky
{
    Ref<void> MeshImporter::Load(const AssetMetadata& metadata)
    {
        std::string absolutePath = Project::GetActive()->ResolveAbsolute(metadata.FilePath).string();

        // 只支持 .lmesh 格式（引擎内部 Mesh 资产格式）
        Ref<Mesh> mesh = MeshSerializer::Deserialize(absolutePath);
        if (!mesh)
        {
            LF_CORE_ERROR("MeshImporter: Failed to load .lmesh: '{0}'", absolutePath);
            return nullptr;
        }
        return mesh;
    }
}