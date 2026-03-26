#include "InspectorPanel.h"

#include "Lucky/Scene/SelectionManager.h"

#include "Lucky/Scene/Components/NameComponent.h"
#include "Lucky/Scene/Components/TransformComponent.h"
#include "Lucky/Scene/Components/MeshFilterComponent.h"

namespace Lucky
{
    InspectorPanel::InspectorPanel(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
        
    }

    void InspectorPanel::OnUpdate(DeltaTime dt)
    {
        
    }

    void InspectorPanel::OnGUI()
    {
        if (SelectionManager::GetSelection() != 0)
        {
            DrawComponents(m_Scene->GetEntityWithUUID(SelectionManager::GetSelection()));
        }
    }

    void InspectorPanel::DrawComponents(Entity entity)
    {
        // Name 组件
        if (entity.HasComponent<NameComponent>())
        {
            std::string& name = entity.GetComponent<NameComponent>().Name;   // 物体名

            char buffer[256];                               // 输入框内容 buffer
            memset(buffer, 0, sizeof(buffer));              // 将 buffer 置零
            strcpy_s(buffer, sizeof(buffer), name.c_str()); // buffer = name
            
            if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
            {
                name = std::string(buffer); // 设置 name
            }
        }
        
        // Transform 组件
        DrawComponent<TransformComponent>("Transform", entity, [](TransformComponent& transform)
        {
            float translation[3] = { transform.Translation.x, transform.Translation.y, transform.Translation.z };
            if (ImGui::DragFloat3("Position", translation, 0.1f))
            {
                transform.Translation = { translation[0], translation[1], translation[2] };
            }
            
            float rotation[3] = { transform.GetRotationEuler().x, transform.GetRotationEuler().y, transform.GetRotationEuler().z };
            if (ImGui::DragFloat3("Rotation", rotation, 0.1f))
            {
                transform.SetRotationEuler({ rotation[0], rotation[1], rotation[2] });
            }
            
            float scale[3] = { transform.Scale.x, transform.Scale.y, transform.Scale.z };
            if (ImGui::DragFloat3("Scale", scale, 0.1f))
            {
                transform.Scale = { scale[0], scale[1], scale[2] };
            }
        });
        
        // MeshFilter 组件
        static std::string meshName;
        DrawComponent<MeshFilterComponent>(meshName + " (Mesh Filter)", entity, [](MeshFilterComponent& meshFilter)
        {
            meshName = meshFilter.Mesh->GetName();
            
            char buffer[256];                                   // 输入框内容 buffer
            memset(buffer, 0, sizeof(buffer));                  // 将 buffer 置零
            strcpy_s(buffer, sizeof(buffer), meshName.c_str()); // buffer = name
            
            ImGui::InputText("Mesh", buffer, sizeof(buffer));
        });
    }

    void InspectorPanel::OnEvent(Event& event)
    {
        
    }
}
