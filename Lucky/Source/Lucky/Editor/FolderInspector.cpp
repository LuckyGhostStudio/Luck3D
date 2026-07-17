#include "lcpch.h"
#include "FolderInspector.h"

#include "Lucky/Editor/EditorIconManager.h"
#include "Lucky/Editor/InspectorHeader.h"

#include <imgui.h>

namespace Lucky
{
    void FolderInspector::Draw(const std::filesystem::path& path)
    {
        if (path.empty())
        {
            return;
        }

        // 目录名（等价于 Unity："Assets/Foo/Bar" 的 Header 显示 "Bar (Folder)"）
        std::string displayName = path.filename().string();
        if (displayName.empty())
        {
            displayName = path.string();    // 极端情况：根路径 fallback
        }

        // 折叠状态的 Folder 图标
        const Ref<Texture2D>& icon = EditorIconManager::GetFolderIcon(false);

        InspectorHeader::Draw(icon, displayName, "Folder", "FolderSettings");
    }
}
