#pragma once

#include "Lucky/Core/Base.h"

#include <imgui/imgui.h>

#include <cstdarg>

namespace Lucky::UI
{
    /// <summary>
    /// Tooltip 样式（可选）。为 nullptr / 默认构造时方角 + 0.4s 延迟。
    /// </summary>
    struct TooltipStyle
    {
        float Rounding = 0.0f;          // 窗口圆角半径（0 = 方角）
        float DelaySeconds = 0.4f;      // Hover 多少秒后才显示
    };

    /// <summary>
    /// 显示样式统一的 Tooltip。必须紧跟在被 Hover 的控件之后调用；
    /// 内部自己做 IsItemHovered + 延迟判定，调用方不需要外部 if (IsItemHovered)。
    ///
    /// 行为：
    ///   - 固定出现在上一个 Item 正下方（左对齐 Item 左边界 + TooltipItemGap 像素间距），不跟随鼠标
    ///   - 独立 Push WindowPadding / Border / Bg / Rounding，不受父面板 WindowPadding 影响
    ///   - 延迟 TooltipDelaySeconds 秒后才出现（Unity 一致）
    /// </summary>
    void SetItemTooltip(const char* fmt, ...);
    void SetItemTooltipV(const char* fmt, va_list args);

    /// <summary>
    /// 带样式参数的版本（用于需要圆角等自定义时）
    /// </summary>
    void SetItemTooltipEx(const TooltipStyle& style, const char* fmt, ...);
    void SetItemTooltipExV(const TooltipStyle& style, const char* fmt, va_list args);
}
