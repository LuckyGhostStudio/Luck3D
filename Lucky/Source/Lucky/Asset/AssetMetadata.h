#pragma once

#include "AssetHandle.h"
#include "AssetType.h"

#include <string>

namespace Lucky
{
    /// <summary>
    /// 资产元数据：描述一个已注册资产的基本信息
    /// </summary>
    struct AssetMetadata
    {
        AssetHandle Handle;                     // 资产唯一标识
        AssetType Type = AssetType::None;       // 资产类型
        std::string FilePath;                   // 相对于项目根目录的文件路径（使用正斜杠 /）

        /// <summary>
        /// 元数据是否有效
        /// </summary>
        bool IsValid() const
        {
            return Handle.IsValid() && Type != AssetType::None && !FilePath.empty();
        }
    };
}
