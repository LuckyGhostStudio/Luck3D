#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Texture.h"
#include "Lucky/UI/Widgets.h"   // GridIconKind（默认参数值需要完整枚举定义，不能前向声明）

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
        /// <param name="icon">左侧大图标（50x50 框）</param>
        /// <param name="displayName">主标题（如资产名 / 目录名）</param>
        /// <param name="typeLabel">附在标题后的类型描述（如 "Material" / "Folder"）</param>
        /// <param name="popupId">设置按钮弹出框的 ID（不同 Header 使用不同 ID 避免冲突）</param>
        /// <param name="iconKind">图标内容类别：Content 时与资产面板 Grid 同一套"等比居中、永不放大"规则；
        /// Symbolic（默认）满框拉伸</param>
        static void Draw(const Ref<Texture2D>& icon, const std::string& displayName, const char* typeLabel, const char* popupId, UI::GridIconKind iconKind = UI::GridIconKind::Symbolic);
    };
}
