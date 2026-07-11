#include "SceneHierarchyPanel.h"

#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/Components.h"
#include "Lucky/Scene/Components/ComponentType.h"

#include "Lucky/Renderer/MeshFactory.h"
#include "Lucky/Renderer/Renderer3D.h"

#include "Lucky/Scene/SelectionManager.h"

#include "Lucky/Editor/EditorIconManager.h"
#include "Lucky/Editor/DragDropPayloads.h"
#include "Lucky/Editor/DragDropContext.h"
#include "Lucky/Editor/DragDropVisuals.h"

#include "Lucky/UI/Widgets.h"
#include "Lucky/UI/Theme.h"
#include "imgui/imgui.h"
#include <imgui/imgui_internal.h>

namespace Lucky
{
    namespace
    {
        /// <summary>
        /// 拖拽放置位置（Hierarchy 面板内部实现细节，不暴露到头文件）
        /// </summary>
        enum class DropPosition
        {
            Before,     // 插入到目标节点前面（同级）
            Inside,     // 成为目标节点的子节点
            After       // 插入到目标节点后面（同级）
        };

        /// <summary>
        /// 根据鼠标在 LastItem 矩形中的相对 Y 位置计算放置模式
        /// 25% / 50% / 25% 三分区，与 Unity Hierarchy 交互一致
        /// </summary>
        DropPosition ComputeDropPosition()
        {
            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 itemMin = ImGui::GetItemRectMin();
            ImVec2 itemMax = ImGui::GetItemRectMax();
            float itemHeight = itemMax.y - itemMin.y;
            if (itemHeight <= 0.0f)
            {
                return DropPosition::Inside;
            }

            float relativeY = (mousePos.y - itemMin.y) / itemHeight;
            if (relativeY < 0.25f) return DropPosition::Before;
            if (relativeY > 0.75f) return DropPosition::After;
            return DropPosition::Inside;
        }
    }

    SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
        
    }

    void SceneHierarchyPanel::SetScene(const Ref<Scene>& scene)
    {
        m_Scene = scene;
    }

    void SceneHierarchyPanel::OnUpdate(DeltaTime dt)
    {
        
    }

    void SceneHierarchyPanel::OnGUI()
    {
        std::string sceneName = m_Scene->GetName();
        
        const std::string& strSceneID = std::format("{0}##{1}", sceneName, typeid(Scene).hash_code());
        
        const Ref<Texture2D>& icon = EditorIconManager::GetAssetTypeIcon(AssetType::Scene);
        
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);    // TODO 封装 Fonts
        bool opened = UI::BeginTreeNode(icon, strSceneID.c_str(), true);
        ImGui::PopFont();
        
        if (opened)
        {
            // 按 m_RootEntityOrder 顺序绘制根节点
            const std::vector<UUID>& rootOrder = m_Scene->GetRootEntityOrder();
            for (UUID rootID : rootOrder)
            {
                Entity entity = m_Scene->TryGetEntityWithUUID(rootID);
                if (entity)
                {
                    DrawEntityNode(entity);
                }
                else
                {
                    // 兜底：m_RootEntityOrder 与 m_EntityIDMap 不一致时（理论上不应发生）
                    LF_CORE_WARN("[Hierarchy] Root entity UUID {0} not found in scene", static_cast<uint64_t>(rootID));
                }
            }
            
            UI::EndTreeNode();
        }
        
        // 点击鼠标 && 鼠标悬停在该窗口（点击空白位置）
        if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered())
        {
            SelectionManager::Deselect();   // 取消选中
        }
        
        // ---- 空白区域拖拽目标：拖到空白 → 变为根节点（末尾） ----
        // 注意：这里必须使用 BeginDragDropTargetCustom（自定义矩形版本），而不是
        //       InvisibleButton/Dummy + BeginDragDropTarget。原因：
        //       InvisibleButton/Dummy 会创建一个 ImGui Item，把整个空白区域"占据"，导致：
        //         1) 下方 BeginPopupContextWindow(NoOpenOverItems) 判定失败 → 右键菜单弹不出，无法创建物体
        //         2) ImGui::IsWindowHovered / IsMouseClicked 的"点击空白取消选中"逻辑失效
        //       BeginDragDropTargetCustom 直接注册一个纯粹的拖放热区，不产生 Item，天然规避以上副作用
        {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
            ImVec2 windowMax = window->WorkRect.Max;
            ImRect emptyRect(cursorScreenPos, windowMax);

            if (emptyRect.Min.y < emptyRect.Max.y &&
                ImGui::BeginDragDropTargetCustom(emptyRect, ImGui::GetID("##HierarchyEmptyDropArea")))
            {
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                    DragDrop::EntityHierarchy,
                    ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

                if (payload && payload->DataSize == sizeof(UUID))
                {
                    UUID draggedID = *static_cast<UUID*>(payload->Data);
                    Entity dragged = m_Scene->TryGetEntityWithUUID(draggedID);

                    // 空白区域语义："把 dragged 移动到根层级的末尾"（对齐 Unity 表现）：
                    // - dragged 是非根节点：从原父节点移出，追加到 m_RootEntityOrder 末尾
                    // - dragged 已是根节点：在 m_RootEntityOrder 中挪到末尾（若原本就在末尾则位置不变）
                    // 无论是否为根，只要 Entity 有效就视为合法目标，tooltip 显示允许图标
                    bool acceptable = static_cast<bool>(dragged);

                    if (acceptable)
                    {
                        // 通知源端 → tooltip 切换为"允许"图标
                        UI::DragDropContext::NotifyTargetAccepts(DragDrop::EntityHierarchy);

                        // 空白区域默认不绘制视觉反馈（tooltip 图标已足够）

                        if (payload->IsDelivery())
                        {
                            if (dragged.GetParent())
                            {
                                // 非根节点：脱离旧父，追加到根节点末尾
                                dragged.SetParent({});
                            }
                            else
                            {
                                // 已是根节点：在根节点顺序列表中挪到末尾
                                dragged.MoveToIndex(-1);
                            }
                        }
                    }
                }

                ImGui::EndDragDropTarget();
            }
        }
        
        // 创建物体 右键点击窗口白区域弹出菜单：- 右键 不在物体项上
        if (UI::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            DrawEntityCreateMenu({});   // 绘制创建物体菜单
            UI::EndPopup();
        }
    }
    
    void SceneHierarchyPanel::DrawEntityNode(Entity entity)
    {
        const std::string& name = entity.GetComponent<NameComponent>().Name;  // 物体名
        UUID id = entity.GetUUID();
        const std::string& strID = std::format("{0}##{1}", name, static_cast<uint64_t>(id));

        bool isLeaf = entity.GetChildren().empty(); // 是叶节点

        const Ref<Texture2D>& icon = EditorIconManager::GetEntityIcon();
        
        bool opened = UI::BeginTreeNode(icon, strID.c_str(), false, SelectionManager::IsSelected(id), isLeaf);

        // 绘制右侧组件图标列表
        DrawEntityComponentIcons(entity);
        
        // 树结点被点击
        if (ImGui::IsItemClicked())
        {
            SelectionManager::Select(id);   // 选中物体
            
            LF_TRACE("Selected Entity: [ENTT = {0}, UUID {1}, Name {2}]", static_cast<uint32_t>(entity), id, entity.GetName());
        }

        // ---- 拖拽源：与 ProjectAssetsPanel 保持一致的写法 ----
        if (UI::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
        {
            UUID draggedID = entity.GetUUID();
            ImGui::SetDragDropPayload(DragDrop::EntityHierarchy, &draggedID, sizeof(UUID));

            // 源端 tooltip 图标：命中匹配目标时显示"允许"，否则显示"禁止"
            UI::DragDropPreview(UI::DragDropContext::IsRejected(DragDrop::EntityHierarchy));

            UI::EndDragDropSource();
        }

        // ---- 拖拽目标：peek + NotifyTargetAccepts + IsDelivery 分离 ----
        if (ImGui::BeginDragDropTarget())
        {
            // 悬停期间即可拿到 payload；抑制 ImGui 默认高亮矩形，由目标端自绘反馈
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                DragDrop::EntityHierarchy,
                ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

            if (payload && payload->DataSize == sizeof(UUID))
            {
                UUID draggedID = *static_cast<UUID*>(payload->Data);
                Entity dragged = m_Scene->TryGetEntityWithUUID(draggedID);

                // 三分区计算放置模式
                DropPosition dropPos = ComputeDropPosition();

                // 业务校验（自身 / 无效实体 / 循环）
                bool acceptable = dragged && dragged != entity;
                if (acceptable)
                {
                    if (dropPos == DropPosition::Inside)
                    {
                        acceptable = !WouldCreateCycle(entity, dragged);
                    }
                    else
                    {
                        // Before / After 的循环校验目标是 entity 的父节点
                        // WouldCreateCycle 已对空 parent 做了保护
                        acceptable = !WouldCreateCycle(entity.GetParent(), dragged);
                    }
                }

                if (acceptable)
                {
                    // 通知源端：本帧存在可接受目标 → tooltip 切换为"允许"图标
                    UI::DragDropContext::NotifyTargetAccepts(DragDrop::EntityHierarchy);

                    // 目标端主动绘制视觉反馈
                    switch (dropPos)
                    {
                        case DropPosition::Before: UI::DragDropVisuals::HighlightTargetInsertLine(true);  break;
                        case DropPosition::After:  UI::DragDropVisuals::HighlightTargetInsertLine(false); break;
                        case DropPosition::Inside: UI::DragDropVisuals::HighlightTargetNode();            break;
                    }

                    // 只有鼠标释放时才真正执行层级变更
                    if (payload->IsDelivery())
                    {
                        switch (dropPos)
                        {
                            case DropPosition::Inside:
                            {
                                dragged.SetParent(entity);
                                break;
                            }
                            case DropPosition::Before:
                            {
                                Entity targetParent = entity.GetParent();
                                int targetIndex = m_Scene->GetEntityIndexInParent(entity);
                                dragged.SetParent(targetParent, targetIndex);
                                break;
                            }
                            case DropPosition::After:
                            {
                                Entity targetParent = entity.GetParent();
                                int targetIndex = m_Scene->GetEntityIndexInParent(entity);
                                dragged.SetParent(targetParent, targetIndex + 1);
                                break;
                            }
                        }
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }

        const std::string rightClickPopupID = std::format("{0}-ContextMenu", strID);
        
        // 删除物体
        bool entityDeleted = false;
        // 右键点击该物体结点
        if (UI::BeginPopupContextItem(rightClickPopupID.c_str(), ImGuiPopupFlags_MouseButtonRight))
        {
            SelectionManager::Select(id);   // 选中物体
            
            // 菜单项：删除物体
            if (ImGui::MenuItem("Delete"))
            {
                entityDeleted = true;   // 标记为已删除：渲染结束后面的 UI 再删除该物体
            }
            
            ImGui::Separator();
            
            DrawEntityCreateMenu(entity);   // 绘制创建物体菜单

            UI::EndPopup();
        }

        // 树结点已打开
        if (opened)
        {
            // 拷贝子节点列表后再遍历：避免遍历过程中原始列表被修改导致跳过子节点
            std::vector<UUID> children = entity.GetChildren();
            for (UUID childID : children)
            {
                Entity child = m_Scene->GetEntityWithUUID(childID);
                DrawEntityNode(child);
            }

            UI::EndTreeNode();
        }
        
        if (entityDeleted)
        {
            m_Scene->DestroyEntity(entity); // 销毁物体
            
            // 删除的物体为已选中物体
            if (SelectionManager::IsSelected(id))
            {
                SelectionManager::Deselect();   // 清空选中项
            }
        }
    }

    void SceneHierarchyPanel::DrawEntityCreateMenu(Entity parent)
    {
        if (!ImGui::BeginMenu("Create"))
        {
            return;
        }
        
        Entity newEntity;
        
        // 创建空物体
        if (ImGui::MenuItem("Create Empty"))
        {
            std::string uniqueName = GenerateUniqueName("Entity", parent);
            newEntity = m_Scene->CreateEntity(uniqueName, parent);
        }
        
        // 创建 3D Object
        if (ImGui::BeginMenu("3D Object"))
        {
            // 创建 Cube
            if (ImGui::MenuItem("Cube"))
            {
                std::string uniqueName = GenerateUniqueName("Cube", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
            
                // MeshFilter
                newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Cube);
            
                // MeshRenderer
                MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
                meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());  // 使用默认材质
            }
            
            // 创建 Plane
            if (ImGui::MenuItem("Plane"))
            {
                std::string uniqueName = GenerateUniqueName("Plane", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Plane);
                MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
                meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
            }
            
            // 创建 Sphere
            if (ImGui::MenuItem("Sphere"))
            {
                std::string uniqueName = GenerateUniqueName("Sphere", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Sphere);
                MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
                meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
            }
            
            // 创建 Cylinder
            if (ImGui::MenuItem("Cylinder"))
            {
                std::string uniqueName = GenerateUniqueName("Cylinder", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Cylinder);
                MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
                meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
            }
            
            // 创建 Capsule
            if (ImGui::MenuItem("Capsule"))
            {
                std::string uniqueName = GenerateUniqueName("Capsule", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Capsule);
                MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
                meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
            }
            
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Light"))
        {
            // 创建 Directional Light
            if (ImGui::MenuItem("Directional Light"))
            {
                std::string uniqueName = GenerateUniqueName("Directional Light", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<LightComponent>(LightType::Directional);
            
                // 设置初始方向斜向下
                TransformComponent& transform = newEntity.GetComponent<TransformComponent>();
                transform.SetRotationEuler(glm::vec3(glm::radians(50.0f), glm::radians(-32.0f), 0.0f));
            }
        
            // 创建 Point Light
            if (ImGui::MenuItem("Point Light"))
            {
                std::string uniqueName = GenerateUniqueName("Point Light", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<LightComponent>(LightType::Point);
            }

            // 创建 Spot Light
            if (ImGui::MenuItem("Spot Light"))
            {
                std::string uniqueName = GenerateUniqueName("Spot Light", parent);
                newEntity = m_Scene->CreateEntity(uniqueName, parent);
                newEntity.AddComponent<LightComponent>(LightType::Spot);
            }
            
            ImGui::EndMenu();
        }

        // 创建 Post Process Volume
        if (ImGui::MenuItem("Post Process Volume"))
        {
            std::string uniqueName = GenerateUniqueName("Post Process Volume", parent);
            newEntity = m_Scene->CreateEntity(uniqueName, parent);
            newEntity.AddComponent<PostProcessVolumeComponent>();
        }
        
        if (newEntity)
        {
            SelectionManager::Deselect();
            SelectionManager::Select(newEntity.GetUUID());
        }
        
        ImGui::EndMenu();
    }

    void SceneHierarchyPanel::DrawEntityComponentIcons(Entity entity)
    {
        // 保存 TreeNode 的 LastItemData，绘制完图标后恢复，确保调用方的 IsItemClicked 等交互检测正确工作
        ImGuiContext& g = *GImGui;
        const ImGuiLastItemData savedItemData = g.LastItemData;

        // 收集该实体拥有的非默认组件图标（排除 Transform）
        std::vector<const Ref<Texture2D>*> icons;
        if (entity.HasComponent<MeshFilterComponent>())         icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::MeshFilter));
        if (entity.HasComponent<MeshRendererComponent>())       icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::MeshRenderer));
        if (entity.HasComponent<LightComponent>())
        {
            LightType lightType = entity.GetComponent<LightComponent>().Type;
            icons.push_back(&EditorIconManager::GetLightIcon(lightType));
        }
        if (entity.HasComponent<PostProcessVolumeComponent>())  icons.push_back(&EditorIconManager::GetComponentIcon(ComponentType::PostProcessVolume));

        if (icons.empty())
        {
            g.LastItemData = savedItemData;
            return;
        }

        float iconSize = ImGui::GetTextLineHeight() - UI::Theme::Layout::TreeNodeIconSizeShrink;
        float iconSpacing = UI::Theme::Layout::TreeNodeComponentIconSpacing;
        float minGap = UI::Theme::Layout::TreeNodeComponentIconMinGap;

        // 计算图标区域总宽度
        float totalIconWidth = icons.size() * iconSize + (icons.size() - 1) * iconSpacing;

        // 获取可用区域右边界（使用窗口内相对坐标，减去右边距）
        float contentRightLocal = ImGui::GetContentRegionMax().x - UI::Theme::Layout::TreeNodeComponentIconRightMargin;
        float iconsStartLocal = contentRightLocal - totalIconWidth;

        // 计算名称文本的实际右边界（窗口内相对坐标）
        // 注意：GetItemRectMax 返回的是 TreeNode 的 Rect（SpanFullWidth 导致其跨越整行），
        // 所以需要通过 TreeNode 左边界 + 缩进 + 箭头 + 图标 + 文本宽度 来计算名称的实际右边界
        const std::string& name = entity.GetComponent<NameComponent>().Name;
        float textWidth = ImGui::CalcTextSize(name.c_str()).x;

        // TreeNode 的左边界（ItemRectMin 是屏幕坐标，转为窗口内坐标）
        float itemLeftLocal = ImGui::GetItemRectMin().x - ImGui::GetWindowPos().x + ImGui::GetScrollX();
        // 获取当前窗口的缩进量（TreePush/TreePop 会改变此值）
        float indentWidth = ImGui::GetCurrentWindow()->DC.Indent.x;
        // 箭头宽度（ImGui 内部箭头占 FontSize + FramePadding.x）
        float arrowWidth = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.x;
        // 名称文本的右边界 = TreeNode左边界 + 缩进 + 箭头宽度 + 箭头到图标间距 + 图标尺寸 + 图标到文本间距 + 文本宽度
        float nameEndLocal = itemLeftLocal + indentWidth + arrowWidth
            + UI::Theme::Layout::TreeNodeArrowToIconSpacing
            + iconSize
            + UI::Theme::Layout::TreeNodeIconToTextSpacing
            + textWidth;

        // 碰撞检测：如果图标区域侵入名称区域，则从左侧开始移除图标
        while (!icons.empty() && iconsStartLocal < nameEndLocal + minGap)
        {
            icons.erase(icons.begin());
            if (icons.empty())
            {
                g.LastItemData = savedItemData;
                return;
            }
            totalIconWidth = icons.size() * iconSize + (icons.size() - 1) * iconSpacing;
            iconsStartLocal = contentRightLocal - totalIconWidth;
        }

        // 保存当前光标位置
        ImVec2 savedCursorPos = ImGui::GetCursorPos();

        // 定位到图标绘制位置（与当前行同行）
        float lineY = ImGui::GetItemRectMin().y - ImGui::GetWindowPos().y + ImGui::GetScrollY();
        float iconOffsetY = (ImGui::GetTextLineHeight() - iconSize) * 0.5f;

        ImGui::SetCursorPos(ImVec2(iconsStartLocal, lineY + iconOffsetY));

        for (size_t i = 0; i < icons.size(); i++)
        {
            const Ref<Texture2D>& compIcon = *icons[i];
            if (compIcon)
            {
                UI::ImageFlipped(compIcon, ImVec2(iconSize, iconSize));
            }

            if (i < icons.size() - 1)
            {
                ImGui::SameLine(0, iconSpacing);
            }
        }

        // 恢复光标位置
        ImGui::SetCursorPos(savedCursorPos);

        // 恢复 LastItemData，使调用方的 IsItemClicked 等交互检测正确指向 TreeNode
        g.LastItemData = savedItemData;
    }

    void SceneHierarchyPanel::OnEvent(Event& event)
    {
        
    }

    std::string SceneHierarchyPanel::GenerateUniqueName(const std::string& baseName, Entity parent)
    {
        // 收集同层级下所有兄弟节点的名称
        std::vector<std::string> siblingNames;

        if (parent)
        {
            // 有父节点：遍历父节点的所有子节点
            for (UUID childID : parent.GetChildren())
            {
                Entity child = m_Scene->GetEntityWithUUID(childID);
                siblingNames.push_back(child.GetName());
            }
        }
        else
        {
            // 无父节点：遍历所有根节点（Parent == 0）
            auto view = m_Scene->GetAllEntitiesWith<NameComponent, RelationshipComponent>();
            for (auto entityID : view)
            {
                Entity entity{ entityID, m_Scene.get() };
                if (entity.GetParentUUID() == 0)
                {
                    siblingNames.push_back(entity.GetName());
                }
            }
        }

        // 检查名称是否已存在
        auto nameExists = [&siblingNames](const std::string& name) -> bool
        {
            return std::find(siblingNames.begin(), siblingNames.end(), name) != siblingNames.end();
        };

        // baseName 不重复则直接返回
        if (!nameExists(baseName))
        {
            return baseName;
        }

        // 从 (1) 开始递增直到找到不重复的名称
        int suffix = 1;
        std::string candidateName;
        do
        {
            candidateName = std::format("{} ({})", baseName, suffix);
            suffix++;
        } while (nameExists(candidateName));

        return candidateName;
    }

    bool SceneHierarchyPanel::WouldCreateCycle(Entity potentialParent, Entity potentialChild)
    {
        // 拖到根节点的兄弟位置时 potentialParent 可能是无效 Entity，一定不会形成循环
        if (!potentialParent)
        {
            return false;
        }

        // 不能将节点设为自身的子节点
        if (potentialParent == potentialChild)
        {
            return true;
        }

        // 检查 potentialParent 是否是 potentialChild 的子孙（即 potentialChild 是 potentialParent 的祖先）
        Entity current = potentialParent.GetParent();
        while (current)
        {
            if (current == potentialChild)
            {
                return true;
            }
            current = current.GetParent();
        }

        return false;
    }
}
