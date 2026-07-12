#include "lcpch.h"
#include "Widgets.h"

#include "DrawUtils.h"
#include "ScopedGuards.h"
#include "Theme.h"
#include "UICore.h"

#include "Lucky/Editor/EditorIconManager.h"

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
    static bool BeginTreeNodeInternal(const Ref<Texture2D>& icon, const char* name, bool defaultOpen, bool selected, bool isLeaf, bool renderName)
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
        // renderName = false 时由调用方（内联重命名）接管文本区的渲染，内部不再绘制默认文本
        if (renderName)
        {
            const char* hashPos = strstr(name, "##");
            if (hashPos)
            {
                ImGui::TextUnformatted(name, hashPos);
            }
            else
            {
                ImGui::TextUnformatted(name);
            }
        }

        // 恢复 TreeNode 为 LastItem，使调用方的 IsItemClicked 等交互检测正确工作
        g.LastItemData = treeNodeItemData;

        return opened;
    }

    bool BeginTreeNode(const Ref<Texture2D>& icon, const char* name, bool defaultOpen, bool selected, bool isLeaf, bool renderName)
    {
        ImGui::PushID(name);
        bool opened = BeginTreeNodeInternal(icon, name, defaultOpen, selected, isLeaf, renderName);
        ImGui::PopID();
        return opened;
    }

    bool BeginTreeNode(const Ref<Texture2D>& closedIcon, const Ref<Texture2D>& openIcon, const char* name, bool defaultOpen, bool selected, bool isLeaf, bool renderName)
    {
        ImGui::PushID(name);

        // 先查询节点当前的展开状态，以选择正确的图标
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGuiID storageID = window->GetID("");
        bool isNodeOpen = window->DC.StateStorage->GetInt(storageID, defaultOpen ? 1 : 0) != 0;

        const Ref<Texture2D>& icon = isNodeOpen ? openIcon : closedIcon;
        bool opened = BeginTreeNodeInternal(icon, name, defaultOpen, selected, isLeaf, renderName);
        ImGui::PopID();
        
        return opened;
    }
    
    void EndTreeNode()
    {
        ImGui::TreePop();
    }

    InlineRenameResult InlineRenameInput(const char* id, const ImVec2& rectMin, const ImVec2& rectMax, char* buffer, size_t bufferSize, bool firstFrame)
    {
        InlineRenameResult result;

        ImVec2 size = ImVec2(rectMax.x - rectMin.x, rectMax.y - rectMin.y);
        if (size.x <= 0.0f || size.y <= 0.0f)
        {
            return result;
        }

        // ---- 样式覆盖：无圆角 + 强制 1px 边框 + 动态推导 FramePadding.y（问题 2：编辑框高度 = TreeNode 行高 - 1px） ----
        // 关键：TreeNode 真实行高 ≠ FontSize + style.FramePadding.y*2。
        //   见 imgui_widgets.cpp:5848 TreeNodeBehavior：
        //     padding = (framed) ? style.FramePadding
        //                        : ImVec2(FramePadding.x, ImMin(CurrLineTextBaseOffset, FramePadding.y));
        //   BeginTreeNodeInternal 未使用 Framed / FramePadding flag，因此 padding.y 通常 < style.FramePadding.y，
        //   TreeNode 真实行高 = FontSize + padding.y*2，往往比 InputText 默认高度小。
        //   若直接沿用 style.FramePadding.y，InputText 会明显比 TreeNode 高（图片实测症状）。
        //
        // 修复策略：以调用方传入的 size.y（= 命中矩形高度 = TreeNode 真实行高）为准，反推 FramePadding.y：
        //     目标 InputText 高度 = size.y - 1（比 TreeNode 矮 1px，视觉更贴合）
        //     InputText 高度 = FontSize + FramePadding.y * 2
        //     → FramePadding.y = (size.y - 1 - FontSize) / 2，并夹到 [0, style.FramePadding.y] 保证不出现负值
        const ImGuiStyle& style = ImGui::GetStyle();
        const float fontSize     = ImGui::GetFontSize();
        const float targetHeight = ImMax(size.y - 1.0f, fontSize); // 至少容纳一行文字
        float framePaddingY      = (targetHeight - fontSize) * 0.5f;
        framePaddingY            = ImClamp(framePaddingY, 0.0f, style.FramePadding.y);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, framePaddingY));

        // ---- 保存布局状态：InputText 是"覆盖式绘制"，理应不影响后续布局 ----
        // 背景分析（关键，直接决定恢复策略是否正确）：
        //   1) TreeNode 内部 BeginTreeNodeInternal 使用 ScopedStyle(ItemSpacing, {0,0})，
        //      TreeNodeEx 的 ItemSize 走垂直分支：CursorPos.y = 起点.y + line_height + 0 = TreeNode 底。
        //      随后 SameLine() 令 CursorPos.y = CursorPosPrevLine.y = TreeNode 顶。
        //      Image / DrawEntityComponentIcons 内的所有绘制都在 SameLine 分支上（CursorPos.y 保持 TreeNode 顶）。
        //      DrawEntityComponentIcons 结尾 SetCursorPos(savedCursorPos) 也恢复到 TreeNode 顶。
        //   2) 所以进入 InlineRenameInput 时：DC.CursorPos.y ≈ TreeNode 顶（**尚未推进到下一行**）
        //      而"下一节点 Directional Light 应该起绘的 Y" = TreeNode 底 + ItemSpacing.y = rectMax.y + ItemSpacing.y。
        //   3) 若原样保存并恢复 CursorPos.y，下一节点会从 TreeNode 顶开始画 → 与 Cube 编辑行整行重叠。
        //   4) 若完全不恢复，InputText 的 ItemSize 会把 CursorPos.y 推进到"InputText 底 + ItemSpacing.y"
        //      ≈ TreeNode 底 + ItemSpacing.y - 0.5f，虽然接近正确值，但 InputText 的水平 CursorMaxPos.x 与
        //      垂直换行分支产物会污染布局（下一节点被推低约 ItemSpacing.y 像素）。
        //
        // 因此正确策略：**主动前置** DC.CursorPos.y 到"TreeNode 底"（模拟 TreeNode 的 ItemSize 未启用 ScopedStyle 时的
        // 状态），让 InputText 的 ItemSize 走垂直分支时推进到 rectMax.y + ItemSpacing.y = 下一节点正确起点。
        // 通过手动构造正确的 CursorPos.y 起点 + 让 InputText 自然走 ItemSize，就能让"布局副作用恰好等于 TreeNode 本该产生的换行"。
        ImGuiWindow* layoutWindow = ImGui::GetCurrentWindow();
        const ImVec2 savedCursorMaxPos = layoutWindow->DC.CursorMaxPos;

        // ---- 定位光标 ----
        // 问题 1 精修：InputText 内文字起点 = frame_bb.Min.x + FramePadding.x（不含 FrameBorderSize，见 imgui_widgets.cpp:4549）；
        //              原名文本 = 图标右侧 + IconToTextSpacing，紧接其后由 TextUnformatted 从 CursorPos 直接绘制（无 padding）。
        //              调用方传入的 rectMin.x 已对齐"原名文本真实 X"，因此只需将编辑框整体向左偏移 FramePadding.x，
        //              使 frame_bb.Min.x + FramePadding.x == rectMin.x，从而让编辑框内文本严格对齐原名文本。
        //              注意：*不* 再补偿 FrameBorderSize，因为 draw_pos 计算里根本不含 border。
        const float xOffset = style.FramePadding.x;
        ImVec2 inputPos = rectMin;
        inputPos.x -= xOffset;
        // 垂直居中：InputText 实际高度 = FontSize + framePaddingY*2；向下偏移 (size.y - 实际高度) / 2 使其在命中矩形内居中
        const float actualHeight = fontSize + framePaddingY * 2.0f;
        inputPos.y += (size.y - actualHeight) * 0.5f;
        ImGui::SetCursorScreenPos(inputPos);

        // 首帧：自动将焦点给到下个 Item（InputText），并启用 AutoSelectAll
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
        if (firstFrame)
        {
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::PushID(id);
        // 宽度补偿：向左偏了 xOffset 像素，为保持右端不越出命中矩形，总宽度补 xOffset
        ImGui::SetNextItemWidth(size.x + xOffset);
        bool enterPressed = ImGui::InputText("##InlineRename", buffer, bufferSize, flags);
        bool isItemActive          = ImGui::IsItemActive();
        bool isItemDeactivated     = ImGui::IsItemDeactivated();
        bool isItemDeactivatedEdit = ImGui::IsItemDeactivatedAfterEdit();
        bool isItemHovered         = ImGui::IsItemHovered();
        ImGui::PopID();

        ImGui::PopStyleVar(3);

        // ---- 修正布局状态：让 InputText 表现得等价于"TreeNode 的换行" ----
        // 目标：让下一个节点的起绘 Y 与"非编辑态"时完全一致，避免编辑框把下面的节点挤下去 1 行间距。
        //
        // 非编辑态下 Cube → Directional Light 之间的 CursorPos.y 推进过程：
        //   1) BeginTreeNodeInternal 内 ScopedStyle(ItemSpacing, {0,0}) 生效期间，最后一个 Item（TextUnformatted(name)）
        //      走垂直分支 ItemSize：CursorPos.y = 行顶 + line_height + 0 = 行底 = rectMax.y。
        //   2) ScopedStyle 析构（恢复 ItemSpacing.y = 4）不改 CursorPos.y。
        //   3) 下一个 DrawEntityNode 的 TreeNodeEx 从 CursorPos.y = rectMax.y 开始起绘。
        // 也就是说：**Cube 与 Directional Light 之间没有 ItemSpacing.y 间距**（被 ScopedStyle 强制为 0）。
        //
        // 编辑态下 renderName=false，BeginTreeNodeInternal 结尾跳过 TextUnformatted，最后一个 Item 是 Image；
        // Image 后紧跟 SameLine + ShiftCursorY(-TreeNodeIconOffsetY)，导致 DC.CursorPos.y 停在"行顶 - 2px"（不推进到行底）。
        // 因此我们必须**主动**把 CursorPos.y 恢复到 rectMax.y（= 非编辑态下 Cube 结束时的 CursorPos.y），
        // 而**不能**再加 style.ItemSpacing.y（那会多出 ~4px，导致 Directional Light 被挤下去）。
        //
        // CursorMaxPos.y 也一并强制对齐 rectMax.y，与 TreeNode 结束时一致（Window 用它决定滚动条与内容底）。
        layoutWindow->DC.CursorPos.y    = rectMax.y;
        layoutWindow->DC.CursorMaxPos.y = ImMax(savedCursorMaxPos.y, rectMax.y);

        // Esc 取消（仅当输入框仍活跃时才捕获，避免失焦后误触）
        if (isItemActive && ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            result.Cancelled = true;
            return result;
        }

        // Enter 提交
        if (enterPressed)
        {
            result.Submitted = true;
            result.CommittedName = buffer;
            return result;
        }

        // ---- 失焦处理：区分"已编辑失焦"与"未编辑失焦" ----
        // 只有 IsItemDeactivatedAfterEdit（用户实际编辑过后失焦）才走提交路径；
        // 未编辑就失焦 → 视为取消，避免"点击编辑框内部但被 imgui 判定为 deactivate"时误提交空串
        // firstFrame 禁止判定失焦（SetKeyboardFocusHere 首帧尚未完全激活）
        if (!firstFrame && isItemDeactivatedEdit)
        {
            result.Submitted = true;
            result.CommittedName = buffer;
            return result;
        }
        if (!firstFrame && isItemDeactivated)
        {
            // 未编辑就失焦：取消（不写入 name，等价于保留原名）
            result.Cancelled = true;
            return result;
        }

        return result;
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

    void DragDropPreview(bool rejected)
    {
        const Ref<Texture2D>& icon = EditorIconManager::GetDragDropIcon(rejected);
        ImageFlipped(icon, ImVec2(Theme::Layout::DragDropIconSize, Theme::Layout::DragDropIconSize));
    }

    bool BeginDragDropSource(ImGuiDragDropFlags flags)
    {
        // ImGui 拖拽 tooltip 内部使用 ImGuiCol_PopupBg 作为窗口背景色（见 GetWindowBgColorIdx()），
        // 且 BeginTooltipEx 会强制调用 SetNextWindowBgAlpha(PopupBg.w * 0.60f) 覆盖 alpha 通道；
        // 因此必须将 PopupBg 的 alpha 也置 0，才能让最终背景透明
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));

        bool active = ImGui::BeginDragDropSource(flags);
        if (!active)
        {
            // 未进入拖拽状态，未创建 tooltip，直接回滚样式栈
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(3);
        }
        return active;
    }

    void EndDragDropSource()
    {
        ImGui::EndDragDropSource();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
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
