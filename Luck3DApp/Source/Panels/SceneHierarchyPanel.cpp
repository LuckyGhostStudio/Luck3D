#include "SceneHierarchyPanel.h"

#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/NameComponent.h"
#include "Lucky/Scene/Components/MeshFilterComponent.h"

#include "Lucky/Renderer/MeshFactory.h"

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
        ImGui::PushID(sceneName.c_str());

        // 树结点标志
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

        // 哈希值作为结点 id
        bool opened = ImGui::TreeNodeEx((void*)typeid(Scene).hash_code(), flags, sceneName.c_str());
        
        if (opened)
        {
            // 遍历场景所有实体，并调用 each 内的函数
            m_Scene->m_Registry.each([&](auto entityID)
            {
                Entity entity{ entityID, m_Scene.get() };

                DrawEntityNode(entity); // 绘制物体结点
            });
            
            ImGui::TreePop();
        }

        ImGui::PopID();
        
        // 创建物体 右键点击窗口白区域弹出菜单：- 右键 不在物体项上
        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            // 创建空物体
            if (ImGui::MenuItem("Create Empty"))
            {
                m_Scene->CreateEntity("Entity");
            }
            // 创建 Cube
            if (ImGui::MenuItem("Cube"))
            {
                Entity newEntity = m_Scene->CreateEntity("Cube");
                newEntity.AddComponent<MeshFilterComponent>(MeshFactory::CreateCube());
            }
            
            ImGui::EndPopup();
        }
    }
    
    void SceneHierarchyPanel::DrawEntityNode(Entity entity)
    {
        std::string& name = entity.GetComponent<NameComponent>().Name;  // 物体名

        // 树结点标志
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanAvailWidth;
        
        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, name.c_str());

        // 树结点被点击
        if (ImGui::IsItemClicked())
        {
            LF_TRACE("Selected Entity: [ENTT = {0}, UUID {1}, Name {2}]", (uint32_t)entity, entity.GetUUID(), entity.GetName());
        }

        // 删除物体
        bool entityDeleted = false;
        // 右键点击该物体结点
        if (ImGui::BeginPopupContextItem())
        {
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
            // TODO 子节点
            

            ImGui::TreePop();
        }

        if (entityDeleted)
        {
            m_Scene->DestroyEntity(entity); // 销毁物体
        }
    }

    void SceneHierarchyPanel::OnEvent(Event& event)
    {
        
    }
}
