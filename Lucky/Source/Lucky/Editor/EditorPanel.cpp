#include "lcpch.h"
#include "EditorPanel.h"

#include "Lucky/UI/UICore.h"
#include "Lucky/UI/Widgets.h"

#include <imgui/imgui.h>

namespace Lucky
{
    void EditorPanel::OnImGuiRender(const char* name, bool& isOpen)
    {
        if (!isOpen)
        {
            // 面板关闭时清零状态，避免陈旧的 Hovered/Focused 被 OnEvent 误用
            m_IsHovered = false;
            m_IsFocused = false;
            return;
        }

        OnBegin(name);

        // 每个面板起始时把 UI::GenerateID 的计数器清零 —— 让同一面板跨帧生成的 ID 稳定。
        // 否则 UI::DropdownList 等用 GenerateID() 做 label 的控件 ID 每帧漂移，
        // ImGui 的 ComboBox Popup / 焦点等跨帧状态会丢失，下拉框点了打不开。
        UI::ResetIDCounter();
        
        if (UI::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Close Tab"))
            {
                isOpen = false;
            }

            UI::EndPopup();
        }
        
        OnGUI();

        // 统一捕获面板的鼠标悬停 / 聚焦状态。
        // 时机：OnGUI 之后、OnEnd 之前 —— 此时子类内部的 BeginChild 已全部 EndChild，
        // 上下文回到最外层 ImGui::Begin 窗口，判定结果覆盖整个面板矩形。
        // Flag：ChildWindows / RootAndChildWindows 让嵌套子窗口的命中向上冒泡。
        m_IsHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
        m_IsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        OnEnd();
    }

    void EditorPanel::OnBegin(const char* name)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 }); // 窗口 padding = 0
        ImGui::Begin(name, nullptr, m_WindowFlags);
    }

    void EditorPanel::OnEnd()
    {
        ImGui::End();
        ImGui::PopStyleVar();
    }
}