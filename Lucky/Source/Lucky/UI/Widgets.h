#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Texture.h"

#include <imgui/imgui.h>

namespace Lucky::UI
{
    // ========================================================================
    // 非 Property 语义化控件
    // ========================================================================
    
    // ---- 只用于第一层的可折叠子分组：无嵌套，可嵌套 Sub 或者 普通分组 ----
    bool BeginPrimaryCollapsing(const char* label);
    void EndPrimaryCollapsing();
    
    // bool BeginSubCollapsing(const char* label, bool defaultOpen = true);
    
    bool BeginCollapsing(const char* label, bool defaultOpen = true);
    
    void EndCollapsing();

    /// <summary>
    /// 树节点 用于 Hierarchy / Assets 等面板的树形结构
    /// </summary>
    /// <param name="defaultOpen">默认打开</param>
    /// <param name="name">名称</param>
    /// <param name="selected">已选中</param>
    /// <param name="isLeaf">是叶节点</param>
    /// <returns></returns>
    bool BeginTreeNode(const char* name, bool defaultOpen = false, bool selected = false, bool isLeaf = false);
    void EndTreeNode();

    /// <summary>
    /// 绘制图像
    /// </summary>
    /// <param name="texture">纹理（可为 nullptr，此时不绘制）</param>
    /// <param name="size">图像尺寸</param>
    /// <param name="uv0">UV 左上角（默认 {0, 0}）</param>
    /// <param name="uv1">UV 右下角（默认 {1, 1}）</param>
    /// <param name="tintColor">着色颜色（默认白色）</param>
    /// <param name="borderColor">边框颜色（默认无边框）</param>
    void Image(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tintColor = ImVec4(1, 1, 1, 1), const ImVec4& borderColor = ImVec4(0, 0, 0, 0));

    /// <summary>
    /// 绘制图像（OpenGL Y 翻转版本，UV 默认为 { 0, 1 } -> { 1, 0 }）适用于 Framebuffer 颜色附件等需要 Y 翻转的场景
    /// </summary>
    void ImageFlipped(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec4& tintColor = ImVec4(1, 1, 1, 1), const ImVec4& borderColor = ImVec4(0, 0, 0, 0));
    
    // ---- Popup ----
    
    bool BeginPopupContextWindow(const char* strID = nullptr, ImGuiPopupFlags popupFlags = 1);
    bool BeginPopupContextItem(const char* strID = nullptr, ImGuiPopupFlags popupFlags = 1);
    bool BeginPopup(const char* strID = nullptr, ImGuiPopupFlags popupFlags = 0);
    void EndPopup();
    
    bool DropdownList(int& selected, const char* const* options, int count);
}