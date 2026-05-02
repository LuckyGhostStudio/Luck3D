#include "PreferencesPanel.h"

#include "Lucky/Editor/EditorPreferences.h"

#include "Lucky/UI/PropertyGrid.h"
#include "Lucky/UI/Widgets.h"

#include <imgui/imgui.h>

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
        if (UI::BeginCollapsing("Gizmo Colors"))
        {
            UI::BeginPropertyGrid();
            
            changed |= UI::PropertyColor4("Grid X Axis", colors.GridAxisXColor);
            changed |= UI::PropertyColor4("Grid Z Axis", colors.GridAxisZColor);
            changed |= UI::PropertyColor4("Grid Lines", colors.GridLineColor);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Outline Leaf", colors.OutlineLeafColor);
            changed |= UI::PropertyColor4("Outline Parent", colors.OutlineParentColor);
            
            UI::EndPropertyGrid();
            
            UI::EndCollapsing();
        }
        
        ImGui::Separator();
        
        // ---- 视口颜色 ----
        if (UI::BeginCollapsing("Viewport"))
        {
            UI::BeginPropertyGrid();
            changed |= UI::PropertyColor4("Clear Color", colors.ViewportClearColor);
            UI::EndPropertyGrid();
            
            UI::EndCollapsing();
        }
        
        ImGui::Separator();
        
        // ---- UI 颜色 ----
        if (UI::BeginCollapsing("UI Colors"))
        {
            UI::BeginPropertyGrid();
            
            changed |= UI::PropertyColor4("Window Bg", colors.WindowBackground);
            changed |= UI::PropertyColor4("Child Bg", colors.ChildBackground);
            changed |= UI::PropertyColor4("Popup Bg", colors.PopupBackground);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Border", colors.BorderColor);
            changed |= UI::PropertyColor4("Border Shadow", colors.BorderShadowColor);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Frame Bg", colors.FrameBackground);
            changed |= UI::PropertyColor4("Frame Hovered", colors.FrameBackgroundHovered);
            changed |= UI::PropertyColor4("Frame Active", colors.FrameBackgroundActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Button", colors.ButtonColor);
            changed |= UI::PropertyColor4("Button Hovered", colors.ButtonHovered);
            changed |= UI::PropertyColor4("Button Active", colors.ButtonActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Header", colors.HeaderColor);
            changed |= UI::PropertyColor4("Header Hovered", colors.HeaderHovered);
            changed |= UI::PropertyColor4("Header Active", colors.HeaderActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Title Bar", colors.TitleBarBackground);
            changed |= UI::PropertyColor4("Menu Bar Bg", colors.MenuBarBackground);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Tab", colors.TabColor);
            changed |= UI::PropertyColor4("Tab Hovered", colors.TabHovered);
            changed |= UI::PropertyColor4("Tab Active", colors.TabActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Text", colors.TextColor);
            changed |= UI::PropertyColor4("Text Disabled", colors.TextDisabledColor);
            changed |= UI::PropertyColor4("Text Selected Bg", colors.TextSelectedBackground);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Check Mark", colors.CheckMarkColor);
            changed |= UI::PropertyColor4("Slider Grab", colors.SliderGrab);
            changed |= UI::PropertyColor4("Slider Grab Active", colors.SliderGrabActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Separator", colors.SeparatorColor);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Resize Grip", colors.ResizeGripColor);
            changed |= UI::PropertyColor4("Resize Grip Hovered", colors.ResizeGripHovered);
            changed |= UI::PropertyColor4("Resize Grip Active", colors.ResizeGripActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Scrollbar Bg", colors.ScrollbarBackground);
            changed |= UI::PropertyColor4("Scrollbar Grab", colors.ScrollbarGrab);
            changed |= UI::PropertyColor4("Scrollbar Grab Hovered", colors.ScrollbarGrabHovered);
            changed |= UI::PropertyColor4("Scrollbar Grab Active", colors.ScrollbarGrabActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Table Header Bg", colors.TableHeaderBg);
            changed |= UI::PropertyColor4("Table Border Strong", colors.TableBorderStrong);
            changed |= UI::PropertyColor4("Table Border Light", colors.TableBorderLight);
            changed |= UI::PropertyColor4("Table Row Bg", colors.TableRowBg);
            changed |= UI::PropertyColor4("Table Row Bg Alt", colors.TableRowBgAlt);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Drag Drop Target", colors.DragDropTarget);
            changed |= UI::PropertyColor4("Modal Window Dim Bg", colors.ModalWindowDimBackground);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor4("Docking Preview", colors.DockingPreview);
            changed |= UI::PropertyColor4("Docking Empty Bg", colors.DockingEmptyBackground);
            
            UI::EndPropertyGrid();
            
            UI::EndCollapsing();
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