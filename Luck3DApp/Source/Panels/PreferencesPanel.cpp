#include "PreferencesPanel.h"

#include "Lucky/Editor/EditorPreferences.h"

#include "Lucky/UI/Controls.h"
#include "Lucky/UI/PropertyGrid.h"
#include "Lucky/UI/Widgets.h"
#include "Lucky/UI/DrawUtils.h"
#include "Lucky/UI/UICore.h"
#include "Lucky/UI/Theme.h"

#include <imgui/imgui.h>


namespace Lucky
{
    PreferencesPanel::PreferencesPanel()
    {
        SetFlags(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);    // 禁用滚动条
    }

    void PreferencesPanel::OnUpdate(DeltaTime dt)
    {

    }

    void PreferencesPanel::OnGUI()
    {
        if (ImGui::BeginTable("##Preferences Table", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadInnerX))
        {
            float panelWidth = ImGui::GetContentRegionAvail().x;
            float categoriesWidth = panelWidth * 0.3f;
            ImGui::TableSetupColumn("Categories Column", 0, categoriesWidth);
            ImGui::TableSetupColumn("Content Column", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            // ---- 左侧分类列表 ----
            ImGui::BeginChild("Categories", ImVec2(0, 0));
            {
                // TODO X 对齐比例有点问题
                UI::ScopedStyle textAlign(ImGuiStyleVar_SelectableTextAlign, { UI::Theme::Layout::FramePaddingX / categoriesWidth, 0 });
                if (ImGui::Selectable("Colors", m_SelectedCategory == 0))
                {
                    m_SelectedCategory = 0;
                }

                // TODO 其他分类
            }
            ImGui::EndChild();

            // TODO 分割线拖动到最右边会卡住
            ImGui::TableSetColumnIndex(1);

            // ---- 右侧设置内容 ----
            ImGui::BeginChild("Content", ImVec2(0, 0));
            {
                switch (m_SelectedCategory)
                {
                case 0:
                    DrawColorsPage();
                    break;
                }
            }
            ImGui::EndChild();

            ImGui::EndTable();
        }
    }

    void PreferencesPanel::OnEvent(Event& event)
    {

    }
    
    void PreferencesPanel::DrawColorsPage()
    {
        ColorSettings& colors = EditorPreferences::Get().GetColors();
        bool changed = false;
        
        // ---- Gizmo 颜色 ----
        if (UI::BeginCollapsing("Gizmo Colors"))
        {
            changed |= UI::PropertyColor("Grid X Axis", colors.GridAxisXColor);
            changed |= UI::PropertyColor("Grid Z Axis", colors.GridAxisZColor);
            changed |= UI::PropertyColor("Grid Lines", colors.GridLineColor);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Outline Leaf", colors.OutlineLeafColor);
            changed |= UI::PropertyColor("Outline Parent", colors.OutlineParentColor);
            
            UI::EndCollapsing();
        }
        
        UI::Draw::HorizontalLine();
        
        // ---- 视口颜色 ----
        if (UI::BeginCollapsing("Viewport"))
        {
            changed |= UI::PropertyColor("Clear Color", colors.ViewportClearColor);
            
            UI::EndCollapsing();
        }
        
        UI::Draw::HorizontalLine();
        
        // ---- UI 颜色 ----
        if (UI::BeginCollapsing("UI Colors"))
        {
            changed |= UI::PropertyColor("Window Bg", colors.WindowBackground);
            changed |= UI::PropertyColor("Child Bg", colors.ChildBackground);
            changed |= UI::PropertyColor("Popup Bg", colors.PopupBackground);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Border", colors.BorderColor);
            changed |= UI::PropertyColor("Border Shadow", colors.BorderShadowColor);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Frame Bg", colors.FrameBackground);
            changed |= UI::PropertyColor("Frame Hovered", colors.FrameBackgroundHovered);
            changed |= UI::PropertyColor("Frame Active", colors.FrameBackgroundActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Button", colors.ButtonColor);
            changed |= UI::PropertyColor("Button Hovered", colors.ButtonHovered);
            changed |= UI::PropertyColor("Button Active", colors.ButtonActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Header", colors.HeaderColor);
            changed |= UI::PropertyColor("Header Hovered", colors.HeaderHovered);
            changed |= UI::PropertyColor("Header Active", colors.HeaderActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Title Bar", colors.TitleBarBackground);
            changed |= UI::PropertyColor("Menu Bar Bg", colors.MenuBarBackground);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Tab", colors.TabColor);
            changed |= UI::PropertyColor("Tab Hovered", colors.TabHovered);
            changed |= UI::PropertyColor("Tab Active", colors.TabActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Text", colors.TextColor);
            changed |= UI::PropertyColor("Text Disabled", colors.TextDisabledColor);
            changed |= UI::PropertyColor("Text Selected Bg", colors.TextSelectedBackground);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Check Mark", colors.CheckMarkColor);
            changed |= UI::PropertyColor("Slider Grab", colors.SliderGrab);
            changed |= UI::PropertyColor("Slider Grab Active", colors.SliderGrabActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Separator", colors.SeparatorColor);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Resize Grip", colors.ResizeGripColor);
            changed |= UI::PropertyColor("Resize Grip Hovered", colors.ResizeGripHovered);
            changed |= UI::PropertyColor("Resize Grip Active", colors.ResizeGripActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Scrollbar Bg", colors.ScrollbarBackground);
            changed |= UI::PropertyColor("Scrollbar Grab", colors.ScrollbarGrab);
            changed |= UI::PropertyColor("Scrollbar Grab Hovered", colors.ScrollbarGrabHovered);
            changed |= UI::PropertyColor("Scrollbar Grab Active", colors.ScrollbarGrabActive);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Table Header Bg", colors.TableHeaderBg);
            changed |= UI::PropertyColor("Table Border Strong", colors.TableBorderStrong);
            changed |= UI::PropertyColor("Table Border Light", colors.TableBorderLight);
            changed |= UI::PropertyColor("Table Row Bg", colors.TableRowBg);
            changed |= UI::PropertyColor("Table Row Bg Alt", colors.TableRowBgAlt);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Drag Drop Target", colors.DragDropTarget);
            changed |= UI::PropertyColor("Modal Window Dim Bg", colors.ModalWindowDimBackground);
            
            ImGui::Spacing();
            
            changed |= UI::PropertyColor("Docking Preview", colors.DockingPreview);
            changed |= UI::PropertyColor("Docking Empty Bg", colors.DockingEmptyBackground);
            
            UI::EndCollapsing();
        }
        
        // 颜色改变时立即应用到 ImGui 主题
        if (changed)
        {
            EditorPreferences::Get().ApplyImGuiColors();
        }
        
        UI::Draw::HorizontalLine();
        ImGui::Spacing();
        
        UI::ShiftCursorX(UI::Theme::Layout::WindowPaddingX);
        
        // 恢复默认设置
        if (UI::Button("Reset"))
        {
            EditorPreferences::Get().ResetColorsToDefault();
            EditorPreferences::Get().ApplyImGuiColors();
        }
        
        ImGui::SameLine();
        
        // 保存设置
        if (UI::Button("Save"))
        {
            EditorPreferences::Get().Save();
        }
        
        ImGui::Spacing();
    }
}
