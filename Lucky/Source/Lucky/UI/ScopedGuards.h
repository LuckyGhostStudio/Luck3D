#pragma once

#include <imgui/imgui.h>

namespace Lucky::UI
{
    /// <summary>
    /// RAII 样式变量管理：自动配对 PushStyleVar / PopStyleVar
    /// </summary>
    class ScopedStyle
    {
    public:
        ScopedStyle(ImGuiStyleVar idx, float val)
        {
            ImGui::PushStyleVar(idx, val);
        }

        ScopedStyle(ImGuiStyleVar idx, const ImVec2& val)
        {
            ImGui::PushStyleVar(idx, val);
        }

        ~ScopedStyle()
        {
            ImGui::PopStyleVar();
        }

        ScopedStyle(const ScopedStyle&) = delete;
        ScopedStyle& operator=(const ScopedStyle&) = delete;
    };

    /// <summary>
    /// RAII 颜色管理：自动配对 PushStyleColor / PopStyleColor
    /// </summary>
    class ScopedColor
    {
    public:
        ScopedColor(ImGuiCol idx, const ImVec4& color)
        {
            ImGui::PushStyleColor(idx, color);
        }

        ScopedColor(ImGuiCol idx, ImU32 color)
        {
            ImGui::PushStyleColor(idx, color);
        }

        ~ScopedColor()
        {
            ImGui::PopStyleColor();
        }

        ScopedColor(const ScopedColor&) = delete;
        ScopedColor& operator=(const ScopedColor&) = delete;
    };

    /// <summary>
    /// RAII ID 管理：自动配对 PushID / PopID
    /// </summary>
    class ScopedID
    {
    public:
        ScopedID(int id)
        {
            ImGui::PushID(id);
        }

        ScopedID(const char* id)
        {
            ImGui::PushID(id);
        }

        ScopedID(const void* id)
        {
            ImGui::PushID(id);
        }

        ~ScopedID()
        {
            ImGui::PopID();
        }

        ScopedID(const ScopedID&) = delete;
        ScopedID& operator=(const ScopedID&) = delete;
    };

    /// <summary>
    /// RAII 字体管理：自动配对 PushFont / PopFont
    /// </summary>
    class ScopedFont
    {
    public:
        ScopedFont(ImFont* font)
        {
            ImGui::PushFont(font);
        }

        ~ScopedFont()
        {
            ImGui::PopFont();
        }

        ScopedFont(const ScopedFont&) = delete;
        ScopedFont& operator=(const ScopedFont&) = delete;
    };

    /// <summary>
    /// RAII 禁用状态管理：自动配对 BeginDisabled / EndDisabled
    /// 当 disabled = true 时，控件变灰且不可交互
    /// </summary>
    class ScopedDisable
    {
    public:
        ScopedDisable(bool disabled = true)
            : m_Disabled(disabled)
        {
            if (m_Disabled)
            {
                ImGui::BeginDisabled(true);
            }
        }

        ~ScopedDisable()
        {
            if (m_Disabled)
            {
                ImGui::EndDisabled();
            }
        }

        ScopedDisable(const ScopedDisable&) = delete;
        ScopedDisable& operator=(const ScopedDisable&) = delete;

    private:
        bool m_Disabled;
    };
    
    /// <summary>
    /// RAII 批量颜色管理：一次 Push 多个颜色，析构时统一 Pop
    /// 参数必须成对传入：(ImGuiCol, color, ImGuiCol, color, ...)
    /// </summary>
    class ScopedColorStack
    {
    public:
        ScopedColorStack(const ScopedColorStack&) = delete;
        ScopedColorStack& operator=(const ScopedColorStack&) = delete;

        template<typename ColorType, typename... OtherColors>
        ScopedColorStack(ImGuiCol firstColorID, ColorType firstColor, OtherColors&&... otherColorPairs)
            : m_Count((sizeof...(otherColorPairs) / 2) + 1)
        {
            static_assert((sizeof...(otherColorPairs) & 1u) == 0, "ScopedStyleStack Parameters must be passed in pairs: (ImGuiStyleVar, value) pairs");

            PushColor(firstColorID, firstColor, std::forward<OtherColors>(otherColorPairs)...);
        }

        ~ScopedColorStack()
        {
            ImGui::PopStyleColor(m_Count);
        }

    private:
        int m_Count;

        template<typename ColorType, typename... OtherColors>
        void PushColor(ImGuiCol colorID, ColorType color, OtherColors&&... otherColorPairs)
        {
            if constexpr (sizeof...(otherColorPairs) == 0)
            {
                ImGui::PushStyleColor(colorID, ImColor(color).Value);
            }
            else
            {
                ImGui::PushStyleColor(colorID, ImColor(color).Value);
                PushColor(std::forward<OtherColors>(otherColorPairs)...);
            }
        }
    };

    /// <summary>
    /// RAII 批量样式管理：一次 Push 多个样式变量，析构时统一 Pop
    /// 参数必须成对传入：(ImGuiStyleVar, value, ImGuiStyleVar, value, ...)
    /// </summary>
    class ScopedStyleStack
    {
    public:
        ScopedStyleStack(const ScopedStyleStack&) = delete;
        ScopedStyleStack& operator=(const ScopedStyleStack&) = delete;

        template<typename ValueType, typename... OtherStylePairs>
        ScopedStyleStack(ImGuiStyleVar firstStyleVar, ValueType firstValue, OtherStylePairs&&... otherStylePairs)
            : m_Count((sizeof...(otherStylePairs) / 2) + 1)
        {
            static_assert((sizeof...(otherStylePairs) & 1u) == 0, "ScopedStyleStack Parameters must be passed in pairs: (ImGuiStyleVar, value) pairs");

            PushStyle(firstStyleVar, firstValue, std::forward<OtherStylePairs>(otherStylePairs)...);
        }

        ~ScopedStyleStack()
        {
            ImGui::PopStyleVar(m_Count);
        }

    private:
        int m_Count;

        template<typename ValueType, typename... OtherStylePairs>
        void PushStyle(ImGuiStyleVar styleVar, ValueType value, OtherStylePairs&&... otherStylePairs)
        {
            if constexpr (sizeof...(otherStylePairs) == 0)
            {
                ImGui::PushStyleVar(styleVar, value);
            }
            else
            {
                ImGui::PushStyleVar(styleVar, value);
                PushStyle(std::forward<OtherStylePairs>(otherStylePairs)...);
            }
        }
    };
}