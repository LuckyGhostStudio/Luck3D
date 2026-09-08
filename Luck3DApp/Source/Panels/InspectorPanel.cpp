#include "InspectorPanel.h"

#include "Lucky/Scene/SelectionManager.h"
#include "Lucky/Scene/ComponentRegistry.h"

#include "Lucky/UI/Controls.h"
#include "Lucky/UI/PropertyGrid.h"
#include "Lucky/UI/Widgets.h"

#include "Lucky/Editor/MaterialEditor.h"
#include "Lucky/Editor/AssetInspectorRegistry.h"
#include "Lucky/Editor/FolderInspector.h"

namespace Lucky
{
    InspectorPanel::InspectorPanel(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
        // 订阅 SceneManager 的场景切换事件
        m_SceneChangedSub = SceneManager::Subscribe([this](const Ref<Scene>& newScene)
        {
            SetScene(newScene);
        });
    }

    InspectorPanel::~InspectorPanel()
    {
        SceneManager::Unsubscribe(m_SceneChangedSub);
    }

    void InspectorPanel::SetScene(const Ref<Scene>& scene)
    {
        m_Scene = scene;
    }

    void InspectorPanel::OnUpdate(DeltaTime dt)
    {
        
    }

    void InspectorPanel::OnGUI()
    {
        // 按当前选中类型分发到不同的绘制路径
        // - Entity：走 DrawComponents 显示组件面板
        // - Asset ：走 AssetInspectorRegistry，由已注册的具体 AssetInspector 处理
        // - Folder：走 FolderInspector，仅显示 Header
        // - None  ：不绘制任何内容（面板保持为空）
        switch (SelectionManager::GetSelectionType())
        {
            case SelectionType::Entity:
            {
                UUID entityID = SelectionManager::GetSelection();
                if (entityID != 0 && m_Scene)
                {
                    Entity entity = m_Scene->GetEntityWithUUID(entityID);
                    if (entity)
                    {
                        DrawComponents(entity);
                    }
                }
                break;
            }
            case SelectionType::Asset:
            {
                AssetHandle handle(static_cast<uint64_t>(SelectionManager::GetSelection()));
                AssetInspectorRegistry::Draw(handle);
                break;
            }
            case SelectionType::Folder:
            {
                FolderInspector::Draw(SelectionManager::GetSelectedFolder());
                break;
            }
            case SelectionType::None:
            default:
                break;
        }
    }

    void InspectorPanel::DrawComponents(Entity entity)
    {
        // Name 组件顶部特殊输入框（不套 TreeNode 外壳，独立绘制）
        if (entity.HasComponent<NameComponent>())
        {
            const std::string& name = entity.GetName();

            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            strcpy_s(buffer, sizeof(buffer), name.c_str());
            
            UI::ShiftCursor(8.0f, 8.0f);
            // 提交时机（对齐 Inspector 常规输入体验）：
            // - Enter：ImGuiInputTextFlags_EnterReturnsTrue 令 Enter 结束编辑（不加此 flag 则 Enter 会插入换行且不释放焦点）
            // - 失焦（点击别处 / 切换控件 / 切换窗口）：由 IsItemDeactivatedAfterEdit() 命中
            // - Esc：ImGui 会把 buffer 恢复为进入编辑时的值再 deactivate，此时"未产生编辑"，
            //        IsItemDeactivatedAfterEdit() 返回 false，天然实现"Esc 不提交、保留原名"
            UI::InputText("##Name", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue);
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                entity.SetName(std::string(buffer));
            }
            UI::ShiftCursorY(8.0f);
        }

        // 主组件列表：Registry 循环，按注册顺序绘制拥有的组件
        // 有 Draw 回调的组件才绘制（Name / Relationship 无 Draw，跳过）
        ComponentRegistry::ForEach([&](const ComponentDescriptor& desc)
        {
            if (desc.Draw && desc.Has && desc.Has(entity))
            {
                DrawComponentHeader(entity, desc);
            }
        });

        // 材质编辑器附加块（不属于任何单个组件的 Draw）
        DrawMaterialEditors(entity);

        UI::Draw::HorizontalLine();

        // 添加组件按钮（居中、固定宽度、点击弹出组件菜单）
        DrawAddComponentButton(entity);
    }

    void InspectorPanel::DrawComponentHeader(Entity entity, const ComponentDescriptor& desc)
    {
        // 树节点标志：打开|框架|延伸到右边|允许重叠|框架边框
        const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;

        // 生成唯一 ID：ComponentType + 实体 UUID
        const std::string& strComponentID = std::format("{}##{}{}",
            desc.Name,
            static_cast<uint64_t>(entity.GetUUID()),
            static_cast<int>(desc.Type));

        bool opened = false;

        ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail();
        float lineHeight = ImGui::GetTextLineHeight();

        UI::Draw::HorizontalLine();

        UI::ShiftCursorY(1.0f);
        {
            UI::ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, { 0, 0 });   // 树节点和底部水平线之间的 Spacing
            opened = ImGui::TreeNodeEx(strComponentID.c_str(), flags, "");

            // 组件图标 + 组件名
            ImGui::SameLine();
            UI::ShiftCursorX(UI::Theme::Layout::ComponentHeaderIconSpacing);

            const Ref<Texture2D>& componentIcon = desc.GetIcon ? desc.GetIcon(entity) : EditorIconManager::GetComponentIcon(desc.Type);
            if (componentIcon)
            {
                float iconSize = lineHeight - UI::Theme::Layout::TreeNodeIconSizeShrink;
                UI::ShiftCursorY(UI::Theme::Layout::ComponentHeaderIconOffsetY);
                UI::ImageFlipped(componentIcon, ImVec2(iconSize, iconSize));
                ImGui::SameLine();
                UI::ShiftCursorX(UI::Theme::Layout::ComponentHeaderIconToTextSpacing);
                UI::ShiftCursorY(-UI::Theme::Layout::ComponentHeaderIconOffsetY);
            }

            {
                UI::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[0]);    // TODO 封装 Fonts
                ImGui::TextUnformatted(desc.Name.c_str());
            }

            ImGui::SameLine(contentRegionAvail.x - lineHeight);
            UI::ShiftCursorY(UI::Theme::Layout::ComponentHeaderIconOffsetY * 0.5f);

            // 设置按钮
            const Ref<Texture2D>& settingsIcon = EditorIconManager::GetSettingsIcon();
            {
                ColorSettings& colorSettings = EditorPreferences::Get().GetColors();

                UI::ScopedStyle buttonBorderSize(ImGuiStyleVar_FrameBorderSize, 0.0f);
                UI::ScopedColor buttonColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                UI::ScopedColor buttonActiveColor(ImGuiCol_ButtonActive, { colorSettings.ButtonHovered.x, colorSettings.ButtonHovered.y, colorSettings.ButtonHovered.z, colorSettings.ButtonHovered.w });
                if (UI::ImageButtonFlipped(settingsIcon, ImVec2(lineHeight, lineHeight), 0))
                {
                    ImGui::OpenPopup("ComponentSettings");
                }
            }

            ImGui::Indent(-UI::Theme::Layout::IndentSpacing);
            UI::Draw::HorizontalLine(0.6f);
            ImGui::Indent(UI::Theme::Layout::IndentSpacing);
        }

        // 移除组件
        bool componentRemoved = false;
        if (UI::BeginPopup("ComponentSettings"))
        {
            if (desc.CanRemove && desc.Remove)
            {
                if (ImGui::MenuItem("Remove Component"))
                {
                    componentRemoved = true;
                }
            }

            UI::EndPopup();
        }

        if (opened)
        {
            desc.Draw(entity);
            ImGui::TreePop();
        }

        if (componentRemoved)
        {
            desc.Remove(entity);
        }
    }

    void InspectorPanel::DrawMaterialEditors(Entity entity)
    {
        if (entity.HasComponent<MeshRendererComponent>())
        {
            MeshRendererComponent& meshRenderer = entity.GetComponent<MeshRendererComponent>();
            for (Ref<Material>& material : meshRenderer.Materials)
            {
                if (material)
                {
                    MaterialEditor::OnGUI(material);
                }
            }
        }

        if (entity.HasComponent<SpriteRendererComponent>())
        {
            SpriteRendererComponent& spriteRenderer = entity.GetComponent<SpriteRendererComponent>();
            if (spriteRenderer.Material)
            {
                MaterialEditor::OnGUI(spriteRenderer.Material);
            }
        }
    }

    void InspectorPanel::DrawAddComponentButton(Entity entity)
    {
        constexpr float buttonWidth = 300.0f;
        constexpr float topSpacing = 8.0f;
        const char* popupID = "AddComponentPopup";

        UI::ShiftCursorY(topSpacing);

        // 水平居中：在当前可用区域内偏移使按钮居中
        float availWidth = ImGui::GetContentRegionAvail().x;
        float offsetX = (availWidth - buttonWidth) * 0.5f;
        if (offsetX > 0.0f)
        {
            UI::ShiftCursorX(offsetX);
        }

        // 记录按钮矩形，用于将 Popup 定位到按钮正下方
        ImVec2 buttonMin = ImGui::GetCursorScreenPos();
        if (ImGui::Button("Add Component", ImVec2(buttonWidth, 0.0f)))
        {
            ImGui::OpenPopup(popupID);
        }
        ImVec2 buttonMax = ImGui::GetItemRectMax();

        // Popup 对齐到按钮正下方，宽度与按钮一致
        ImGui::SetNextWindowPos(ImVec2(buttonMin.x, buttonMax.y));
        ImGui::SetNextWindowSizeConstraints(ImVec2(buttonWidth, 0.0f), ImVec2(buttonWidth, FLT_MAX));

        if (UI::BeginPopup(popupID))
        {
            // Registry 循环：每个组件按其 AddMenuItems 依次绘制菜单项
            // Light 三子类型（Directional / Point / Spot）通过 AddMenuItems 3 项表达，同属 LightComponent
            ComponentRegistry::ForEach([&](const ComponentDescriptor& desc)
            {
                if (desc.AddMenuItems.empty())
                {
                    return;
                }

                bool alreadyHas = desc.Has && desc.Has(entity);
                for (const ComponentAddMenuItem& item : desc.AddMenuItems)
                {
                    const Ref<Texture2D>& icon = item.GetIcon ? item.GetIcon() : EditorIconManager::GetComponentIcon(desc.Type);
                    if (UI::IconMenuItem(icon, item.Label.c_str(), alreadyHas))
                    {
                        item.AddFn(entity);
                    }
                }
            });

            UI::EndPopup();
        }
    }

    void InspectorPanel::OnEvent(Event& event)
    {
        
    }
}
