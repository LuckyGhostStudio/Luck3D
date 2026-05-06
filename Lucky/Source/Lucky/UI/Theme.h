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
        constexpr float Alpha = 1.0f;                       // 全局透明度
        constexpr float DisabledAlpha = 0.60f;              // 禁用状态的透明度
        constexpr float WindowPaddingX = 8.0f;              // 窗口内边距 X
        constexpr float WindowPaddingY = 8.0f;              // 窗口内边距 Y
        constexpr float WindowRounding = 4.0f;              // 窗口圆角半径
        constexpr float WindowBorderSize = 1.0f;            // 窗口边框大小
        constexpr float WindowMinSizeX = 50.0f;             // 窗口最小尺寸 X
        constexpr float WindowMinSizeY = 50.0f;             // 窗口最小尺寸 Y
        constexpr float WindowTitleAlignX = 0.0f;           // 窗口标题对齐 X
        constexpr float WindowTitleAlignY = 0.5f;           // 窗口标题对齐 Y
        constexpr int WindowMenuButtonPosition = -1;        // 窗口菜单按钮位置 (-1=左, 0=无, 1=右)
        constexpr float ChildRounding = 4.0f;               // 子窗口圆角半径
        constexpr float ChildBorderSize = 1.0f;             // 子窗口边框大小
        constexpr float PopupRounding = 4.0f;               // 弹出窗口圆角半径
        constexpr float PopupBorderSize = 1.0f;             // 弹出窗口边框大小
        constexpr float FramePaddingX = 6.0f;               // 框架内边距 X
        constexpr float FramePaddingY = 2.0f;               // 框架内边距 Y
        constexpr float FrameRounding = 4.0f;               // 框架圆角半径
        constexpr float FrameBorderSize = 1.0f;             // 框架边框大小
        constexpr float ItemSpacingX = 8.0f;                // 项目间距 X
        constexpr float ItemSpacingY = 4.0f;                // 项目间距 Y
        constexpr float ItemInnerSpacingX = 4.0f;           // 项目内间距 X
        constexpr float ItemInnerSpacingY = 4.0f;           // 项目内间距 Y
        constexpr float CellPaddingX = 4.0f;                // 单元格内边距 X
        constexpr float CellPaddingY = 2.0f;                // 单元格内边距 Y
        constexpr float TouchExtraPaddingX = 0.0f;          // 触摸额外内边距 X
        constexpr float TouchExtraPaddingY = 0.0f;          // 触摸额外内边距 Y
        constexpr float IndentSpacing = 21.0f;              // 缩进间距
        constexpr float ColumnsMinSpacing = 6.0f;           // 列最小间距
        constexpr float ScrollbarSize = 18.0f;              // 滚动条大小
        constexpr float ScrollbarRounding = 12.0f;          // 滚动条圆角半径
        constexpr float GrabMinSize = 10.0f;                // 抓取最小大小
        constexpr float GrabRounding = 4.0f;                // 抓取圆角半径
        constexpr float LogSliderDeadzone = 4.0f;           // 对数滑块死区
        constexpr float TabRounding = 2.0f;                 // 标签页圆角半径
        constexpr float TabBorderSize = 0.0f;               // 标签页边框大小
        constexpr float TabMinWidthForCloseButton = 0.0f;   // 显示关闭按钮的标签页最小宽度
        constexpr int ColorButtonPosition = 1;              // 颜色按钮位置 (0=左, 1=右)
        constexpr float ButtonTextAlignX = 0.5f;            // 按钮文本对齐 X
        constexpr float ButtonTextAlignY = 0.5f;            // 按钮文本对齐 Y
        constexpr float SelectableTextAlignX = 0.0f;        // 可选文本对齐 X
        constexpr float SelectableTextAlignY = 0.5f;        // 可选文本对齐 Y
        constexpr float DisplayWindowPaddingX = 19.0f;      // 显示窗口内边距 X
        constexpr float DisplayWindowPaddingY = 19.0f;      // 显示窗口内边距 Y
        constexpr float DisplaySafeAreaPaddingX = 3.0f;     // 显示安全区内边距 X
        constexpr float DisplaySafeAreaPaddingY = 3.0f;     // 显示安全区内边距 Y
        constexpr float MouseCursorScale = 1.0f;            // 鼠标光标缩放
        constexpr bool AntiAliasedLines = true;             // 抗锯齿线条
        constexpr bool AntiAliasedLinesUseTex = true;       // 使用纹理抗锯齿线条
        constexpr bool AntiAliasedFill = true;              // 抗锯齿填充
        constexpr float CurveTessellationTol = 1.25f;       // 曲线细分容差
        constexpr float CircleTessellationMaxError = 0.30f; // 圆细分最大误差
    }
}