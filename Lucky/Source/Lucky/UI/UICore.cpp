#include "lcpch.h"
#include "UICore.h"

#include <cstdio>
#include <vector>

namespace Lucky::UI
{
    static int s_UIContextID = 0;                   // 上下文 ID（PushID/PopID 管理）
    static uint32_t s_Counter = 0;                  // 自增计数器
    static std::vector<uint32_t> s_CounterStack;    // 计数器栈（配合 PushID/PopID 使用）
    static char s_IDBuffer[16] = "##";              // ID 缓冲区

    const char* GenerateID()
    {
        snprintf(s_IDBuffer + 2, sizeof(s_IDBuffer) - 2, "%u", s_Counter++);
        return s_IDBuffer;
    }

    void ResetIDCounter()
    {
        s_Counter = 0;
    }

    void PushID()
    {
        ImGui::PushID(s_UIContextID++);
        s_CounterStack.push_back(s_Counter);    // 保存当前计数器
        s_Counter = 0;                          // 新作用域从 0 开始
    }

    void PopID()
    {
        ImGui::PopID();
        s_UIContextID--;

        // 从栈恢复上一层计数器；栈空时兜底清零，避免未配对 Push 时下溢
        if (!s_CounterStack.empty())
        {
            s_Counter = s_CounterStack.back();
            s_CounterStack.pop_back();
        }
        else
        {
            s_Counter = 0;
        }
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