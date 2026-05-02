#include "lcpch.h"
#include "Widgets.h"

#include "ScopedGuards.h"

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
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen * defaultOpen;

        ScopedStyle frameRounding(ImGuiStyleVar_FrameRounding, 0.0f);
        ScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 6.0f));

        // TODO Font 粗体
        
        return ImGui::TreeNodeEx(label, flags);
    }
    
    void EndCollapsing()
    {
        ImGui::TreePop();
    }
    
    bool BeginTreeNode(const char* name, bool selected, bool isLeaf)
    {
        // TODO 图标
        
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (selected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        
        if (isLeaf)
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

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
}
