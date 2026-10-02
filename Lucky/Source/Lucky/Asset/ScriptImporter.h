#pragma once

#include "AssetImporter.h"

namespace Lucky
{
    /// <summary>
    /// 脚本导入器：把 .cs 文件注册为 Script 资产
    /// 不读取源码内容，也不做 Save（脚本内容由用户在外部编辑器维护）
    /// </summary>
    class ScriptImporter : public AssetImporter
    {
    public:
        Ref<void> Load(const AssetMetadata& metadata) override;

        bool Save(const Ref<Asset>& asset, const std::string& filepath) override;
    };
}
