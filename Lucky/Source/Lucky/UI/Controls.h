#pragma once

#include "DrawUtils.h"
#include "Theme.h"
#include "ScopedGuards.h"

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Texture.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>


namespace Lucky::UI
{
    // ========================================================================
    // 增强控件
    // 参数签名与 ImGui 原生完全一致，内部加 DrawItemActivityOutline 视觉增强
    // ========================================================================

    // ---- Drag 系列 ----

    inline bool DragFloat(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.4f", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);
        Draw::ItemTopShadow();
        DrawItemActivityOutline();
        return changed;
    }

    inline bool DragFloat2(const char* label, float v[2], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.4f", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::DragFloat2(label, v, v_speed, v_min, v_max, format, flags);
        DrawItemActivityOutline();
        return changed;
    }

    inline bool DragFloat3(const char* label, float v[3], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.4f", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::DragFloat3(label, v, v_speed, v_min, v_max, format, flags);
        DrawItemActivityOutline();
        return changed;
    }

    inline bool DragFloat4(const char* label, float v[4], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.4f", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::DragFloat4(label, v, v_speed, v_min, v_max, format, flags);
        DrawItemActivityOutline();
        return changed;
    }

    inline bool DragInt(const char* label, int* v, float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::DragInt(label, v, v_speed, v_min, v_max, format, flags);
        Draw::ItemTopShadow();
        DrawItemActivityOutline();
        return changed;
    }

    // ---- Slider 系列 ----

    inline bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.4f", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
        Draw::ItemTopShadow();
        DrawItemActivityOutline();
        return changed;
    }

    inline bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0)
    {
        bool changed = ImGui::SliderInt(label, v, v_min, v_max, format, flags);
        Draw::ItemTopShadow();
        DrawItemActivityOutline();
        return changed;
    }

    // ---- Input 系列 ----

    inline bool InputText(const char* label, char* buf, size_t bufSize, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* userData = nullptr)
    {
        bool changed = ImGui::InputText(label, buf, bufSize, flags, callback, userData);
        Draw::ItemTopShadow();
        DrawItemActivityOutline();
        return changed;
    }

    inline bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* userData = nullptr)
    {
        bool changed = ImGui::InputText(label, str, flags, callback, userData);
        Draw::ItemTopShadow();
        DrawItemActivityOutline();
        return changed;
    }

    inline bool InputInt(const char* label, int* v, int step = 1, int stepFast = 100, ImGuiInputTextFlags flags = 0)
    {
        bool changed = ImGui::InputInt(label, v, step, stepFast, flags);
        DrawItemActivityOutline();
        return changed;
    }

    // ---- Color 系列 ----

    inline bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0)
    {
        bool changed = ImGui::ColorEdit3(label, col, flags);
        DrawItemActivityOutline();
        return changed;
    }

    inline bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0)
    {
        bool changed = ImGui::ColorEdit4(label, col, flags);
        DrawItemActivityOutline();
        return changed;
    }
    
    inline bool ColorButton(const char* desc_id, ImVec4& color, ImGuiColorEditFlags flags = 0)
    {
        ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, { Theme::Layout::WindowPaddingX, Theme::Layout::WindowPaddingY });
        
        // Generate a default palette. The palette will persist and can be edited.
        static bool saved_palette_init = true;
        static ImVec4 saved_palette[32] = {};
        if (saved_palette_init)
        {
            for (int n = 0; n < IM_ARRAYSIZE(saved_palette); n++)
            {
                ImGui::ColorConvertHSVtoRGB(n / 31.0f, 0.8f, 0.8f,
                    saved_palette[n].x, saved_palette[n].y, saved_palette[n].z);
                saved_palette[n].w = 1.0f; // Alpha
            }
            saved_palette_init = false;
        }

        float width = ImGui::GetContentRegionAvail().x - Theme::Layout::WindowPaddingX;
        
        static ImVec4 backup_color;

        bool changed = false;
        bool open_popup = ImGui::ColorButton(desc_id, color, flags, { width, 28 });
        DrawItemActivityOutline();
        
        if (open_popup)
        {
            ImGui::OpenPopup("mypicker");
            backup_color = color;
        }
        if (ImGui::BeginPopup("mypicker"))
        {
            changed = ImGui::ColorPicker4("##picker", (float*)&color, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
            ImGui::SameLine();

            ImGui::BeginGroup(); // Lock X position
            
            // Current Color
            ImGui::Text("Current");
            ImGui::ColorButton("##current", color, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2(60, 40));
            DrawItemActivityOutline();
            
            // Original Color
            ImGui::Text("Original");
            if (ImGui::ColorButton("##original", backup_color, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2(60, 40)))
            {
                color = backup_color;
                changed = true;
            }
            DrawItemActivityOutline();

            // Palette Colors
            ImGui::Text("Palette");
            for (int n = 0; n < IM_ARRAYSIZE(saved_palette); n++)
            {
                ImGui::PushID(n);
                if ((n % 8) != 0)
                {
                    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);
                }
                
                ImGuiColorEditFlags palette_button_flags = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker;
                if (ImGui::ColorButton("##palette", saved_palette[n], palette_button_flags, ImVec2(20, 20)))
                {
                    color = ImVec4(saved_palette[n].x, saved_palette[n].y, saved_palette[n].z, color.w); // Preserve alpha!
                    changed = true;
                }
                DrawItemActivityOutline();
                
                // Allow user to drop colors into each palette entry. Note that ColorButton() is already a
                // drag source by default, unless specifying the ImGuiColorEditFlags_NoDragDrop flag.
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
                        memcpy((float*)&saved_palette[n], payload->Data, sizeof(float) * 3);
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
                        memcpy((float*)&saved_palette[n], payload->Data, sizeof(float) * 4);
                    ImGui::EndDragDropTarget();
                }

                ImGui::PopID();
            }
            ImGui::EndGroup();
            
            ImGui::EndPopup();
        }

        return changed;
    }

    // ---- Combo ----

    inline bool BeginCombo(const char* label, const char* previewValue, ImGuiComboFlags flags = 0)
    {
        bool opened = ImGui::BeginCombo(label, previewValue, flags);
        Draw::ItemTopShadow();
        DrawItemActivityOutline();
        return opened;
    }

    inline void EndCombo()
    {
        ImGui::EndCombo();
    }

    // ---- Checkbox ----

    inline bool Checkbox(const char* label, bool* v)
    {
        bool changed = ImGui::Checkbox(label, v);
        Draw::ItemTopShadow();
        DrawItemActivityOutline();
        return changed;
    }
    
    // ---- Button ----
    
    inline bool Button(const char* label, const ImVec2& size = ImVec2(0, 0))
    {
        bool changed = ImGui::Button(label, size);
        Draw::ItemBottomShadow();
        return changed;
    }
    
    // ---- ImageButton TODO 整合到 ObjectField ----
    
    inline bool ImageButton(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 1),  const ImVec2& uv1 = ImVec2(1, 0), int frame_padding = -1, const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1))
    {
        bool changed = ImGui::ImageButton(user_texture_id, size, uv0, uv1, frame_padding, bg_col, tint_col);
        DrawItemActivityOutline();
        return changed;
    }
    
    // const Ref<TObject>& ObjectField(label)
    // ---- ObjectField TODO 资产系统实现后再完善，现在仅做显示用 ----
    inline bool ObjectField(const char* label)
    {
        const std::string& strID = label;
        
        float width = ImGui::GetContentRegionAvail().x - Theme::Layout::WindowPaddingX;
        
        // Field Frame
        ImVec4 frameBgColor = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        ImGui::PushStyleColor(ImGuiCol_Button, frameBgColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, frameBgColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, frameBgColor);
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, { (Theme::Layout::FramePaddingX - 4) / width, 0 });
        bool openPopup = ImGui::Button(strID.c_str(), { width, 28 });
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        
        Draw::ItemTopShadow();
        DrawItemActivityOutline();
        
        // static bool openPopup = false;
        
        bool changed = false;
        
        if (openPopup)
        {
            // TODO 打开对象选择面板，从当前场景中选择实体、从资产库选择资产
            // ImGui::OpenPopup("AssetSelect");
        }
        
        // if (ImGui::BeginPopup("AssetSelect", &openPopup))
        // {
        //     ImGui::Text("Test");
        //     
        //     ImGui::EndPopup();
        // }
        
        // Field Icon

        return changed;
    }
    
    // ---- AssetField：资产引用控件 ----
    // 显示 [图标] [资产名称] 的样式，用于资产引用属性
    // icon: 资产类型图标（可为 nullptr）
    // displayName: 显示的资产名称（如 "Metal.lmat"），无资产时传 "None (Material)" 等
    // 返回值：是否被点击（预留给未来资产选择弹窗）
    inline bool AssetField(const char* id, const Ref<Texture2D>& icon, const char* displayName)
    {
        float width = std::max(ImGui::GetContentRegionAvail().x - Theme::Layout::WindowPaddingX, 1.0f);
        float height = Theme::Layout::AssetFieldHeight;
        
        // 布局参数
        float iconSize = Theme::Layout::AssetFieldIconSize;
        float iconPaddingX = Theme::Layout::AssetFieldIconPaddingX;
        float iconToText = Theme::Layout::AssetFieldIconToTextSpacing;
        float textOffsetX = iconPaddingX + iconSize + iconToText;
        
        // 绘制背景框（使用 FrameBg 颜色）
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImVec2 rectMin = cursorPos;
        ImVec2 rectMax = ImVec2(cursorPos.x + width, cursorPos.y + height);
        
        ImVec4 frameBgColor = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(frameBgColor);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        float rounding = Theme::Layout::FrameRounding;
        drawList->AddRectFilled(rectMin, rectMax, bgColor, rounding);
        
        // 绘制黑色边框
        ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Border));
        
        ImVec2 borderMin(rectMin.x + 0.5f, rectMin.y + 0.5f);
        ImVec2 borderMax(rectMax.x - 0.5f, rectMax.y - 0.5f);
        drawList->AddRect(borderMin, borderMax, borderColor, rounding, 0, 1.0f);
        
        // 使用 InvisibleButton 处理点击交互
        std::string buttonID = std::string("##AssetField_") + id;
        bool clicked = ImGui::InvisibleButton(buttonID.c_str(), { width, height });
        
        // 绘制图标
        if (icon)
        {
            float iconY = rectMin.y + (height - iconSize) * 0.5f;
            float iconX = rectMin.x + iconPaddingX;
            
            drawList->AddImage(
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(icon->GetRendererID())),
                ImVec2(iconX, iconY),
                ImVec2(iconX + iconSize, iconY + iconSize),
                ImVec2(0, 1), ImVec2(1, 0)  // OpenGL Y 翻转
            );
        }
        
        // 绘制文本（精确像素定位，不受文本长度影响）
        {
            float textX = rectMin.x + textOffsetX;
            float textY = rectMin.y + (height - ImGui::GetTextLineHeight()) * 0.5f;
            
            // 裁剪文本，防止超出控件右边界
            ImVec4 clipRect(textX, rectMin.y, rectMax.x - Theme::Layout::FramePaddingX, rectMax.y);
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(textX, textY),
                ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)),
                displayName, nullptr, 0.0f, &clipRect);
        }
        
        Draw::ItemTopShadow();
        DrawItemActivityOutline();
        
        return clicked;
    }
}
