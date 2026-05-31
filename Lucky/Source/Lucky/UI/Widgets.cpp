#include "lcpch.h"
#include "Widgets.h"

#include "DrawUtils.h"
#include "ScopedGuards.h"
#include "Theme.h"
#include "UICore.h"

#include <imgui/imgui_internal.h>

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
    
    bool BeginPrimaryCollapsing(const char* label)
    {
        // 树节点标志：打开|框架|延伸到右边|框架边框
        const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;
        
        // 生成唯一 ID
        const std::string& strID = std::format("{}", label);
        
        bool opened = false;
        
        Draw::HorizontalLine();
        
        ShiftCursorY(1.0f);
        {
            ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, { 0, 0 });   // 树节点和底部水平线之间的 Spacing
            opened = ImGui::TreeNodeEx(strID.c_str(), flags, "");
            
            // 组件名
            ImGui::SameLine();
            ShiftCursorX(Theme::Layout::ComponentHeaderIconSpacing);
            {
                ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[0]);    // TODO 封装 Fonts
                ImGui::TextUnformatted(label);
            }
        
            ImGui::Indent(-Theme::Layout::IndentSpacing);
            Draw::HorizontalLine(0.6f);
            ImGui::Indent(Theme::Layout::IndentSpacing);
        }
        
        return opened;
    }
    
    void EndPrimaryCollapsing()
    {
        ImGui::TreePop();
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
        
        ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[0]);    // TODO 封装 Fonts
        
        ShiftCursorY(Theme::Layout::ItemSpacingY);   // 向下偏移，增加与上方内容的间距
        bool opened = ImGui::TreeNodeEx(label, flags);
        if (opened)
        {
            ImGui::Indent(Theme::Layout::IndentSpacing);    // 下方内容缩进
        }
        
        return opened;
    }
    
    void EndCollapsing()
    {
        ImGui::TreePop();
        ImGui::Indent(-Theme::Layout::IndentSpacing);   // 恢复缩进
    }

    /// <summary>
    /// 内部实现：树节点核心逻辑
    /// </summary>
    static bool BeginTreeNodeInternal(const Ref<Texture2D>& icon, const char* name, bool defaultOpen, bool selected, bool isLeaf)
    {
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
        
        bool opened = ImGui::TreeNodeEx("", flags);

        // 保存 TreeNode 的 LastItemData，用于后续恢复
        ImGuiContext& g = *GImGui;
        const ImGuiLastItemData treeNodeItemData = g.LastItemData;

        ImGui::SameLine();

        ShiftCursorX(Theme::Layout::TreeNodeArrowToIconSpacing);
        
        float iconSize = ImGui::GetTextLineHeight() - Theme::Layout::TreeNodeIconSizeShrink;
        ImTextureID texID = GetImTextureID(icon);
        if (texID)
        {
            ShiftCursorY(Theme::Layout::TreeNodeIconOffsetY);
            ImGui::Image(texID, ImVec2(iconSize, iconSize), ImVec2(0, 1), ImVec2(1, 0));   // OpenGL Y 翻转
            ImGui::SameLine();
            ShiftCursorX(Theme::Layout::TreeNodeIconToTextSpacing);
            ShiftCursorY(-Theme::Layout::TreeNodeIconOffsetY);
        }

        // 截断 ## 及其后面的 ID 部分，只显示可见文本
        const char* hashPos = strstr(name, "##");
        if (hashPos)
        {
            ImGui::TextUnformatted(name, hashPos);
        }
        else
        {
            ImGui::TextUnformatted(name);
        }

        // 恢复 TreeNode 为 LastItem，使调用方的 IsItemClicked 等交互检测正确工作
        g.LastItemData = treeNodeItemData;

        return opened;
    }

    bool BeginTreeNode(const Ref<Texture2D>& icon, const char* name, bool defaultOpen, bool selected, bool isLeaf)
    {
        ImGui::PushID(name);
        bool opened = BeginTreeNodeInternal(icon, name, defaultOpen, selected, isLeaf);
        ImGui::PopID();
        return opened;
    }

    bool BeginTreeNode(const Ref<Texture2D>& closedIcon, const Ref<Texture2D>& openIcon, const char* name, bool defaultOpen, bool selected, bool isLeaf)
    {
        ImGui::PushID(name);

        // 先查询节点当前的展开状态，以选择正确的图标
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGuiID storageID = window->GetID("");
        bool isNodeOpen = window->DC.StateStorage->GetInt(storageID, defaultOpen ? 1 : 0) != 0;

        const Ref<Texture2D>& icon = isNodeOpen ? openIcon : closedIcon;
        bool opened = BeginTreeNodeInternal(icon, name, defaultOpen, selected, isLeaf);
        ImGui::PopID();
        
        return opened;
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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { Theme::Layout::WindowPaddingX, Theme::Layout::WindowPaddingY });
        bool opened = ImGui::BeginPopupContextWindow(strID, popupFlags);
        ImGui::PopStyleVar();
        
        if (opened)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { Theme::Layout::WindowPaddingX, Theme::Layout::WindowPaddingY });
        }
        return opened;
    }
    
    bool BeginPopupContextItem(const char* strID, ImGuiPopupFlags popupFlags)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { Theme::Layout::WindowPaddingX, Theme::Layout::WindowPaddingY });
        bool opened = ImGui::BeginPopupContextItem(strID, popupFlags);
        ImGui::PopStyleVar();
        
        if (opened)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { Theme::Layout::WindowPaddingX, Theme::Layout::WindowPaddingY });
        }
        return opened;
    }
    
    bool BeginPopup(const char* strID, ImGuiPopupFlags popupFlags)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { Theme::Layout::WindowPaddingX, Theme::Layout::WindowPaddingY });
        bool opened = ImGui::BeginPopup(strID, popupFlags);
        ImGui::PopStyleVar();
        
        if (opened)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { Theme::Layout::WindowPaddingX, Theme::Layout::WindowPaddingY });
        }
        return opened;
    }
    
    void EndPopup()
    {
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }
    
    bool DropdownList(int& selected, const char* const* options, int count)
    {
        bool modified = false;
        
        if (ImGui::BeginCombo(GenerateID(), options[selected]))
        {
            for (int i = 0; i < count; i++)
            {
                bool isSelected = (i == selected);

                if (ImGui::Selectable(options[i], isSelected))
                {
                    if (i != selected)
                    {
                        selected = i;
                        modified = true;
                    }
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        
        return modified;
    }
}
