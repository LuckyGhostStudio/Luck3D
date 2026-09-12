#pragma once

#include <filesystem>

namespace Lucky
{
    /// <summary>
    /// 文件系统工具（平台无关封装）
    /// </summary>
    class FileSystem
    {
    public:
        /// <summary>
        /// 获取编辑器可执行文件所在目录（首次调用后缓存）
        /// 用于定位所有随 exe 一起分发的资源（图标、字体、Mono、内置 shader 等）
        /// </summary>
        /// <returns>exe 所在目录的绝对路径</returns>
        static const std::filesystem::path& GetEditorExecutableDirectory();
    };
}
