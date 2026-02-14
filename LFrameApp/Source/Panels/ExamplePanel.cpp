#include "ExamplePanel.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <Lucky/Utils/PlatformUtils.h>
#include <Lucky/Editor/EditorUtility.h>

namespace Lucky
{
    ExamplePanel::ExamplePanel()
    {
        
    }

    void ExamplePanel::OnGUI()
    {
        if (ImGui::Button("Open File Dialog"))
        {
            // 打开文件对话框（文件类型名\0 文件类型.lf）
            std::string filepath = FileDialogs::OpenFile("LFile(*.lf)\0*.lf\0");
        }

        if (ImGui::Button("Save File Dialog"))
        {
            // 保存文件对话框（文件类型名\0 文件类型.lf）
            std::string filepath = FileDialogs::SaveFile("LFile(*.lf)\0*.lf\0");
        }
    }

    void ExamplePanel::OnEvent(Event& event)
    {

    }
}
