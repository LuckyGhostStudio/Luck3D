#include "PreferencesPanel.h"

#include "Lucky/Editor/EditorPreferences.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

namespace Lucky
{
    void PreferencesPanel::OnImGuiRender()
    {
        if (!m_IsOpen)
        {
            return;
        }
        
        ImGui::SetNextWindowSize(ImVec2(720, 560), ImGuiCond_FirstUseEver);
        
        if (ImGui::Begin("Preferences", &m_IsOpen))
        {
            // ---- 左侧分类列表 ----
            ImGui::BeginChild("Categories", ImVec2(150, 0), true);
            {
                if (ImGui::Selectable("Colors", m_SelectedCategory == 0))
                {
                    m_SelectedCategory = 0;
                }
                
                // TODO 其他分类
            }
            ImGui::EndChild();
            
            ImGui::SameLine();
            
            // ---- 右侧设置内容 ----
            ImGui::BeginChild("Content", ImVec2(0, 0), false);
            {
                switch (m_SelectedCategory)
                {
                case 0:
                    DrawColorsPage();
                    break;
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
    
    void PreferencesPanel::DrawColorsPage()
    {
        auto& colors = EditorPreferences::Get().GetColors();
        bool changed = false;
        
        // ---- Gizmo 颜色 ----
        if (ImGui::TreeNodeEx("Gizmo Colors", ImGuiTreeNodeFlags_DefaultOpen))
        {
            changed |= ImGui::ColorEdit4("Grid X Axis", glm::value_ptr(colors.GridAxisXColor));
            changed |= ImGui::ColorEdit4("Grid Z Axis", glm::value_ptr(colors.GridAxisZColor));
            changed |= ImGui::ColorEdit4("Grid Lines", glm::value_ptr(colors.GridLineColor));
            
            ImGui::TreePop();
        }
        
        ImGui::Separator();
        
        // ---- 视口颜色 ----
        if (ImGui::TreeNodeEx("Viewport", ImGuiTreeNodeFlags_DefaultOpen))
        {
            changed |= ImGui::ColorEdit4("Clear Color", glm::value_ptr(colors.ViewportClearColor));
            
            ImGui::TreePop();
        }
        
        ImGui::Separator();
        
        // ---- UI 颜色 ----
        if (ImGui::TreeNodeEx("UI Colors", ImGuiTreeNodeFlags_DefaultOpen))
        {
            changed |= ImGui::ColorEdit4("Window Bg", glm::value_ptr(colors.WindowBackground));
            changed |= ImGui::ColorEdit4("Child Bg", glm::value_ptr(colors.ChildBackground));
            
            ImGui::Spacing();
            
            changed |= ImGui::ColorEdit4("Frame Bg", glm::value_ptr(colors.FrameBackground));
            changed |= ImGui::ColorEdit4("Frame Hovered", glm::value_ptr(colors.FrameBackgroundHovered));
            changed |= ImGui::ColorEdit4("Frame Active", glm::value_ptr(colors.FrameBackgroundActive));
            
            ImGui::Spacing();
            
            changed |= ImGui::ColorEdit4("Button", glm::value_ptr(colors.ButtonColor));
            changed |= ImGui::ColorEdit4("Button Hovered", glm::value_ptr(colors.ButtonHovered));
            changed |= ImGui::ColorEdit4("Button Active", glm::value_ptr(colors.ButtonActive));
            
            ImGui::Spacing();
            
            changed |= ImGui::ColorEdit4("Header", glm::value_ptr(colors.HeaderColor));
            changed |= ImGui::ColorEdit4("Header Hovered", glm::value_ptr(colors.HeaderHovered));
            changed |= ImGui::ColorEdit4("Header Active", glm::value_ptr(colors.HeaderActive));
            
            ImGui::Spacing();
            
            changed |= ImGui::ColorEdit4("Title Bar", glm::value_ptr(colors.TitleBarBackground));
            
            ImGui::Spacing();
            
            changed |= ImGui::ColorEdit4("Tab", glm::value_ptr(colors.TabColor));
            changed |= ImGui::ColorEdit4("Tab Hovered", glm::value_ptr(colors.TabHovered));
            changed |= ImGui::ColorEdit4("Tab Active", glm::value_ptr(colors.TabActive));
            
            ImGui::Spacing();
            
            changed |= ImGui::ColorEdit4("Text", glm::value_ptr(colors.TextColor));
            changed |= ImGui::ColorEdit4("Text Disabled", glm::value_ptr(colors.TextDisabledColor));
            
            ImGui::Spacing();
            
            changed |= ImGui::ColorEdit4("Separator", glm::value_ptr(colors.SeparatorColor));
            
            ImGui::Spacing();
            
            changed |= ImGui::ColorEdit4("Scrollbar Bg", glm::value_ptr(colors.ScrollbarBackground));
            changed |= ImGui::ColorEdit4("Scrollbar Grab", glm::value_ptr(colors.ScrollbarGrab));
            changed |= ImGui::ColorEdit4("Scrollbar Grab Hovered", glm::value_ptr(colors.ScrollbarGrabHovered));
            changed |= ImGui::ColorEdit4("Scrollbar Grab Active", glm::value_ptr(colors.ScrollbarGrabActive));
            
            ImGui::TreePop();
        }
        
        // 颜色改变时立即应用到 ImGui 主题
        if (changed)
        {
            EditorPreferences::Get().ApplyImGuiColors();
        }
        
        ImGui::Separator();
        ImGui::Spacing();
        
        // 恢复默认设置
        if (ImGui::Button("Reset to Default"))
        {
            EditorPreferences::Get().ResetColorsToDefault();
            EditorPreferences::Get().ApplyImGuiColors();
        }
        
        ImGui::SameLine();
        
        // 保存设置
        if (ImGui::Button("Save"))
        {
            EditorPreferences::Get().Save();
        }
    }
}