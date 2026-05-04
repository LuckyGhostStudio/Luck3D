#include "lcpch.h"
#include "Widgets.h"

#include "ScopedGuards.h"
#include "Theme.h"
#include "UICore.h"

namespace Lucky::UI
{
    /// <summary>
    /// 将 Texture2D 转换为 ImTextureID
    /// </summary>
    static ImTextureID GetImTextureID(const Ref<Texture2D>& texture)
    {
        if (!texture)
        {
            return nullptr;
        }

        return reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(texture->GetRendererID()));
    }
    
    bool BeginCollapsing(const char* label, bool defaultOpen)
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (defaultOpen)
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        
        ScopedStyle frameRounding(ImGuiStyleVar_FrameRounding, 0.0f);
        ScopedStyle framePadding(ImGuiStyleVar_FramePadding, { 6.0f, 6.0f });

        // TODO Font 粗体
        
        UI::ShiftCursorY(4.0f);   // 向下偏移，增加与上方内容的间距
        return ImGui::TreeNodeEx(label, flags);
    }
    
    void EndCollapsing()
    {
        ImGui::TreePop();
    }
    
    bool BeginTreeNode(const char* name, bool defaultOpen, bool selected, bool isLeaf)
    {
        // TODO 图标
        
        ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, { 0, 0 });
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanAvailWidth;

        ImVec4 color = { 0.4f, 0.4f, 0.4f, 1.0f };
        ImVec4 hoveredColor = { 0.4f, 0.4f, 0.4f, 0.6f };
        
        if (defaultOpen)
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        
        if (selected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
            
            color = { 0.2f, 0.302f, 0.452f, 1.0f };         // 蓝色
            hoveredColor = { 0.2f, 0.302f, 0.502f, 1.0f };
        }
        else
        {
            color = { 0.4f, 0.4f, 0.4f, 1.0f };
            hoveredColor = { 0.4f, 0.4f, 0.4f, 0.6f };
        }
        
        if (isLeaf)
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

        ScopedColor headerColor(ImGuiCol_Header, color);
        ScopedColor headerHoveredColor(ImGuiCol_HeaderHovered, hoveredColor);
        return ImGui::TreeNodeEx(name, flags);
    }
    
    void EndTreeNode()
    {
        ImGui::TreePop();
    }
    
    void Image(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tintColor, const ImVec4& borderColor)
    {
        ImTextureID texID = GetImTextureID(texture);
        if (texID)
        {
            ImGui::Image(texID, size, uv0, uv1, tintColor, borderColor);
        }
    }

    void ImageFlipped(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec4& tintColor, const ImVec4& borderColor)
    {
        // OpenGL Y 翻转：UV 从 {0,1} 到 {1,0}
        Image(texture, size, ImVec2(0, 1), ImVec2(1, 0), tintColor, borderColor);
    }
    
    // ---- Popup ----
    
    bool BeginPopupContextWindow(const char* strID, ImGuiPopupFlags popupFlags)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { Theme::Layout::WindowPaddingX, Theme::Layout::WindowPaddingX });
        bool opened = ImGui::BeginPopupContextWindow(strID, popupFlags);
        ImGui::PopStyleVar();
        
        if (opened)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { Theme::Layout::WindowPaddingX, Theme::Layout::WindowPaddingX });
        }
        return opened;
    }
    
    bool BeginPopupContextItem(const char* strID, ImGuiPopupFlags popupFlags)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { Theme::Layout::WindowPaddingX, Theme::Layout::WindowPaddingX });
        bool opened = ImGui::BeginPopupContextItem(strID, popupFlags);
        ImGui::PopStyleVar();
        
        if (opened)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { Theme::Layout::WindowPaddingX, Theme::Layout::WindowPaddingX });
        }
        return opened;
    }
    
    bool BeginPopup(const char* strID, ImGuiPopupFlags popupFlags)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { Theme::Layout::WindowPaddingX, Theme::Layout::WindowPaddingX });
        bool opened = ImGui::BeginPopup(strID, popupFlags);
        ImGui::PopStyleVar();
        
        if (opened)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { Theme::Layout::WindowPaddingX, Theme::Layout::WindowPaddingX });
        }
        return opened;
    }
    
    void EndPopup()
    {
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }
}
