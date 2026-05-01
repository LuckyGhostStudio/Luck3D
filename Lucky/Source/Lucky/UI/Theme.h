#pragma once

namespace Lucky::UI::Theme
{
    /// <summary>
    /// 布局常量：控件间距、偏移、尺寸等
    /// </summary>
    namespace Layout
    {
        // ---- PropertyGrid 布局 ----
        constexpr float PropertyLabelOffsetX = 10.0f;       // Label 列水平偏移
        constexpr float PropertyLabelOffsetY = 9.0f;        // Label 列垂直偏移
        constexpr float PropertyValueOffsetY = 4.0f;        // Value 列垂直偏移
        constexpr float PropertyRowSpacing = 8.0f;          // 属性行间距
        constexpr float PropertyFramePaddingX = 4.0f;       // 属性控件内边距 X
        constexpr float PropertyFramePaddingY = 4.0f;       // 属性控件内边距 Y
        constexpr float PropertyLabelWidthRatio = 0.35f;    // Label 列宽占面板宽度的比例
        constexpr float PropertyLabelMinWidth = 80.0f;      // Label 列最小宽度

        // ---- 组件头部 ----
        constexpr float ComponentHeaderPaddingX = 4.0f;         // 组件头部内边距 X
        constexpr float ComponentHeaderPaddingY = 4.0f;         // 组件头部内边距 Y
        constexpr float ComponentSettingsButtonSize = 30.0f;    // 组件设置按钮尺寸
        constexpr float ComponentSettingsButtonOffset = 18.0f;  // 组件设置按钮右偏移

        // ---- 纹理预览 ----
        constexpr float TexturePreviewSize = 64.0f; // 纹理预览图尺寸

        // ---- 通用 ----
        constexpr float ItemSpacingX = 8.0f;        // 控件水平间距
        constexpr float ItemSpacingY = 8.0f;        // 控件垂直间距
        constexpr float IndentSpacing = 12.0f;      // 缩进间距
        constexpr float SeparatorSpacingY = 18.0f;  // 分隔线后的垂直间距
    }
}