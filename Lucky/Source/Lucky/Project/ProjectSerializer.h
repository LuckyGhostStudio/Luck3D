#pragma once

#include "Lucky/Core/Base.h"
#include "Project.h"

#include <filesystem>

namespace Lucky
{
    /// <summary>
    /// .lcproj 文件的 YAML 序列化 / 反序列化
    /// </summary>
    class ProjectSerializer
    {
    public:
        /// <summary>
        /// 序列化 Project 到 .lcproj 文件
        /// </summary>
        /// <param name="project">要保存的 Project</param>
        /// <param name="filepath">.lcproj 文件绝对路径</param>
        /// <returns>是否成功</returns>
        static bool Serialize(const Ref<Project>& project, const std::filesystem::path& filepath);

        /// <summary>
        /// 反序列化 .lcproj 文件到 Project
        /// </summary>
        /// <param name="filepath">.lcproj 文件绝对路径</param>
        /// <returns>加载成功返回 Project，失败返回 nullptr</returns>
        static Ref<Project> Deserialize(const std::filesystem::path& filepath);
    };
}
