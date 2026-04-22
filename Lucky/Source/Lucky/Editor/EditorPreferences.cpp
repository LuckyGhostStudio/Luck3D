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
        
        // ---- Tab ----
        colors[ImGuiCol_Tab]                    = ToImVec4(c.TabColor);
        colors[ImGuiCol_TabHovered]             = ToImVec4(c.TabHovered);
        colors[ImGuiCol_TabActive]              = ToImVec4(c.TabActive);
        colors[ImGuiCol_TabUnfocused]           = ToImVec4(c.TabColor);
        colors[ImGuiCol_TabUnfocusedActive]     = ToImVec4(c.TabActive);
        
        // ---- 文字 ----
        colors[ImGuiCol_Text]                   = ToImVec4(c.TextColor);
        colors[ImGuiCol_TextDisabled]           = ToImVec4(c.TextDisabledColor);
        
        // ---- 分隔线 ----
        colors[ImGuiCol_Separator]              = ToImVec4(c.SeparatorColor);
        colors[ImGuiCol_SeparatorHovered]       = ToImVec4(c.SeparatorColor);
        colors[ImGuiCol_SeparatorActive]        = ToImVec4(c.SeparatorColor);
        
        // ---- 滚动条 ----
        colors[ImGuiCol_ScrollbarBg]            = ToImVec4(c.ScrollbarBackground);
        colors[ImGuiCol_ScrollbarGrab]          = ToImVec4(c.ScrollbarGrab);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ToImVec4(c.ScrollbarGrabHovered);
        colors[ImGuiCol_ScrollbarGrabActive]    = ToImVec4(c.ScrollbarGrabActive);
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
            out << YAML::EndMap;
            
            // Viewport
            out << YAML::Key << "Viewport" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "ClearColor" << YAML::Value << m_Colors.ViewportClearColor;
            out << YAML::EndMap;
            
            // UI
            out << YAML::Key << "UI" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "WindowBackground" << YAML::Value << m_Colors.WindowBackground;
            out << YAML::Key << "ChildBackground" << YAML::Value << m_Colors.ChildBackground;
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
            out << YAML::Key << "TabColor" << YAML::Value << m_Colors.TabColor;
            out << YAML::Key << "TabHovered" << YAML::Value << m_Colors.TabHovered;
            out << YAML::Key << "TabActive" << YAML::Value << m_Colors.TabActive;
            out << YAML::Key << "TextColor" << YAML::Value << m_Colors.TextColor;
            out << YAML::Key << "TextDisabledColor" << YAML::Value << m_Colors.TextDisabledColor;
            out << YAML::Key << "SeparatorColor" << YAML::Value << m_Colors.SeparatorColor;
            out << YAML::Key << "ScrollbarBackground" << YAML::Value << m_Colors.ScrollbarBackground;
            out << YAML::Key << "ScrollbarGrab" << YAML::Value << m_Colors.ScrollbarGrab;
            out << YAML::Key << "ScrollbarGrabHovered" << YAML::Value << m_Colors.ScrollbarGrabHovered;
            out << YAML::Key << "ScrollbarGrabActive" << YAML::Value << m_Colors.ScrollbarGrabActive;
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
            ReadVec4(ui, "TabColor", m_Colors.TabColor);
            ReadVec4(ui, "TabHovered", m_Colors.TabHovered);
            ReadVec4(ui, "TabActive", m_Colors.TabActive);
            ReadVec4(ui, "TextColor", m_Colors.TextColor);
            ReadVec4(ui, "TextDisabledColor", m_Colors.TextDisabledColor);
            ReadVec4(ui, "SeparatorColor", m_Colors.SeparatorColor);
            ReadVec4(ui, "ScrollbarBackground", m_Colors.ScrollbarBackground);
            ReadVec4(ui, "ScrollbarGrab", m_Colors.ScrollbarGrab);
            ReadVec4(ui, "ScrollbarGrabHovered", m_Colors.ScrollbarGrabHovered);
            ReadVec4(ui, "ScrollbarGrabActive", m_Colors.ScrollbarGrabActive);
        }
        
        LF_CORE_INFO("Preferences loaded from: {0}", filepath);
        return true;
    }
}