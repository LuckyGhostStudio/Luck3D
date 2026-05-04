#include "InspectorPanel.h"

#include "Lucky/Scene/SelectionManager.h"

#include "Lucky/Scene/Components/Components.h"

#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/RenderState.h"
#include "Lucky/Renderer/RenderContext.h"

#include "Lucky/Utils/PlatformUtils.h"

#include "Lucky/UI/Controls.h"
#include "Lucky/UI/PropertyGrid.h"
#include "Lucky/UI/Widgets.h"

#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

namespace Lucky
{
    /// <summary>
    /// 将 fieldName 名称转换为友好的显示名称
    /// </summary>
    static std::string GetDisplayName(const std::string& fieldName)
    {
        std::string name = fieldName;

        // 1. 移除常见前缀（m_, _, u_）
        if (name.size() > 2 && (name.substr(0, 2) == "m_" || name.substr(0, 2) == "u_"))
        {
            name = name.substr(2);
        }
        else if (name.size() > 1 && name[0] == '_')
        {
            name = name.substr(1);
        }

        // 2. 过滤字符串，仅保留字母、数字和下划线
        std::string filtered;
        for (char c : name)
        {
            if (std::isalnum(c) || c == '_')
            {
                filtered += c;
            }
        }
        name = filtered;

        // 3. 将下划线替换为空格
        std::replace(name.begin(), name.end(), '_', ' ');

        // 4. 在大写字母前插入空格（驼峰转空格分隔）
        std::string result;
        for (size_t i = 0; i < name.size(); ++i)
        {
            if (i > 0 && std::isupper(name[i]) && !std::isupper(name[i - 1]))
            {
                result += ' ';
            }
            result += name[i];
        }

        // 5. 应用标题大小写（每个单词首字母大写）
        bool capitalize = true;
        for (char& c : result)
        {
            if (std::isspace(c))
            {
                capitalize = true;
            }
            else if (capitalize)
            {
                c = std::toupper(c);
                capitalize = false;
            }
            // else
            // {
            //     c = std::tolower(c);
            // }
        }

        return result;
    }

    
    /// <summary>
    /// 判断属性名是否表示颜色（根据命名约定自动识别）TODO Remove
    /// </summary>
    static bool IsColorProperty(const std::string& name)
    {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        return lower.find("color") != std::string::npos
            || lower.find("colour") != std::string::npos
            || lower.find("tint") != std::string::npos
            || lower.find("albedo") != std::string::npos;
    }
    
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
        UUID id = entity.GetUUID();
        
        // Name 组件
        if (entity.HasComponent<NameComponent>())
        {
            std::string& name = entity.GetComponent<NameComponent>().Name;   // 物体名

            char buffer[256];                               // 输入框内容 buffer
            memset(buffer, 0, sizeof(buffer));              // 将 buffer 置零
            strcpy_s(buffer, sizeof(buffer), name.c_str()); // buffer = name
            
            UI::ShiftCursor(8.0f, 8.0f);
            if (UI::InputText("##Name", buffer, sizeof(buffer)))
            {
                name = std::string(buffer);
            }
            UI::ShiftCursorY(8.0f);
        }
        
        // Transform 组件
        DrawComponent<TransformComponent>("Transform", entity, [](TransformComponent& transform)
        {
            UI::BeginPropertyGrid();
    
            UI::PropertyFloat3("Position", transform.Translation, 0.01f);
            
            glm::vec3 rotationEuler = glm::degrees(transform.GetRotationEuler());
            if (UI::PropertyFloat3("Rotation", rotationEuler, 1.0f))
            {
                transform.SetRotationEuler(glm::radians(rotationEuler));
            }

            UI::PropertyFloat3("Scale", transform.Scale, 0.01f);
            
            UI::EndPropertyGrid();
        });
        
        // Light 组件
        DrawComponent<LightComponent>("Light", entity, [](LightComponent& light)
        {
            UI::BeginPropertyGrid();

            const char* lightTypes[] = { "Directional", "Point", "Spot" };
            int currentType = static_cast<int>(light.Type);
            if (UI::PropertyCombo("Type", currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
            {
                light.Type = static_cast<LightType>(currentType);
            }

            UI::PropertyColor("Color", light.Color);
            UI::PropertyFloat("Intensity", light.Intensity, 0.01f, 0.0f, 100.0f);
            
            // Point / Spot 属性
            if (light.Type == LightType::Point || light.Type == LightType::Spot)
            {
                UI::PropertyFloat("Range", light.Range, 0.1f, 0.1f, 1000.0f);
            }

            // Spot 属性
            if (light.Type == LightType::Spot)
            {
                UI::PropertyFloat("Inner Cutoff", light.InnerCutoffAngle, 0.5f, 0.0f, light.OuterCutoffAngle);
                UI::PropertyFloat("Outer Cutoff", light.OuterCutoffAngle, 0.5f, light.InnerCutoffAngle, 90.0f);
            }

            // 阴影属性
            const char* shadowTypes[] = { "No Shadows", "Hard Shadows", "Soft Shadows" };
            int currentShadow = static_cast<int>(light.Shadows);
            if (UI::PropertyCombo("Shadow Type", currentShadow, shadowTypes, IM_ARRAYSIZE(shadowTypes)))
            {
                light.Shadows = static_cast<ShadowType>(currentShadow);
            }

            if (light.Shadows != ShadowType::None)
            {
                UI::PropertyFloat("Shadow Bias", light.ShadowBias, 0.0001f, 0.0f, 0.05f);
                UI::PropertyFloat("Shadow Strength", light.ShadowStrength, 0.01f, 0.0f, 1.0f);
            }
            
            UI::EndPropertyGrid();
        });

        // MeshFilter 组件
        static std::string meshName;
        DrawComponent<MeshFilterComponent>(meshName + " (Mesh Filter)", entity, [](MeshFilterComponent& meshFilter)
        {
            meshName = meshFilter.Mesh->GetName();
            
            char buffer[256];                                   // 输入框内容 buffer
            memset(buffer, 0, sizeof(buffer));                  // 将 buffer 置零
            strcpy_s(buffer, sizeof(buffer), meshName.c_str()); // buffer = name
            
            UI::BeginPropertyGrid();
            
            UI::PropertyString("Mesh", buffer, sizeof(buffer));
            
            UI::EndPropertyGrid();
        });

        // MeshRenderer 组件
        DrawComponent<MeshRendererComponent>("Mesh Renderer", entity, [&](MeshRendererComponent& meshRenderer)
        {
            const std::string& strID = std::format("Materials##{0}", static_cast<uint64_t>(id));

            if (UI::BeginCollapsing(strID.c_str()))
            {
                UI::BeginPropertyGrid();
                
                // 材质数量 TODO: 可编辑
                int materialSize = static_cast<int>(meshRenderer.Materials.size());
                UI::PropertyInt("Size", materialSize);
                
                // 材质列表
                for (int i = 0; i < materialSize; i++)
                {
                    const std::string& label = std::format("Element {0}", i);
                    const std::string& materialName = meshRenderer.Materials[i]->GetName();
                
                    char buffer[256];                                   // 输入框内容 buffer
                    memset(buffer, 0, sizeof(buffer));                  // 将 buffer 置零
                    strcpy_s(buffer, sizeof(buffer), materialName.c_str()); // buffer = name
                
                    UI::PropertyString(label.c_str(), buffer, sizeof(buffer));
                }
                
                UI::EndPropertyGrid();
                
                UI::EndCollapsing();
            }
        });
        
        // PostProcessVolume 组件
        DrawComponent<PostProcessVolumeComponent>("Post Process Volume", entity, [&](PostProcessVolumeComponent& volume)
        {
            UI::BeginPropertyGrid();
            
            // Volume 设置
            UI::PropertyCheckbox("Is Global", volume.IsGlobal);
            UI::PropertyFloat("Priority", volume.Priority, 0.1f);
            
            UI::EndPropertyGrid();
            
            // ---- Tonemapping ----
            const std::string& strTonemappingID = std::format("Tonemapping##{0}", static_cast<uint64_t>(id));
            if (UI::BeginCollapsing(strTonemappingID.c_str()))
            {
                UI::BeginPropertyGrid();
                
                const char* tonemapModes[] = { "Reinhard", "ACES Filmic", "Uncharted 2" };
                int tonemapIndex = static_cast<int>(volume.Tonemap);
                if (UI::PropertyCombo("Tonemap Mode", tonemapIndex, tonemapModes, IM_ARRAYSIZE(tonemapModes)))
                {
                    volume.Tonemap = static_cast<TonemapMode>(tonemapIndex);
                }
                UI::PropertyFloat("Exposure", volume.Exposure, 0.01f, 0.0f, 10.0f);
                
                UI::EndPropertyGrid();
                
                UI::EndCollapsing();
            }

            // ---- Bloom ----
            const std::string& strBloomID = std::format("Bloom##{0}", static_cast<uint64_t>(id));
            if (UI::BeginCollapsing(strBloomID.c_str()))
            {
                UI::BeginPropertyGrid();
                
                UI::PropertyCheckbox("Bloom Enabled", volume.BloomEnabled);
                if (volume.BloomEnabled)
                {
                    UI::PropertyFloat("Threshold", volume.BloomThreshold, 0.01f, 0.0f, 10.0f);
                    UI::PropertyFloat("Bloom Intensity", volume.BloomIntensity, 0.01f, 0.0f, 10.0f);
                    UI::PropertyInt("Iterations", volume.BloomIterations, 1, 1, 10);
                }
                
                UI::EndPropertyGrid();
                
                UI::EndCollapsing();
            }

            // ---- FXAA ----
            const std::string& strFXAAID = std::format("FXAA##{0}", static_cast<uint64_t>(id));
            if (UI::BeginCollapsing(strFXAAID.c_str()))
            {
                UI::BeginPropertyGrid();
                
                UI::PropertyCheckbox("FXAA Enabled", volume.FXAAEnabled);
                
                UI::EndPropertyGrid();
                
                UI::EndCollapsing();
            }

            // ---- Vignette ----
            const std::string& strVignetteID = std::format("Vignette##{0}", static_cast<uint64_t>(id));
            if (UI::BeginCollapsing(strVignetteID.c_str()))
            {
                UI::BeginPropertyGrid();
                
                UI::PropertyCheckbox("Vignette Enabled", volume.VignetteEnabled);
                if (volume.VignetteEnabled)
                {
                    UI::PropertyFloat("Vignette Intensity", volume.VignetteIntensity, 0.01f, 0.0f, 1.0f);
                    UI::PropertyFloat("Smoothness", volume.VignetteSmoothness, 0.01f, 0.0f, 10.0f);
                }
                
                UI::EndPropertyGrid();
                
                UI::EndCollapsing();
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
        
        UI::Draw::HorizontalLine();
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
        
        UUID id = SelectionManager::GetSelection();
        
        // 树节点标志：打开|框架|延伸到右边|允许重叠|框架边框
        const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
        
        const std::string& strMaterialID = std::format("{0}##{1}", material->GetName(), static_cast<uint64_t>(id));
        
        if (ImGui::TreeNodeEx(strMaterialID.c_str(), flags))
        {
            // ---- Shader ----
        
            // 所有用户可见 Shader 名称（排除引擎内部 Shader）
            std::vector<std::string> shaderNames = Renderer3D::GetShaderLibrary()->GetUserVisibleShaderNames();

            // 创建 const char* 数组
            std::vector<const char*> shaderNamesArr;
            shaderNamesArr.reserve(shaderNames.size());  // 预留空间
            for (const std::string& name : shaderNames)
            {
                shaderNamesArr.push_back(name.c_str());
            }
            
            // 当前 Shader 名称
            std::string currentShaderName = material->GetShader()->GetName();

            // 查找当前 Shader 在列表中的索引
            int currentShaderIndex = -1;
            for (int i = 0; i < static_cast<int>(shaderNames.size()); i++)
            {
                if (shaderNames[i] == currentShaderName)
                {
                    currentShaderIndex = i;
                    break;
                }
            }
            
            UI::BeginPropertyGrid();
            
            // 绘制下拉选择框
            if (UI::PropertyCombo(currentShaderName.c_str(), currentShaderIndex, shaderNamesArr.data(), static_cast<int>(shaderNames.size())))
            {
                const Ref<Shader>& newShader = Renderer3D::GetShaderLibrary()->Get(shaderNames[currentShaderIndex]);
                material->SetShader(newShader);  // 设置 Shader 触发属性重建
            }
            
            UI::EndPropertyGrid();
            
            // ---- 渲染状态 ----
            const std::string& strRenderStateID = std::format("Render State##{0}{1}", static_cast<uint64_t>(id), material->GetName());
            if (UI::BeginCollapsing(strRenderStateID.c_str()))
            {
                UI::BeginPropertyGrid();
                
                RenderState& state = material->GetRenderState();
                
                // RenderingMode 下拉框（一键预设：自动配置 Blend / DepthWrite / Queue）
                const char* renderingModes[] = { "Opaque", "Cutout", "Fade", "Transparent" };
                int currentMode = static_cast<int>(material->GetRenderingMode());
                if (UI::PropertyCombo("Rendering Mode", currentMode, renderingModes, IM_ARRAYSIZE(renderingModes)))
                {
                    material->SetRenderingMode(static_cast<RenderingMode>(currentMode));
                }
                
                // CullMode 下拉框
                const char* cullModes[] = { "Back", "Front", "Off (Two Sided)" };
                int currentCull = static_cast<int>(state.Cull);
                if (UI::PropertyCombo("Cull Mode", currentCull, cullModes, IM_ARRAYSIZE(cullModes)))
                {
                    state.Cull = static_cast<CullMode>(currentCull);
                }
                
                // ZWrite 复选框
                UI::PropertyCheckbox("Depth Write", state.DepthWrite);
                
                // ZTest 下拉框
                const char* depthFuncs[] = { "Less", "LessEqual", "Greater", "GreaterEqual", "Equal", "NotEqual", "Always", "Never" };
                int currentDepth = static_cast<int>(state.DepthTest);
                if (UI::PropertyCombo("Depth Test", currentDepth, depthFuncs, IM_ARRAYSIZE(depthFuncs)))
                {
                    state.DepthTest = static_cast<DepthCompareFunc>(currentDepth);
                }
                
                // BlendMode 下拉框
                const char* blendModes[] = { "None (Opaque)", "Alpha Blend", "Additive", "Soft Additive" };
                int currentBlend = static_cast<int>(state.Blend);
                if (UI::PropertyCombo("Blend Mode", currentBlend, blendModes, IM_ARRAYSIZE(blendModes)))
                {
                    state.Blend = static_cast<BlendMode>(currentBlend);
                }
                
                UI::PropertyInt("Render Queue", state.Queue, 1, 0, 5000);
                
                UI::EndPropertyGrid();
                
                UI::EndCollapsing();
            }
            
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
                const std::string& displayName = GetDisplayName(prop.Name);  // 显示名称（友好格式）
                
                UI::BeginPropertyGrid();
                
                switch (prop.Type)
                {
                    case ShaderUniformType::Float:
                    {
                        float value = std::get<float>(prop.Value);
                        if (UI::PropertyFloat(displayName.c_str(), value, 0.1f))
                        {
                            material->SetFloat(prop.Name, value);
                        }
                        break;
                    }
                    case ShaderUniformType::Float2:
                    {
                        glm::vec2 value = std::get<glm::vec2>(prop.Value);
                        if (UI::PropertyFloat2(displayName.c_str(), value, 0.1f))
                        {
                            material->SetFloat2(prop.Name, value);
                        }
                        break;
                    }
                    case ShaderUniformType::Float3:
                    {
                        glm::vec3 value = std::get<glm::vec3>(prop.Value);
                        if (IsColorProperty(prop.Name))
                        {
                            if (UI::PropertyColor(displayName.c_str(), value))
                            {
                                material->SetFloat3(prop.Name, value);
                            }
                        }
                        else
                        {
                            if (UI::PropertyFloat3(displayName.c_str(), value, 0.1f))
                            {
                                material->SetFloat3(prop.Name, value);
                            }
                        }
                        break;
                    }
                    case ShaderUniformType::Float4:
                    {
                        glm::vec4 value = std::get<glm::vec4>(prop.Value);
                        if (IsColorProperty(prop.Name))
                        {
                            if (UI::PropertyColor4(displayName.c_str(), value))
                            {
                                material->SetFloat4(prop.Name, value);
                            }
                        }
                        else
                        {
                            if (UI::PropertyFloat4(displayName.c_str(), value, 0.1f))
                            {
                                material->SetFloat4(prop.Name, value);
                            }
                        }
                        break;
                    }
                    case ShaderUniformType::Int:
                    {
                        int value = std::get<int>(prop.Value);
                        if (UI::PropertyInt(displayName.c_str(), value))
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
                            
                        if (UI::PropertyTexture(displayName.c_str(), texture))
                        {
                            std::string filepath = FileDialogs::OpenFile("Texture(*.png;*.jpg)\0*.png;*.jpg\0");
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
                
                UI::EndPropertyGrid();
            }

            ImGui::TreePop();
        }
    }
}
