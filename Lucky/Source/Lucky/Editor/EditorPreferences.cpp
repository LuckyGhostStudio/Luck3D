#include "lcpch.h"
#include "EditorPreferences.h"

#include "Lucky/Serialization/YamlHelpers.h"

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>

namespace Lucky
{
    EditorPreferences& EditorPreferences::Get()
    {
        static EditorPreferences instance;
        return instance;
    }
    
    void EditorPreferences::ResetColorsToDefault()
    {
        m_Colors = ColorSettings();  // 使用默认构造（所有字段回到默认值）
    }
    
    void EditorPreferences::ApplyImGuiColors()
    {
        auto& colors = ImGui::GetStyle().Colors;
        const ColorSettings& c = m_Colors;
        
        // 辅助 lambda：glm::vec4 → ImVec4
        auto ToImVec4 = [](const glm::vec4& v) -> ImVec4 {
            return ImVec4(v.r, v.g, v.b, v.a);
        };
        
        // ---- 窗口背景 ----
        colors[ImGuiCol_WindowBg]               = ToImVec4(c.WindowBackground);
        colors[ImGuiCol_ChildBg]                = ToImVec4(c.ChildBackground);
        colors[ImGuiCol_PopupBg]                = ToImVec4(c.PopupBackground);
        
        // ---- 边框 ----
        colors[ImGuiCol_Border]                 = ToImVec4(c.BorderColor);
        colors[ImGuiCol_BorderShadow]           = ToImVec4(c.BorderShadowColor);
        
        // ---- 控件背景 ----
        colors[ImGuiCol_FrameBg]                = ToImVec4(c.FrameBackground);
        colors[ImGuiCol_FrameBgHovered]         = ToImVec4(c.FrameBackgroundHovered);
        colors[ImGuiCol_FrameBgActive]          = ToImVec4(c.FrameBackgroundActive);
        
        // ---- 按钮 ----
        colors[ImGuiCol_Button]                 = ToImVec4(c.ButtonColor);
        colors[ImGuiCol_ButtonHovered]          = ToImVec4(c.ButtonHovered);
        colors[ImGuiCol_ButtonActive]           = ToImVec4(c.ButtonActive);
        
        // ---- 标题/组件头 ----
        colors[ImGuiCol_Header]                 = ToImVec4(c.HeaderColor);
        colors[ImGuiCol_HeaderHovered]          = ToImVec4(c.HeaderHovered);
        colors[ImGuiCol_HeaderActive]           = ToImVec4(c.HeaderActive);
        
        // ---- 标题栏 ----
        colors[ImGuiCol_TitleBg]                = ToImVec4(c.TitleBarBackground);
        colors[ImGuiCol_TitleBgActive]          = ToImVec4(c.TitleBarBackground);
        colors[ImGuiCol_TitleBgCollapsed]       = ToImVec4(c.TitleBarBackground);
        
        // ---- 菜单栏 ----
        colors[ImGuiCol_MenuBarBg]              = ToImVec4(c.MenuBarBackground);
        
        // ---- Tab ----
        colors[ImGuiCol_Tab]                    = ToImVec4(c.TabColor);
        colors[ImGuiCol_TabHovered]             = ToImVec4(c.TabHovered);
        colors[ImGuiCol_TabActive]              = ToImVec4(c.TabActive);
        colors[ImGuiCol_TabUnfocused]           = ToImVec4(c.TabColor);
        colors[ImGuiCol_TabUnfocusedActive]     = ToImVec4(c.TabActive);
        
        // ---- 文字 ----
        colors[ImGuiCol_Text]                   = ToImVec4(c.TextColor);
        colors[ImGuiCol_TextDisabled]           = ToImVec4(c.TextDisabledColor);
        colors[ImGuiCol_TextSelectedBg]         = ToImVec4(c.TextSelectedBackground);
        
        // ---- 复选框 / 滑块 ----
        colors[ImGuiCol_CheckMark]              = ToImVec4(c.CheckMarkColor);
        colors[ImGuiCol_SliderGrab]             = ToImVec4(c.SliderGrab);
        colors[ImGuiCol_SliderGrabActive]       = ToImVec4(c.SliderGrabActive);
        
        // ---- 分隔线 ----
        colors[ImGuiCol_Separator]              = ToImVec4(c.SeparatorColor);
        colors[ImGuiCol_SeparatorHovered]       = ToImVec4(c.SeparatorColor);
        colors[ImGuiCol_SeparatorActive]        = ToImVec4(c.SeparatorColor);
        
        // ---- 缩放手柄 ----
        colors[ImGuiCol_ResizeGrip]             = ToImVec4(c.ResizeGripColor);
        colors[ImGuiCol_ResizeGripHovered]      = ToImVec4(c.ResizeGripHovered);
        colors[ImGuiCol_ResizeGripActive]       = ToImVec4(c.ResizeGripActive);
        
        // ---- 滚动条 ----
        colors[ImGuiCol_ScrollbarBg]            = ToImVec4(c.ScrollbarBackground);
        colors[ImGuiCol_ScrollbarGrab]          = ToImVec4(c.ScrollbarGrab);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ToImVec4(c.ScrollbarGrabHovered);
        colors[ImGuiCol_ScrollbarGrabActive]    = ToImVec4(c.ScrollbarGrabActive);
        
        // ---- 表格 ----
        colors[ImGuiCol_TableHeaderBg]          = ToImVec4(c.TableHeaderBg);
        colors[ImGuiCol_TableBorderStrong]      = ToImVec4(c.TableBorderStrong);
        colors[ImGuiCol_TableBorderLight]       = ToImVec4(c.TableBorderLight);
        colors[ImGuiCol_TableRowBg]             = ToImVec4(c.TableRowBg);
        colors[ImGuiCol_TableRowBgAlt]          = ToImVec4(c.TableRowBgAlt);
        
        // ---- 拖放 ----
        colors[ImGuiCol_DragDropTarget]         = ToImVec4(c.DragDropTarget);
        
        // ---- 模态窗口 ----
        colors[ImGuiCol_ModalWindowDimBg]       = ToImVec4(c.ModalWindowDimBackground);
        
        // ---- Docking ----
        colors[ImGuiCol_DockingPreview]         = ToImVec4(c.DockingPreview);
        colors[ImGuiCol_DockingEmptyBg]         = ToImVec4(c.DockingEmptyBackground);
    }
    
    void EditorPreferences::Save(const std::string& filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        
        out << YAML::Key << "Colors" << YAML::Value << YAML::BeginMap;
        {
            // Gizmo
            out << YAML::Key << "Gizmo" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "GridAxisXColor" << YAML::Value << m_Colors.GridAxisXColor;
            out << YAML::Key << "GridAxisZColor" << YAML::Value << m_Colors.GridAxisZColor;
            out << YAML::Key << "GridLineColor" << YAML::Value << m_Colors.GridLineColor;
            out << YAML::Key << "OutlineLeafColor" << YAML::Value << m_Colors.OutlineLeafColor;
            out << YAML::Key << "OutlineParentColor" << YAML::Value << m_Colors.OutlineParentColor;
            out << YAML::EndMap;
            
            // Viewport
            out << YAML::Key << "Viewport" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "ClearColor" << YAML::Value << m_Colors.ViewportClearColor;
            out << YAML::EndMap;
            
            // UI
            out << YAML::Key << "UI" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "WindowBackground" << YAML::Value << m_Colors.WindowBackground;
            out << YAML::Key << "ChildBackground" << YAML::Value << m_Colors.ChildBackground;
            out << YAML::Key << "PopupBackground" << YAML::Value << m_Colors.PopupBackground;
            out << YAML::Key << "BorderColor" << YAML::Value << m_Colors.BorderColor;
            out << YAML::Key << "BorderShadowColor" << YAML::Value << m_Colors.BorderShadowColor;
            out << YAML::Key << "FrameBackground" << YAML::Value << m_Colors.FrameBackground;
            out << YAML::Key << "FrameBackgroundHovered" << YAML::Value << m_Colors.FrameBackgroundHovered;
            out << YAML::Key << "FrameBackgroundActive" << YAML::Value << m_Colors.FrameBackgroundActive;
            out << YAML::Key << "ButtonColor" << YAML::Value << m_Colors.ButtonColor;
            out << YAML::Key << "ButtonHovered" << YAML::Value << m_Colors.ButtonHovered;
            out << YAML::Key << "ButtonActive" << YAML::Value << m_Colors.ButtonActive;
            out << YAML::Key << "HeaderColor" << YAML::Value << m_Colors.HeaderColor;
            out << YAML::Key << "HeaderHovered" << YAML::Value << m_Colors.HeaderHovered;
            out << YAML::Key << "HeaderActive" << YAML::Value << m_Colors.HeaderActive;
            out << YAML::Key << "TitleBarBackground" << YAML::Value << m_Colors.TitleBarBackground;
            out << YAML::Key << "MenuBarBackground" << YAML::Value << m_Colors.MenuBarBackground;
            out << YAML::Key << "TabColor" << YAML::Value << m_Colors.TabColor;
            out << YAML::Key << "TabHovered" << YAML::Value << m_Colors.TabHovered;
            out << YAML::Key << "TabActive" << YAML::Value << m_Colors.TabActive;
            out << YAML::Key << "TextColor" << YAML::Value << m_Colors.TextColor;
            out << YAML::Key << "TextDisabledColor" << YAML::Value << m_Colors.TextDisabledColor;
            out << YAML::Key << "TextSelectedBackground" << YAML::Value << m_Colors.TextSelectedBackground;
            out << YAML::Key << "CheckMarkColor" << YAML::Value << m_Colors.CheckMarkColor;
            out << YAML::Key << "SliderGrab" << YAML::Value << m_Colors.SliderGrab;
            out << YAML::Key << "SliderGrabActive" << YAML::Value << m_Colors.SliderGrabActive;
            out << YAML::Key << "SeparatorColor" << YAML::Value << m_Colors.SeparatorColor;
            out << YAML::Key << "ResizeGripColor" << YAML::Value << m_Colors.ResizeGripColor;
            out << YAML::Key << "ResizeGripHovered" << YAML::Value << m_Colors.ResizeGripHovered;
            out << YAML::Key << "ResizeGripActive" << YAML::Value << m_Colors.ResizeGripActive;
            out << YAML::Key << "ScrollbarBackground" << YAML::Value << m_Colors.ScrollbarBackground;
            out << YAML::Key << "ScrollbarGrab" << YAML::Value << m_Colors.ScrollbarGrab;
            out << YAML::Key << "ScrollbarGrabHovered" << YAML::Value << m_Colors.ScrollbarGrabHovered;
            out << YAML::Key << "ScrollbarGrabActive" << YAML::Value << m_Colors.ScrollbarGrabActive;
            out << YAML::Key << "TableHeaderBg" << YAML::Value << m_Colors.TableHeaderBg;
            out << YAML::Key << "TableBorderStrong" << YAML::Value << m_Colors.TableBorderStrong;
            out << YAML::Key << "TableBorderLight" << YAML::Value << m_Colors.TableBorderLight;
            out << YAML::Key << "TableRowBg" << YAML::Value << m_Colors.TableRowBg;
            out << YAML::Key << "TableRowBgAlt" << YAML::Value << m_Colors.TableRowBgAlt;
            out << YAML::Key << "DragDropTarget" << YAML::Value << m_Colors.DragDropTarget;
            out << YAML::Key << "ModalWindowDimBackground" << YAML::Value << m_Colors.ModalWindowDimBackground;
            out << YAML::Key << "DockingPreview" << YAML::Value << m_Colors.DockingPreview;
            out << YAML::Key << "DockingEmptyBackground" << YAML::Value << m_Colors.DockingEmptyBackground;
            out << YAML::EndMap;
        }
        out << YAML::EndMap;
        
        out << YAML::EndMap;
        
        std::ofstream fout(filepath);
        fout << out.c_str();
    }
    
    bool EditorPreferences::Load(const std::string& filepath)
    {
        std::ifstream stream(filepath);
        if (!stream.good())
        {
            LF_CORE_WARN("Preferences file not found: {0}, using defaults.", filepath);
            return false;
        }
        
        YAML::Node data = YAML::LoadFile(filepath);
        
        if (!data["Colors"])
        {
            return false;
        }
        
        auto colorsNode = data["Colors"];
        
        // 辅助 lambda：安全读取 vec4
        auto ReadVec4 = [](const YAML::Node& node, const std::string& key, glm::vec4& target)
        {
            if (node[key])
            {
                target = node[key].as<glm::vec4>();
            }
        };
        
        // Gizmo
        if (colorsNode["Gizmo"])
        {
            auto gizmo = colorsNode["Gizmo"];
            ReadVec4(gizmo, "GridAxisXColor", m_Colors.GridAxisXColor);
            ReadVec4(gizmo, "GridAxisZColor", m_Colors.GridAxisZColor);
            ReadVec4(gizmo, "GridLineColor", m_Colors.GridLineColor);
            ReadVec4(gizmo, "OutlineLeafColor", m_Colors.OutlineLeafColor);
            ReadVec4(gizmo, "OutlineParentColor", m_Colors.OutlineParentColor);
        }
        
        // Viewport
        if (colorsNode["Viewport"])
        {
            auto viewport = colorsNode["Viewport"];
            ReadVec4(viewport, "ClearColor", m_Colors.ViewportClearColor);
        }
        
        // UI
        if (colorsNode["UI"])
        {
            auto ui = colorsNode["UI"];
            ReadVec4(ui, "WindowBackground", m_Colors.WindowBackground);
            ReadVec4(ui, "ChildBackground", m_Colors.ChildBackground);
            ReadVec4(ui, "PopupBackground", m_Colors.PopupBackground);
            ReadVec4(ui, "BorderColor", m_Colors.BorderColor);
            ReadVec4(ui, "BorderShadowColor", m_Colors.BorderShadowColor);
            ReadVec4(ui, "FrameBackground", m_Colors.FrameBackground);
            ReadVec4(ui, "FrameBackgroundHovered", m_Colors.FrameBackgroundHovered);
            ReadVec4(ui, "FrameBackgroundActive", m_Colors.FrameBackgroundActive);
            ReadVec4(ui, "ButtonColor", m_Colors.ButtonColor);
            ReadVec4(ui, "ButtonHovered", m_Colors.ButtonHovered);
            ReadVec4(ui, "ButtonActive", m_Colors.ButtonActive);
            ReadVec4(ui, "HeaderColor", m_Colors.HeaderColor);
            ReadVec4(ui, "HeaderHovered", m_Colors.HeaderHovered);
            ReadVec4(ui, "HeaderActive", m_Colors.HeaderActive);
            ReadVec4(ui, "TitleBarBackground", m_Colors.TitleBarBackground);
            ReadVec4(ui, "MenuBarBackground", m_Colors.MenuBarBackground);
            ReadVec4(ui, "TabColor", m_Colors.TabColor);
            ReadVec4(ui, "TabHovered", m_Colors.TabHovered);
            ReadVec4(ui, "TabActive", m_Colors.TabActive);
            ReadVec4(ui, "TextColor", m_Colors.TextColor);
            ReadVec4(ui, "TextDisabledColor", m_Colors.TextDisabledColor);
            ReadVec4(ui, "TextSelectedBackground", m_Colors.TextSelectedBackground);
            ReadVec4(ui, "CheckMarkColor", m_Colors.CheckMarkColor);
            ReadVec4(ui, "SliderGrab", m_Colors.SliderGrab);
            ReadVec4(ui, "SliderGrabActive", m_Colors.SliderGrabActive);
            ReadVec4(ui, "SeparatorColor", m_Colors.SeparatorColor);
            ReadVec4(ui, "ResizeGripColor", m_Colors.ResizeGripColor);
            ReadVec4(ui, "ResizeGripHovered", m_Colors.ResizeGripHovered);
            ReadVec4(ui, "ResizeGripActive", m_Colors.ResizeGripActive);
            ReadVec4(ui, "ScrollbarBackground", m_Colors.ScrollbarBackground);
            ReadVec4(ui, "ScrollbarGrab", m_Colors.ScrollbarGrab);
            ReadVec4(ui, "ScrollbarGrabHovered", m_Colors.ScrollbarGrabHovered);
            ReadVec4(ui, "ScrollbarGrabActive", m_Colors.ScrollbarGrabActive);
            ReadVec4(ui, "TableHeaderBg", m_Colors.TableHeaderBg);
            ReadVec4(ui, "TableBorderStrong", m_Colors.TableBorderStrong);
            ReadVec4(ui, "TableBorderLight", m_Colors.TableBorderLight);
            ReadVec4(ui, "TableRowBg", m_Colors.TableRowBg);
            ReadVec4(ui, "TableRowBgAlt", m_Colors.TableRowBgAlt);
            ReadVec4(ui, "DragDropTarget", m_Colors.DragDropTarget);
            ReadVec4(ui, "ModalWindowDimBackground", m_Colors.ModalWindowDimBackground);
            ReadVec4(ui, "DockingPreview", m_Colors.DockingPreview);
            ReadVec4(ui, "DockingEmptyBackground", m_Colors.DockingEmptyBackground);
        }
        
        LF_CORE_INFO("Preferences loaded from: {0}", filepath);
        return true;
    }
}