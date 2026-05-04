#include "lcpch.h"
#include "EditorPanel.h"

#include "Lucky/UI/Widgets.h"

#include <imgui/imgui.h>

namespace Lucky
{
    void EditorPanel::OnImGuiRender(const char* name, bool& isOpen)
    {
        if (!isOpen)
        {
            return;
        }

        OnBegin(name);
        
        if (UI::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Close Tab"))
            {
                isOpen = false;
            }

            UI::EndPopup();
        }
        
        OnGUI();

        OnEnd();
    }

    void EditorPanel::OnBegin(const char* name)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 }); // ´°¿Ú padding = 0
        ImGui::Begin(name);
    }

    void EditorPanel::OnEnd()
    {
        ImGui::End();
        ImGui::PopStyleVar();
    }
}