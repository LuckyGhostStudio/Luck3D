#include "lcpch.h"
#include "UICore.h"

#include <cstdio>

namespace Lucky::UI
{
    static int s_UIContextID = 0;       // 上下文 ID（PushID/PopID 管理）
    static uint32_t s_Counter = 0;      // 自增计数器
    static char s_IDBuffer[16] = "##";  // ID 缓冲区

    const char* GenerateID()
    {
        snprintf(s_IDBuffer + 2, sizeof(s_IDBuffer) - 2, "%u", s_Counter++);
        return s_IDBuffer;
    }

    void PushID()
    {
        ImGui::PushID(s_UIContextID++);
        s_Counter = 0;  // 重置计数器，每个作用域从 0 开始
    }

    void PopID()
    {
        ImGui::PopID();
        s_UIContextID--;
    }

    void ShiftCursorX(float distance)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + distance);
    }

    void ShiftCursorY(float distance)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + distance);
    }

    void ShiftCursor(float x, float y)
    {
        const ImVec2 cursor = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cursor.x + x, cursor.y + y));
    }
}