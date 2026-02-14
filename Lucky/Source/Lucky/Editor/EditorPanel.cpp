#include "lpch.h"
#include "EditorPanel.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Lucky
{
    void EditorPanel::OnImGuiRender(const char* name, bool& isOpen)
    {
        if (!isOpen)
        {
            return;
        }

        OnBegin(name);

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Close Tab"))
            {
                isOpen = false;
            }

            ImGui::EndPopup();
        }

        OnGUI();

        OnEnd();
    }

    void EditorPanel::OnBegin(const char* name)
    {
        ImGui::Begin(name);
    }

    void EditorPanel::OnEnd()
    {
        ImGui::End();
    }
}