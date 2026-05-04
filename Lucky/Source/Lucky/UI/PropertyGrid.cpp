#include "lcpch.h"
#include "PropertyGrid.h"

#include "Lucky/UI/UICore.h"
#include "Lucky/UI/Controls.h"
#include "Lucky/UI/DrawUtils.h"
#include "Lucky/UI/Theme.h"
#include "Lucky/UI/ScopedGuards.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

namespace Lucky::UI
{
    // ========================================================================
    // PropertyGrid 布局
    // ========================================================================

    void BeginPropertyGrid()
    {
        PushID();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::Layout::ItemSpacingX, Theme::Layout::ItemSpacingY));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::Layout::PropertyFramePaddingX, Theme::Layout::PropertyFramePaddingY));

        ImGui::BeginTable("##PropertyGrid", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);

        // Label 列：固定宽度，至少 80px，最多占面板宽度的 35%
        float panelWidth = ImGui::GetContentRegionAvail().x;
        float labelWidth = std::max(panelWidth * Theme::Layout::PropertyLabelWidthRatio, Theme::Layout::PropertyLabelMinWidth);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, labelWidth);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    }

    void EndPropertyGrid()
    {
        ImGui::EndTable();
        ImGui::PopStyleVar(2);  // ItemSpacing, FramePadding
        PopID();
    }

    // ========================================================================
    // 内部辅助：Property 行的统一模式
    // ========================================================================

    /// <summary>
    /// 绘制 Property 行的 Label 列
    /// </summary>
    static void PropertyLabel(const char* label)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ShiftCursor(Theme::Layout::PropertyLabelOffsetX, 0.0f);
        ImGui::TextUnformatted(label);
    }

    /// <summary>
    /// 开始绘制 Property 行的 Value 列
    /// </summary>
    static void PropertyValueBegin()
    {
        ImGui::TableSetColumnIndex(1);
        ShiftCursorY(Theme::Layout::PropertyValueOffsetY);
        ImGui::PushItemWidth(-8.0f);  // 填满剩余宽度 右侧 8 间距
    }

    /// <summary>
    /// 结束绘制 Property 行的 Value 列
    /// </summary>
    static void PropertyValueEnd()
    {
        ImGui::PopItemWidth();
    }

    // ========================================================================
    // Property 语义化控件实现
    // ========================================================================

    // ---- Float 系列 ----

    bool PropertyFloat(const char* label, float& value, float delta, float min, float max)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = DragFloat(GenerateID(), &value, delta, min, max);

        PropertyValueEnd();
        return modified;
    }

    bool PropertyFloat2(const char* label, glm::vec2& value, float delta, float min, float max)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = DragFloat2(GenerateID(), glm::value_ptr(value), delta, min, max);

        PropertyValueEnd();
        return modified;
    }

    bool PropertyFloat3(const char* label, glm::vec3& value, float delta, float min, float max)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = DragFloat3(GenerateID(), glm::value_ptr(value), delta, min, max);

        PropertyValueEnd();
        return modified;
    }

    bool PropertyFloat4(const char* label, glm::vec4& value, float delta, float min, float max)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = DragFloat4(GenerateID(), glm::value_ptr(value), delta, min, max);

        PropertyValueEnd();
        return modified;
    }

    // ---- Int 系列 ----

    bool PropertyInt(const char* label, int& value, float delta, int min, int max)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = DragInt(GenerateID(), &value, delta, min, max);

        PropertyValueEnd();
        return modified;
    }

    // ---- Color 系列 ----

    bool PropertyColor(const char* label, glm::vec3& value)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = ColorEdit3(GenerateID(), glm::value_ptr(value));

        PropertyValueEnd();
        return modified;
    }

    bool PropertyColor4(const char* label, glm::vec4& value)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = ColorEdit4(GenerateID(), glm::value_ptr(value));

        PropertyValueEnd();
        return modified;
    }

    // ---- String 系列 ----

    bool PropertyString(const char* label, char* value, size_t bufSize)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = InputText(GenerateID(), value, bufSize);

        PropertyValueEnd();
        return modified;
    }

    void PropertyReadOnlyString(const char* label, const char* value)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        ScopedDisable disabled(true);
        // 使用 const_cast 是安全的，因为 ReadOnly 标志阻止了修改
        InputText(GenerateID(), const_cast<char*>(value), strlen(value) + 1, ImGuiInputTextFlags_ReadOnly);

        PropertyValueEnd();
    }

    // ---- Combo 下拉框 ----

    bool PropertyCombo(const char* label, int& selected, const char* const* options, int count)
    {
        bool modified = false;

        PropertyLabel(label);
        PropertyValueBegin();

        // BeginCombo 返回的是"是否打开"，不是"是否修改"
        // 需要手动跟踪选中项的变化
        if (BeginCombo(GenerateID(), options[selected]))
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

            EndCombo();
        }

        PropertyValueEnd();
        return modified;
    }

    // ---- Checkbox ----

    bool PropertyCheckbox(const char* label, bool& value)
    {
        PropertyLabel(label);
        PropertyValueBegin();

        bool modified = Checkbox(GenerateID(), &value);

        PropertyValueEnd();
        return modified;
    }

    // ---- Texture ----

    bool PropertyTexture(const char* label, const Ref<Texture2D>& texture)
    {
        bool modified = false;

        PropertyLabel(label);

        ImGui::TableSetColumnIndex(1);
        
        uint32_t textureRendererID = texture->GetRendererID();
        float previewSize = Theme::Layout::TexturePreviewSize;

        // 纹理预览按钮
        if (ImageButton(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(textureRendererID)), ImVec2(previewSize, previewSize)))
        {
            modified = true;
        }

        // TODO: 拖拽接收（未来资产浏览器实现后启用）
        // if (ImGui::BeginDragDropTarget())
        // {
        //     if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_TEXTURE"))
        //     {
        //         // 从 payload 中获取纹理路径，创建纹理
        //     }
        //     ImGui::EndDragDropTarget();
        // }
        
        return modified;
    }
}