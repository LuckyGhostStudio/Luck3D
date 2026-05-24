#include "SceneHierarchyPanel.h"

#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/Components.h"
#include "Lucky/Scene/Components/ComponentType.h"

#include "Lucky/Renderer/MeshFactory.h"
#include "Lucky/Renderer/Renderer3D.h"

#include "Lucky/Scene/SelectionManager.h"

#include "Lucky/Editor/EditorIconManager.h"

#include "Lucky/UI/Widgets.h"
#include "Lucky/UI/Theme.h"
#include "imgui/imgui.h"
#include <imgui/imgui_internal.h>

namespace Lucky
{
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
            for (auto entityID : m_Scene->GetAllEntitiesWith<IDComponent, RelationshipComponent>())
            {
                Entity entity{ entityID, m_Scene.get() };

                // 绘制没有父节点的实体
                if (entity.GetParentUUID() == 0)
                {
                    DrawEntityNode(entity);
                }
            }
            
            UI::EndTreeNode();
        }
        
        // 点击鼠标 && 鼠标悬停在该窗口（点击空白位置）
        if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered())
        {
            SelectionManager::Deselect();   // 取消选中
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
            newEntity = m_Scene->CreateEntity(uniqueName);
        }
        
        // 创建 3D Object
        if (ImGui::BeginMenu("3D Object"))
        {
            // 创建 Cube
            if (ImGui::MenuItem("Cube"))
            {
                std::string uniqueName = GenerateUniqueName("Cube", parent);
                newEntity = m_Scene->CreateEntity(uniqueName);
            
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
                newEntity = m_Scene->CreateEntity(uniqueName);
                newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Plane);
                MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
                meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
            }
            
            // 创建 Sphere
            if (ImGui::MenuItem("Sphere"))
            {
                std::string uniqueName = GenerateUniqueName("Sphere", parent);
                newEntity = m_Scene->CreateEntity(uniqueName);
                newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Sphere);
                MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
                meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
            }
            
            // 创建 Cylinder
            if (ImGui::MenuItem("Cylinder"))
            {
                std::string uniqueName = GenerateUniqueName("Cylinder", parent);
                newEntity = m_Scene->CreateEntity(uniqueName);
                newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Cylinder);
                MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
                meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
            }
            
            // 创建 Capsule
            if (ImGui::MenuItem("Capsule"))
            {
                std::string uniqueName = GenerateUniqueName("Capsule", parent);
                newEntity = m_Scene->CreateEntity(uniqueName);
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
                newEntity = m_Scene->CreateEntity(uniqueName);
                newEntity.AddComponent<LightComponent>(LightType::Directional);
            
                // 设置初始方向斜向下
                TransformComponent& transform = newEntity.GetComponent<TransformComponent>();
                transform.SetRotationEuler(glm::vec3(glm::radians(50.0f), glm::radians(-32.0f), 0.0f));
            }
        
            // 创建 Point Light
            if (ImGui::MenuItem("Point Light"))
            {
                std::string uniqueName = GenerateUniqueName("Point Light", parent);
                newEntity = m_Scene->CreateEntity(uniqueName);
                newEntity.AddComponent<LightComponent>(LightType::Point);
            }

            // 创建 Spot Light
            if (ImGui::MenuItem("Spot Light"))
            {
                std::string uniqueName = GenerateUniqueName("Spot Light", parent);
                newEntity = m_Scene->CreateEntity(uniqueName);
                newEntity.AddComponent<LightComponent>(LightType::Spot);
            }
            
            ImGui::EndMenu();
        }

        // 创建 Post Process Volume
        if (ImGui::MenuItem("Post Process Volume"))
        {
            std::string uniqueName = GenerateUniqueName("Post Process Volume", parent);
            newEntity = m_Scene->CreateEntity(uniqueName);
            newEntity.AddComponent<PostProcessVolumeComponent>();
        }
        
        if (newEntity)
        {
            if (parent)
            {
                newEntity.SetParentUUID(parent.GetUUID());
                parent.GetChildren().push_back(newEntity.GetUUID());
            }

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

        // 收集该实体拥有的非默认组件类型（排除 Transform）
        std::vector<ComponentType> types;
        if (entity.HasComponent<MeshFilterComponent>())         types.push_back(ComponentType::MeshFilter);
        if (entity.HasComponent<MeshRendererComponent>())       types.push_back(ComponentType::MeshRenderer);
        if (entity.HasComponent<LightComponent>())              types.push_back(ComponentType::Light);
        if (entity.HasComponent<PostProcessVolumeComponent>())  types.push_back(ComponentType::PostProcessVolume);

        if (types.empty())
        {
            g.LastItemData = savedItemData;
            return;
        }

        float iconSize = ImGui::GetTextLineHeight() - UI::Theme::Layout::TreeNodeIconSizeShrink;
        float iconSpacing = UI::Theme::Layout::TreeNodeComponentIconSpacing;
        float minGap = UI::Theme::Layout::TreeNodeComponentIconMinGap;

        // 计算图标区域总宽度
        float totalIconWidth = types.size() * iconSize + (types.size() - 1) * iconSpacing;

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
        while (!types.empty() && iconsStartLocal < nameEndLocal + minGap)
        {
            types.erase(types.begin());
            if (types.empty())
            {
                g.LastItemData = savedItemData;
                return;
            }
            totalIconWidth = types.size() * iconSize + (types.size() - 1) * iconSpacing;
            iconsStartLocal = contentRightLocal - totalIconWidth;
        }

        // 保存当前光标位置
        ImVec2 savedCursorPos = ImGui::GetCursorPos();

        // 定位到图标绘制位置（与当前行同行）
        float lineY = ImGui::GetItemRectMin().y - ImGui::GetWindowPos().y + ImGui::GetScrollY();
        float iconOffsetY = (ImGui::GetTextLineHeight() - iconSize) * 0.5f;

        ImGui::SetCursorPos(ImVec2(iconsStartLocal, lineY + iconOffsetY));

        for (size_t i = 0; i < types.size(); i++)
        {
            const Ref<Texture2D>& compIcon = EditorIconManager::GetComponentIcon(types[i]);
            if (compIcon)
            {
                ImTextureID texID = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(compIcon->GetRendererID()));
                ImGui::Image(texID, ImVec2(iconSize, iconSize));
            }

            if (i < types.size() - 1)
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
}
