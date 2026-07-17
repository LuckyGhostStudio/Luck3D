#pragma once

#include <filesystem>

namespace Lucky
{
    /// <summary>
    /// 目录 Inspector：当用户在 ProjectAssetsPanel 内容区单击目录项时展示
    /// 显示 Header（图标 + 名称 + (Folder) + 设置按钮），与 Asset Inspector 视觉一致
    /// </summary>
    class FolderInspector
    {
    public:
        static void Draw(const std::filesystem::path& path);
    };
}
