#include "SceneHierarchyPanel.h"

#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/NameComponent.h"
#include "Lucky/Scene/Components/MeshFilterComponent.h"

#include "Lucky/Renderer/MeshFactory.h"

#include "Lucky/Scene/SelectionManager.h"

#include "imgui/imgui.h"

namespace Lucky
{
    SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
        
    }

    void SceneHierarchyPanel::OnUpdate(DeltaTime dt)
    {
        
    }

    void SceneHierarchyPanel::OnGUI()
    {
        std::string sceneName = m_Scene->GetName();
        
        // 树结点标志
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

        // 哈希值作为结点 id
        bool opened = ImGui::TreeNodeEx((void*)typeid(Scene).hash_code(), flags, sceneName.c_str());
        
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
            
            ImGui::TreePop();
        }
        
        // 点击鼠标 && 鼠标悬停在该窗口（点击空白位置）
        if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered())
        {
            SelectionManager::Deselect();   // 取消选中
        }
        
        // 创建物体 右键点击窗口白区域弹出菜单：- 右键 不在物体项上
        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            DrawEntityCreateMenu({});   // 绘制创建物体菜单
            
            ImGui::EndPopup();
        }
    }
    
    void SceneHierarchyPanel::DrawEntityNode(Entity entity)
    {
        std::string& name = entity.GetComponent<NameComponent>().Name;  // 物体名
        UUID id = entity.GetUUID();
        const std::string strID = std::format("{0}{1}", name, (uint64_t)entity.GetUUID());

        bool isLeaf = entity.GetChildren().empty(); // 是叶节点
        
        // 树结点标志
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanAvailWidth;
        
        if (SelectionManager::IsSelected(entity.GetUUID()))
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        
        if (isLeaf)
        {
            flags |= ImGuiTreeNodeFlags_Leaf;   // 叶结点没有箭头
        }
        
        bool opened = ImGui::TreeNodeEx(strID.c_str(), flags, name.c_str());
        
        // 树结点被点击
        if (ImGui::IsItemClicked())
        {
            SelectionManager::Select(id);   // 选中物体
            
            LF_TRACE("Selected Entity: [ENTT = {0}, UUID {1}, Name {2}]", (uint32_t)entity, id, entity.GetName());
        }

        const std::string rightClickPopupID = std::format("{0}-ContextMenu", strID);
        
        // 删除物体
        bool entityDeleted = false;
        // 右键点击该物体结点
        if (ImGui::BeginPopupContextItem(rightClickPopupID.c_str()))
        {
            DrawEntityCreateMenu(entity);   // 绘制创建物体菜单
            
            // 菜单项：删除物体
            if (ImGui::MenuItem("Delete"))
            {
                entityDeleted = true;   // 标记为已删除：渲染结束后面的 UI 再删除该物体
            }

            ImGui::EndPopup();
        }

        // 树结点已打开
        if (opened)
        {
            // 子节点
            for (UUID childID : entity.GetChildren())
            {
                DrawEntityNode(m_Scene->GetEntityWithUUID(childID));
            }

            ImGui::TreePop();
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
            newEntity = m_Scene->CreateEntity("Entity");
        }
        // 创建 Cube
        if (ImGui::MenuItem("Cube"))
        {
            newEntity = m_Scene->CreateEntity("Cube");
            Ref<Mesh> cubeMesh = MeshFactory::CreateCube();
            cubeMesh->SetName("Cube");  // Temp
            newEntity.AddComponent<MeshFilterComponent>(cubeMesh);
        }
        
        if (newEntity)
        {
            if (parent)
            {
                newEntity.SetParent(parent);
            }

            SelectionManager::Deselect();
            SelectionManager::Select(newEntity.GetUUID());
        }
        
        ImGui::EndMenu();
    }

    void SceneHierarchyPanel::OnEvent(Event& event)
    {
        
    }
}
