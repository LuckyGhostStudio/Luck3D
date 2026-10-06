#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Texture.h"

#include <imgui/imgui.h>

namespace Lucky::UI
{
    /// <summary>
    /// Toggle 按钮样式（可选）。为 nullptr 时内部从 EditorPreferences::GetColors 读取默认配色。
    /// </summary>
    struct ToggleStyle
    {
        ImVec4 BgNormal;              // 未选中：常态背景
        ImVec4 BgHovered;             // 未选中：Hover 背景
        ImVec4 BgActive;              // 未选中：按下背景
        ImVec4 BgSelected;            // 选中：常态背景
        ImVec4 BgSelectedHovered;     // 选中：Hover 背景
        ImVec4 BgSelectedActive;      // 选中：按下背景

        // 哪几个角画圆角（ImDrawFlags_RoundCornersXxx）。0 等价于 RoundCornersAll（四角圆）。
        // 位于 BeginSegmentedGroup 作用域内的 Toggle 会被容器根据位置自动 override 此字段。
        ImDrawFlags CornerFlags = 0;
    };

    /// <summary>
    /// RadioGroup 样式。
    /// Connected = true 时相邻 Item 会尝试拼成分段控件（当前版本未实现，按 false 行为处理）。
    /// </summary>
    struct RadioGroupStyle
    {
        float ItemSpacing = 2.0f;           // Item 之间水平间距
        bool Connected = false;             // 分段圆角（当前未实现，预留）
        ToggleStyle Items;                  // 单个 Item 的样式（直接复用 Toggle 样式）
    };

    /// <summary>
    /// 纯图标 Toggle 按钮。点击时内部翻转 value 并返回 true。
    /// 需要 Tooltip 请在调用之后紧跟 UI::SetItemTooltip(...)（Tooltip 对任何上一个 Item 都生效）。
    /// </summary>
    /// <param name="strID">ImGui ID 字符串（如 "##Play"），避免同帧多个按钮 ID 冲突</param>
    /// <param name="icon">图标纹理（内部用 ImageButtonFlipped 处理 OpenGL Y 翻转）</param>
    /// <param name="value">双向绑定的选中状态；点击时内部翻转</param>
    /// <param name="size">按钮总尺寸（图标撑满该尺寸，内边距为 0）</param>
    /// <param name="style">样式覆盖；nullptr 使用主题默认</param>
    /// <returns>本次是否被点击</returns>
    bool ToggleIconButton(const char* strID, const Ref<Texture2D>& icon, bool& value,
                          const ImVec2& size, const ToggleStyle* style = nullptr);

    /// <summary>
    /// 纯文本 Toggle 按钮。label 同时承担 ImGui ID（如需隐藏 ID 可用 "Grid##GridToggle"）。
    /// </summary>
    bool ToggleTextButton(const char* label, bool& value,
                          const ImVec2& size = { 0.0f, 0.0f },
                          const ToggleStyle* style = nullptr);

    /// <summary>
    /// 图标 + 文本 Toggle 按钮（图标在左、文本在右）。内部用 InvisibleButton + DrawList 自绘。
    /// </summary>
    bool ToggleIconTextButton(const char* strID, const Ref<Texture2D>& icon, const char* label,
                              bool& value, const ImVec2& size = { 0.0f, 0.0f },
                              const ToggleStyle* style = nullptr);

    /// <summary>
    /// 开始一个 Radio 组（互斥、必选一个）。必须与 EndRadioGroup() 配对使用。
    /// 期间所有 RadioXxxItem 的 itemValue 若等于 selectedIndex 则绘制为选中状态；
    /// 点击某个 Item 时 selectedIndex = 该 Item 的 itemValue。
    /// 点击已选中项不做任何事（保证"至少选一个"约束）。
    /// </summary>
    void BeginRadioGroup(const char* strID, int& selectedIndex,
                         const RadioGroupStyle* style = nullptr);

    /// <summary>
    /// 向当前 Radio 组追加一个纯图标 Item。Tooltip 由调用方在之后调 UI::SetItemTooltip。
    /// </summary>
    void RadioIconItem(int itemValue, const Ref<Texture2D>& icon, const ImVec2& size);

    /// <summary>
    /// 向当前 Radio 组追加一个纯文本 Item
    /// </summary>
    void RadioTextItem(int itemValue, const char* label, const ImVec2& size = { 0.0f, 0.0f });

    /// <summary>
    /// 向当前 Radio 组追加一个图标 + 文本 Item
    /// </summary>
    void RadioIconTextItem(int itemValue, const Ref<Texture2D>& icon, const char* label,
                           const ImVec2& size = { 0.0f, 0.0f });

    /// <summary>
    /// 结束 Radio 组
    /// </summary>
    void EndRadioGroup();

    // ========================================================================
    // 分段按钮容器（Segmented Group，外圆内方）
    // ========================================================================

    /// <summary>
    /// 开始一个"分段按钮"容器：相邻 Toggle 在视觉上拼成一条"外圆内方"的按钮条，
    /// 相邻按钮之间保留 spacing 像素的极细缝隙（Unity 风格，Pivot/Center 工具栏那种）。
    /// 作用域内的 Toggle 原语自动按位置设置圆角：
    ///   index 0        → 左两角圆、右两角方
    ///   middle         → 全方
    ///   index count-1  → 左两角方、右两角圆
    ///   count == 1     → 全圆（退化）
    /// Toggle 原语自动在后续 Item 之间调 SameLine(spacing)，调用方不需要自己 SameLine。
    /// 可嵌套 BeginRadioGroup —— Radio 组会把 SameLine 让给 Segmented 容器接管。
    /// 必须与 EndSegmentedGroup 配对使用。
    /// </summary>
    /// <param name="strID">ImGui ID 子作用域</param>
    /// <param name="itemCount">组内 Toggle 总数（必须准确，EndSegmentedGroup 会 Assert）</param>
    /// <param name="spacing">相邻按钮之间的像素间距，默认 1</param>
    void BeginSegmentedGroup(const char* strID, int itemCount, float spacing = 1.0f);

    /// <summary>
    /// 结束分段按钮容器
    /// </summary>
    void EndSegmentedGroup();

    /// <summary>
    /// 工厂：构造"面板局部工具栏"专用的 Toggle 样式。
    /// 规则：未选中灰、未选中悬浮稍亮、按下 = 选中蓝、选中态三色全蓝。
    /// 用于 Scene / 其他局部工具栏的 Grid / CSM / Gizmo 组按钮；
    /// 全局 EditorToolbar 的 Play / Pause 不要用，走默认 Toggle 样式。
    /// </summary>
    ToggleStyle MakeToolbarToggleStyle();

    /// <summary>
    /// 便捷接口：一次性调用描述整个纯图标 Radio 组。内部走 Begin/End。
    /// 该接口不支持 per-item Tooltip（因为是一次性调用，无法在 Item 之后再调 SetItemTooltip）；
    /// 需要 Tooltip 请改用 Begin/End 风格手动展开。
    /// </summary>
    struct RadioIconItemDesc
    {
        int Value;                          // 该 Item 对应的 selectedIndex 值
        const Ref<Texture2D>* Icon;         // 图标（用指针避免拷贝 Ref）
    };

    void RadioIconGroup(const char* strID, int& selectedIndex, const RadioIconItemDesc* items,
                        int count, const ImVec2& itemSize,
                        const RadioGroupStyle* style = nullptr);
}
