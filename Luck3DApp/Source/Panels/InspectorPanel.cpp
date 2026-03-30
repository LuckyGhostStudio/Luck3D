#include "InspectorPanel.h"

#include "Lucky/Scene/SelectionManager.h"

#include "Lucky/Scene/Components/NameComponent.h"
#include "Lucky/Scene/Components/TransformComponent.h"
#include "Lucky/Scene/Components/MeshFilterComponent.h"
#include "Lucky/Scene/Components/MeshRendererComponent.h"

#include "Lucky/Renderer/Renderer3D.h"

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
        DrawComponent<MeshRendererComponent>("Mesh Renderer", entity, [&entity](MeshRendererComponent& meshRenderer)
        {
            materials = meshRenderer.Materials;
            
            const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;
            
            const std::string& strID = std::format("Materials##{0}", entity.GetComponent<NameComponent>().Name);
            bool opened = ImGui::TreeNodeEx(strID.c_str(), flags);
            if (opened)
            {
                int materialSize = materials.size();
                ImGui::InputInt("Size", &materialSize);
                
                // 材质列表
                for (int i = 0; i < meshRenderer.Materials.size(); i++)
                {
                    const std::string& label = std::format("Element {0}", i);
                    const std::string& materialName = meshRenderer.Materials[i]->GetName();
                
                    char buffer[256];                                   // 输入框内容 buffer
                    memset(buffer, 0, sizeof(buffer));                  // 将 buffer 置零
                    strcpy_s(buffer, sizeof(buffer), materialName.c_str()); // buffer = name
                
                    ImGui::InputText(label.c_str(), buffer, sizeof(buffer));
                }
                
                ImGui::TreePop();
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
            material = Renderer3D::GetInternalErrorMaterial();  // 使用内部错误材质（表示材质丢失）
        }
        
        // 树节点标志：打开|框架|延伸到右边|允许重叠|框架边框
        const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
        
        const std::string& name = std::format("{0} (Material)", material->GetName());
        
        float treeNodePosY = ImGui::GetCursorPosY();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 30)); // 增加 TreeNode 标题行高度
        bool opened = ImGui::TreeNodeEx(std::format("##{0}", name).c_str(), flags);
        ImGui::PopStyleVar();
        
        ImGui::SameLine(45);
        ImGui::SetCursorPosY(treeNodePosY - 25);
        ImGui::Text(name.c_str());
        
        // ---- Shader ----
        
        // 所有可用 Shader 名称
        std::vector<std::string> shaderNames = Renderer3D::GetShaderLibrary()->GetShaderNameList();

        // 当前 Shader 名称
        std::string currentShaderName = material->GetShader()->GetName();

        // 查找当前 Shader 在列表中的索引
        int currentIndex = -1;
        for (int i = 0; i < (int)shaderNames.size(); i++)
        {
            if (shaderNames[i] == currentShaderName)
            {
                currentIndex = i;
                break;
            }
        }
        
        ImGui::SetCursorPos({ 45, treeNodePosY + 40 });
        
        // 绘制下拉选择框
        if (ImGui::BeginCombo(std::format("Shader##{0}", currentShaderName).c_str(), currentShaderName.c_str()))
        {
            for (int i = 0; i < (int)shaderNames.size(); i++)
            {
                bool isSelected = (i == currentIndex);

                if (ImGui::Selectable(shaderNames[i].c_str(), isSelected))
                {
                    // 选择了新的 Shader
                    if (i != currentIndex)
                    {
                        auto newShader = Renderer3D::GetShaderLibrary()->Get(shaderNames[i]);
                        material->SetShader(newShader);  // 触发属性重建
                    }
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        
        // -----------------
        
        ImGui::SetCursorPosY(treeNodePosY + 88);
        
        if (opened)
        {
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
                    if (!texture)
                    {
                        texture = Renderer3D::GetWhiteTexture();    // 默认使用白色纹理
                    }
                        
                    uint32_t textureID = texture->GetRendererID();
                    if (ImGui::ImageButton((ImTextureID)textureID, { 64, 64 }, { 0, 1 }, { 1, 0 }))
                    {
                        std::string filepath = FileDialogs::OpenFile("Albedo Texture(*.png;*.jpg)\0*.png;*.jpg\0");
                        if (!filepath.empty())
                        {
                            material->SetTexture(prop.Name, Texture2D::Create(filepath));
                        }
                    }
                        
                    break;
                }
                default:
                    break;  // TODO 其他类型
                }
            }

            ImGui::TreePop();
        }
    }
}
