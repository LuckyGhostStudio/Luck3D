#pragma once

#include <string>
#include <filesystem>

namespace Lucky
{
    /// <summary>
    /// 文件对话框
    /// </summary>
    class FileDialogs
    {
    public:
        /// <summary>
        /// 打开文件：取消则返回空字符串
        /// </summary>
        /// <param name="filter">文件过滤器：文件类型</param>
        /// <returns>文件路径</returns>
        static std::string OpenFile(const char* filter);

        /// <summary>
        /// 保存文件：取消则返回空字符串
        /// </summary>
        /// <param name="filter">文件过滤器：文件类型</param>
        /// <returns>文件路径</returns>
        static std::string SaveFile(const char* filter);
    };

    /// <summary>
    /// 系统 Shell 集成：在系统文件管理器中打开路径、外部程序调起等
    /// </summary>
    class PlatformShell
    {
    public:
        /// <summary>
        /// 在系统文件管理器中显示指定路径
        /// - 目录：直接打开该目录
        /// - 文件：打开父目录并选中该文件
        /// Windows 走 ShellExecute；其他平台当前为空实现（记录一条 Warn）
        /// </summary>
        /// <param name="path">目标路径（可为相对或绝对路径，内部会转绝对路径）</param>
        static void RevealInExplorer(const std::filesystem::path& path);
    };
}