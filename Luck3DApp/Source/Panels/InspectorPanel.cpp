#include "InspectorPanel.h"

#include "Lucky/Scene/SelectionManager.h"

#include "Lucky/Scene/Components/Components.h"

#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/RenderState.h"
#include "Lucky/Renderer/RenderContext.h"

#include "Lucky/Utils/PlatformUtils.h"

#include "glm/gtc/type_ptr.hpp"

namespace Lucky
{
    InspectorPanel::InspectorPanel(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
        
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
        UUID selectionID = SelectionManager::GetSelection();
        if (selectionID != 0)
        {
            DrawComponents(m_Scene->GetEntityWithUUID(selectionID));
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
            ImGui::DragFloat3("Position", &transform.Translation.x, 0.01f);
            
            glm::vec3 rotationEuler = glm::degrees(transform.GetRotationEuler());   // 转为角度
            if (ImGui::DragFloat3("Rotation", &rotationEuler.x, 1.0f))
            {
                transform.SetRotationEuler(glm::radians(rotationEuler));
            }

            ImGui::DragFloat3("Scale", &transform.Scale.x, 0.01f);
        });
        
        // Light 组件
        DrawComponent<LightComponent>("Light", entity, [](LightComponent& light)
        {
            // 光源类型下拉框
            const char* lightTypes[] = { "Directional", "Point", "Spot" };
            int currentType = static_cast<int>(light.Type);
            if (ImGui::Combo("Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
            {
                LightType newType = static_cast<LightType>(currentType);

                // 切换类型时设置合理的默认值
                if (newType != light.Type)
                {
                    light.Type = newType;

                    // 根据新类型设置默认阴影模式
                    if (newType == LightType::Directional)
                    {
                        light.Shadows = ShadowType::Hard;
                    }
                    else
                    {
                        light.Shadows = ShadowType::None;
                    }
                }
            }

            ImGui::Separator();

            // 通用属性
            ImGui::ColorEdit3("Color", glm::value_ptr(light.Color));
            ImGui::DragFloat("Intensity", &light.Intensity, 0.01f, 0.0f, 100.0f);

            // Point / Spot 属性
            if (light.Type == LightType::Point || light.Type == LightType::Spot)
            {
                ImGui::DragFloat("Range", &light.Range, 0.1f, 0.1f, 1000.0f);
            }

            // Spot 属性
            if (light.Type == LightType::Spot)
            {
                ImGui::DragFloat("Inner Cutoff", &light.InnerCutoffAngle, 0.5f, 0.0f, light.OuterCutoffAngle);
                ImGui::DragFloat("Outer Cutoff", &light.OuterCutoffAngle, 0.5f, light.InnerCutoffAngle, 90.0f);
            }

            ImGui::Separator();

            // 阴影属性
            const char* shadowTypes[] = { "No Shadows", "Hard Shadows", "Soft Shadows" };
            int currentShadow = static_cast<int>(light.Shadows);
            if (ImGui::Combo("Shadow Type", &currentShadow, shadowTypes, IM_ARRAYSIZE(shadowTypes)))
            {
                light.Shadows = static_cast<ShadowType>(currentShadow);
            }

            if (light.Shadows != ShadowType::None)
            {
                ImGui::DragFloat("Shadow Bias", &light.ShadowBias, 0.0001f, 0.0f, 0.05f, "%.4f");
                ImGui::DragFloat("Shadow Strength", &light.ShadowStrength, 0.01f, 0.0f, 1.0f);
            }
        });
        
        // PostProcessVolume 组件
        DrawComponent<PostProcessVolumeComponent>("Post Process Volume", entity, [](PostProcessVolumeComponent& volume)
        {
            // Volume 设置
            ImGui::Checkbox("Is Global", &volume.IsGlobal);
            ImGui::DragFloat("Priority", &volume.Priority, 0.1f);

            ImGui::Separator();

            // ---- Tonemapping ----
            if (ImGui::CollapsingHeader("Tonemapping", ImGuiTreeNodeFlags_DefaultOpen))
            {
                const char* tonemapModes[] = { "Reinhard", "ACES Filmic", "Uncharted 2" };
                int tonemapIndex = static_cast<int>(volume.Tonemap);
                if (ImGui::Combo("Tonemap Mode", &tonemapIndex, tonemapModes, IM_ARRAYSIZE(tonemapModes)))
                {
                    volume.Tonemap = static_cast<TonemapMode>(tonemapIndex);
                }
                ImGui::DragFloat("Exposure", &volume.Exposure, 0.01f, 0.0f, 10.0f);
            }

            // ---- Bloom ----
            if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Bloom Enabled", &volume.BloomEnabled);
                if (volume.BloomEnabled)
                {
                    ImGui::DragFloat("Threshold", &volume.BloomThreshold, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat("Bloom Intensity", &volume.BloomIntensity, 0.01f, 0.0f, 10.0f);
                    ImGui::DragInt("Iterations", &volume.BloomIterations, 1, 1, 10);
                }
            }

            // ---- FXAA ----
            if (ImGui::CollapsingHeader("FXAA", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("FXAA Enabled", &volume.FXAAEnabled);
            }

            // ---- Vignette ----
            if (ImGui::CollapsingHeader("Vignette", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Vignette Enabled", &volume.VignetteEnabled);
                if (volume.VignetteEnabled)
                {
                    ImGui::DragFloat("Vignette Intensity", &volume.VignetteIntensity, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Smoothness", &volume.VignetteSmoothness, 0.01f, 0.0f, 10.0f);
                }
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

        // MeshRenderer 组件
        DrawComponent<MeshRendererComponent>("Mesh Renderer", entity, [&entity](MeshRendererComponent& meshRenderer)
        {
            const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;
            
            const std::string& strID = std::format("Materials##{0}", entity.GetComponent<NameComponent>().Name);
            bool opened = ImGui::TreeNodeEx(strID.c_str(), flags);
            if (opened)
            {
                // 材质数量 TODO: 可编辑
                int materialSize = meshRenderer.Materials.size();
                ImGui::InputInt("Size", &materialSize, 0, 0, ImGuiInputTextFlags_ReadOnly);
                
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

        if (entity.HasComponent<MeshRendererComponent>())
        {
			// 绘制材质编辑器
            MeshRendererComponent& meshRenderer = entity.GetComponent<MeshRendererComponent>();
            for (Ref<Material>& material : meshRenderer.Materials)
            {
                DrawMaterialEditor(material);
            }
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
        
        // 所有用户可见 Shader 名称（排除引擎内部 Shader）
        std::vector<std::string> shaderNames = Renderer3D::GetShaderLibrary()->GetUserVisibleShaderNames();

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
            // ---- 渲染状态区域 ----
            if (ImGui::CollapsingHeader("Render State", ImGuiTreeNodeFlags_DefaultOpen))
            {
                RenderState& state = material->GetRenderState();
                
                // CullMode 下拉框
                const char* cullModes[] = { "Back", "Front", "Off (Two Sided)" };
                int currentCull = static_cast<int>(state.Cull);
                if (ImGui::Combo("Cull Mode", &currentCull, cullModes, IM_ARRAYSIZE(cullModes)))
                {
                    state.Cull = static_cast<CullMode>(currentCull);
                }
                
                // ZWrite 复选框
                ImGui::Checkbox("Depth Write", &state.DepthWrite);
                
                // ZTest 下拉框
                const char* depthFuncs[] = { "Less", "LessEqual", "Greater", "GreaterEqual", 
                                              "Equal", "NotEqual", "Always", "Never" };
                int currentDepth = static_cast<int>(state.DepthTest);
                if (ImGui::Combo("Depth Test", &currentDepth, depthFuncs, IM_ARRAYSIZE(depthFuncs)))
                {
                    state.DepthTest = static_cast<DepthCompareFunc>(currentDepth);
                }
                
                // BlendMode 下拉框
                const char* blendModes[] = { "None (Opaque)", "Alpha Blend", "Additive", "Soft Additive" };
                int currentBlend = static_cast<int>(state.Blend);
                if (ImGui::Combo("Blend Mode", &currentBlend, blendModes, IM_ARRAYSIZE(blendModes)))
                {
                    state.Blend = static_cast<BlendMode>(currentBlend);
                }
                
                // RenderQueue 拖拽条
                ImGui::DragInt("Render Queue", &state.Queue, 1, 0, 5000);
                
                // 快捷预设按钮
                if (ImGui::Button("Background"))
                {
                    state.Queue = RenderQueue::Background;
                }
                ImGui::SameLine();
                if (ImGui::Button("Geometry"))
                {
                    state.Queue = RenderQueue::Geometry;
                }
                ImGui::SameLine();
                if (ImGui::Button("AlphaTest"))
                {
                    state.Queue = RenderQueue::AlphaTest;
                }
                ImGui::SameLine();
                if (ImGui::Button("Transparent"))
                {
                    state.Queue = RenderQueue::Transparent;
                }
                ImGui::SameLine();
                if (ImGui::Button("Overlay"))
                {
                    state.Queue = RenderQueue::Overlay;
                }
            }
            
            ImGui::Separator();
            
            // 绘制材质属性（按 Shader uniform 声明顺序遍历）
            const auto& propertyMap = material->GetPropertyMap();
            for (const std::string& propName : material->GetPropertyOrder())
            {
                auto it = propertyMap.find(propName);
                if (it == propertyMap.end())
                {
                    continue;
                }

                const MaterialProperty& prop = it->second;  // 当前属性
                
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
                        texture = Renderer3D::GetDefaultTexture(TextureDefault::Black); // 使用黑色纹理表示贴图缺失
                    }
                        
                    uint32_t textureID = texture->GetRendererID();
                    std::string strID = std::format("##{0}{1}", prop.Name, textureID);
                        
                    ImGui::PushID(strID.c_str());
                        
                    if (ImGui::ImageButton((ImTextureID)textureID, { 64, 64 }, { 0, 1 }, { 1, 0 }))
                    {
                        std::string filepath = FileDialogs::OpenFile("Texture(*.png;*.jpg)\0*.png;*.jpg\0");
                        if (!filepath.empty())
                        {
                            material->SetTexture(prop.Name, Texture2D::Create(filepath));
                        }
                    }
                        
                    ImGui::SameLine();
                        
                    ImGui::Text(prop.Name.c_str());
                        
                    ImGui::PopID();
                        
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