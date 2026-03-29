#include "InspectorPanel.h"

#include "Lucky/Scene/SelectionManager.h"

#include "Lucky/Scene/Components/NameComponent.h"
#include "Lucky/Scene/Components/TransformComponent.h"
#include "Lucky/Scene/Components/MeshFilterComponent.h"
#include "Lucky/Scene/Components/MeshRendererComponent.h"
#include "Lucky/Utils/PlatformUtils.h"

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
            ImGui::DragFloat3("Position", &transform.Translation.x, 0.1f);
            
            glm::vec3 rotationEuler = transform.GetRotationEuler();
            if (ImGui::DragFloat3("Rotation", &rotationEuler.x, 0.1f))
            {
                transform.SetRotationEuler(rotationEuler);
            }

            ImGui::DragFloat3("Scale", &transform.Scale.x, 0.1f);
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
        
        static std::vector<Ref<Material>> materials;    // 当前 MeshRenderer 的材质列表
        DrawComponent<MeshRendererComponent>("Mesh Renderer", entity, [](MeshRendererComponent& meshRenderer)
        {
            materials = meshRenderer.Materials;
            
            for (int i = 0; i < meshRenderer.Materials.size(); i++)
            {
                const std::string& label = std::format("Element {0}", i);
                const std::string& materialName = meshRenderer.Materials[i]->GetName();
                
                char buffer[256];                                   // 输入框内容 buffer
                memset(buffer, 0, sizeof(buffer));                  // 将 buffer 置零
                strcpy_s(buffer, sizeof(buffer), materialName.c_str()); // buffer = name
                
                ImGui::InputText(label.c_str(), buffer, sizeof(buffer));
            }
        });
        
        // 绘制所有材质的参数
        for (Ref<Material>& material : materials)
        {
            DrawMaterialEditor(material);
        }
    }

    void InspectorPanel::OnEvent(Event& event)
    {
        
    }

    void InspectorPanel::DrawMaterialEditor(Ref<Material>& material)
    {
        if (!material)
        {
            return;
        }
        
        // 树节点标志：打开|框架|延伸到右边|允许重叠|框架边框
        const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
        
        const std::string name = std::format("{0} (Material)", material->GetName());
        
        bool opened = ImGui::TreeNodeEx(name.c_str(), flags);
        
        if (opened)
        {
            const std::string shaderName = material->GetShader()->GetName();
            
            // 显示 Shader 名字 TODO Shader下拉选择框
            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            strcpy_s(buffer, sizeof(buffer), shaderName.c_str());
                
            ImGui::InputText("Shader", buffer, sizeof(buffer));
            
            // 绘制材质属性
            for (const MaterialProperty& prop : material->GetProperties())
            {
                switch (prop.Type)
                {
                case ShaderUniformType::Float:
                {
                    float value = std::get<float>(prop.Value);
                    if (ImGui::DragFloat(prop.Name.c_str(), &value, 0.1f))
                    {
                        material->SetFloat(prop.Name, value);
                    }
                    break;
                }
                case ShaderUniformType::Float2:
                {
                    glm::vec2 value = std::get<glm::vec2>(prop.Value);
                    if (ImGui::DragFloat2(prop.Name.c_str(), &value.x, 0.1f))
                    {
                        material->SetFloat2(prop.Name, value);
                    }
                    break;
                }
                case ShaderUniformType::Float3:
                {
                    glm::vec3 value = std::get<glm::vec3>(prop.Value);
                    if (ImGui::DragFloat3(prop.Name.c_str(), &value.x, 0.1f))
                    {
                        material->SetFloat3(prop.Name, value);
                    }
                    break;
                }
                case ShaderUniformType::Float4:
                {
                    glm::vec4 value = std::get<glm::vec4>(prop.Value);
                    if (ImGui::DragFloat4(prop.Name.c_str(), &value.x, 0.1f))
                    {
                        material->SetFloat4(prop.Name, value);
                    }
                    break;
                }
                case ShaderUniformType::Int:
                {
                    int value = std::get<int>(prop.Value);
                    if (ImGui::DragInt(prop.Name.c_str(), &value, 0.1f))
                    {
                        material->SetInt(prop.Name, value);
                    }
                    break;
                }
                case ShaderUniformType::Sampler2D:
                {
                    Ref<Texture2D> texture = std::get<Ref<Texture2D>>(prop.Value);
                    if (texture)
                    {
                        uint32_t textureID = texture->GetRendererID();
                        if (ImGui::ImageButton((ImTextureID)textureID, { 64, 64 }, { 0, 1 }, { 1, 0 }))
                        {
                            std::string filepath = FileDialogs::OpenFile("Albedo Texture(*.png;*.jpg)\0*.png;*.jpg\0");
                            if (!filepath.empty())
                            {
                                material->SetTexture(prop.Name, Texture2D::Create(filepath));
                            }
                        }
                    }
                    break;
                }
                default:
                    break;  // TODO 其他类型
                }
            }

            ImGui::TreePop();       // 展开结点
        }
    }
}
