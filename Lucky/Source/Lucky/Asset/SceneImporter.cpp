#include "lcpch.h"
#include "SceneImporter.h"

#include "Lucky/Scene/Scene.h"
#include "Lucky/Serialization/SceneSerializer.h"

#include <filesystem>

namespace Lucky
{
    Ref<void> SceneImporter::Load(const AssetMetadata& metadata)
    {
        std::string absolutePath = std::filesystem::absolute(metadata.FilePath).string();

        Ref<Scene> scene = CreateRef<Scene>();

        if (!SceneSerializer::Deserialize(scene, absolutePath))
        {
            LF_CORE_ERROR("SceneImporter: Failed to load scene from '{0}'", metadata.FilePath);
            return nullptr;
        }

        LF_CORE_INFO("SceneImporter: Loaded scene '{0}' from '{1}'", scene->GetName(), metadata.FilePath);
        return scene;
    }

    bool SceneImporter::Save(const Ref<Asset>& asset, const std::string& filepath)
    {
        auto scene = std::static_pointer_cast<Scene>(asset);
        if (!scene)
        {
            LF_CORE_ERROR("SceneImporter::Save - Invalid scene asset!");
            return false;
        }

        SceneSerializer::Serialize(scene, filepath);
        return true;
    }
}
