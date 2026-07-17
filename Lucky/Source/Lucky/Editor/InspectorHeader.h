#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Texture.h"

#include <string>

namespace Lucky
{
    /// <summary>
    /// Inspector 顶部信息头（Header）通用控件
    /// 用于 AssetInspector / FolderInspector 等所有 Inspector 共享同一份视觉表达
    /// 
    /// 布局（Unity 风格）：
    /// [Icon 50x50]  DisplayName (TypeLabel)                     [SettingsBtn]
    /// -------------------------------------------------------------------
    /// </summary>
    class InspectorHeader
    {
    public:
        /// <summary>
        /// 绘制通用 Inspector 头部
        /// </summary>
        /// <param name="icon">左侧大图标（通常 50x50）</param>
        /// <param name="displayName">主标题（如资产名 / 目录名）</param>
        /// <param name="typeLabel">附在标题后的类型描述（如 "Material" / "Folder"）</param>
        /// <param name="popupId">设置按钮弹出框的 ID（不同 Header 使用不同 ID 避免冲突）</param>
        static void Draw(const Ref<Texture2D>& icon, const std::string& displayName, const char* typeLabel, const char* popupId);
    };
}
