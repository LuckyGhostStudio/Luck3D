#include "lcpch.h"
#include "InspectorHeader.h"

#include "Lucky/Editor/EditorIconManager.h"
#include "Lucky/Editor/EditorPreferences.h"

#include "Lucky/UI/UICore.h"
#include "Lucky/UI/Controls.h"
#include "Lucky/UI/Widgets.h"
#include "Lucky/UI/DrawUtils.h"
#include "Lucky/UI/ScopedGuards.h"

#include <imgui.h>

namespace Lucky
{
    void InspectorHeader::Draw(const Ref<Texture2D>& icon, const std::string& displayName, const char* typeLabel, const char* popupId)
    {
        UI::ShiftCursor(8.0f, 8.0f);

        // 图标（左侧大图，UV Y 翻转以匹配 OpenGL 纹理方向）
        if (icon && icon->GetRendererID() != 0)
        {
            UI::ImageFlipped(icon, ImVec2(50, 50));
            ImGui::SameLine();

            UI::ShiftCursorX(8.0f);
        }

        // 名称 (类型)
        std::string label = displayName;
        if (typeLabel && *typeLabel)
        {
            label += " (";
            label += typeLabel;
            label += ")";
        }
        ImGui::TextUnformatted(label.c_str());

        // 设置按钮
        ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail();
        float lineHeight = ImGui::GetTextLineHeight();
        ImGui::SameLine(contentRegionAvail.x - lineHeight);

        const Ref<Texture2D>& settingsIcon = EditorIconManager::GetSettingsIcon();
        {
            ColorSettings& colorSettings = EditorPreferences::Get().GetColors();

            UI::ScopedStyle buttonBorderSize(ImGuiStyleVar_FrameBorderSize, 0.0f);
            UI::ScopedColor buttonColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            UI::ScopedColor buttonActiveColor(ImGuiCol_ButtonActive, { colorSettings.ButtonHovered.x, colorSettings.ButtonHovered.y, colorSettings.ButtonHovered.z, colorSettings.ButtonHovered.w });
            if (UI::ImageButtonFlipped(settingsIcon, ImVec2(lineHeight, lineHeight), 0))
            {
                ImGui::OpenPopup(popupId);  // 打开弹出框
            }
        }

        // 渲染弹出框
        if (UI::BeginPopup(popupId))
        {
            // TODO 具体设置内容由各 Inspector 未来自行扩展
            
            UI::EndPopup();
        }

        UI::ShiftCursorY(18.0f);
        UI::Draw::HorizontalLine();
        UI::ShiftCursorY(4.0f);
    }
}
