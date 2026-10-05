#pragma once

#include <imgui/imgui.h>

namespace Lucky::UI
{
    /// <summary>
    /// 生成唯一 ID（格式："##0", "##1", "##2" ...）
    /// 当前作用域内每次调用返回递增的 ID；作用域由 PushID/PopID 或面板起始时的 ResetIDCounter 划分。
    /// </summary>
    /// <returns>唯一 ID 字符串（指向内部静态缓冲区，下次调用会覆盖）</returns>
    const char* GenerateID();

    /// <summary>
    /// 把 ID 计数器清零（不改变 ImGui 的 ID 栈）。
    /// 由 EditorPanel::OnImGuiRender 在每帧每个面板起始时调用，
    /// 保证同一面板跨帧生成的 ID 稳定，从而让 ComboBox / Popup / 焦点状态能跨帧保持。
    /// </summary>
    void ResetIDCounter();

    /// <summary>
    /// 进入新的 ID 作用域：Push 一个唯一的上下文 ID，把当前计数器压栈并清零。
    /// 必须与 PopID() 配对使用（PopID 会从栈弹出恢复计数器）。
    /// </summary>
    void PushID();

    /// <summary>
    /// 退出当前 ID 作用域：从栈弹出恢复上一层计数器。
    /// </summary>
    void PopID();

    /// <summary>
    /// 水平偏移光标
    /// </summary>
    /// <param name="distance">偏移距离（像素）</param>
    void ShiftCursorX(float distance);

    /// <summary>
    /// 垂直偏移光标
    /// </summary>
    /// <param name="distance">偏移距离（像素）</param>
    void ShiftCursorY(float distance);

    /// <summary>
    /// 同时偏移光标的水平和垂直位置
    /// </summary>
    /// <param name="x">水平偏移距离（像素）</param>
    /// <param name="y">垂直偏移距离（像素）</param>
    void ShiftCursor(float x, float y);
}