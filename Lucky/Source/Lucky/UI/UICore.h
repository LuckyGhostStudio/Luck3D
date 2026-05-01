#pragma once

#include <imgui/imgui.h>

namespace Lucky::UI
{
    /// <summary>
    /// 生成唯一 ID（格式："##0", "##1", "##2" ...）
    /// 在 PushID/PopID 作用域内使用，每次调用返回不同的 ID
    /// </summary>
    /// <returns>唯一 ID 字符串（指向内部静态缓冲区，下次调用会覆盖）</returns>
    const char* GenerateID();

    /// <summary>
    /// 进入新的 ID 作用域：Push 一个唯一的上下文 ID，并重置计数器
    /// 必须与 PopID() 配对使用
    /// </summary>
    void PushID();

    /// <summary>
    /// 退出当前 ID 作用域
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