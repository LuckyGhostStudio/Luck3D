#include "lcpch.h"
#include "ScriptImporter.h"

#include "Lucky/Asset/Script.h"

#include <filesystem>

namespace Lucky
{
    Ref<void> ScriptImporter::Load(const AssetMetadata& metadata)
    {
        // 只取文件名词干，不读取源码内容，因此不需要转绝对路径
        std::string className = std::filesystem::path(metadata.FilePath).stem().string();

        LF_CORE_TRACE("ScriptImporter: Registered script '{0}'", className);
        return CreateRef<Script>(className);
    }

    bool ScriptImporter::Save(const Ref<Asset>& asset, const std::string& filepath)
    {
        // 脚本内容由用户在外部编辑器维护，引擎不做序列化，视为保存成功
        return true;
    }
}
