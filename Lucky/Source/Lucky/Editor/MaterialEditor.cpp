#include "lcpch.h"
#include "MaterialEditor.h"

#include "Lucky/Core/UUID.h"
#include "Lucky/Scene/SelectionManager.h"
#include "Lucky/Renderer/Renderer3D.h"

#include "Lucky/UI/PropertyGrid.h"
#include "Lucky/UI/Widgets.h"
#include "Lucky/UI/DrawUtils.h"
#include "Lucky/UI/ScopedGuards.h"
#include "Lucky/UI/UICore.h"
#include "Lucky/UI/Theme.h"

#include "Lucky/Utils/PlatformUtils.h"

#include <imgui.h>

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
    
    void MaterialEditor::OnGUI(const Ref<Material>& material)
    {
        // 树节点标志：打开|框架|延伸到右边|允许重叠|框架边框
        const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;
        
        UUID id = SelectionManager::GetSelection();
        
        const std::string& materialName = material->GetName();
        const std::string& strMaterialID = std::format("{0}##{1}", materialName, static_cast<uint64_t>(id));
        
        bool opened = false;
        
        UI::Draw::HorizontalLine();
        
        UI::ShiftCursorY(1.0f);
        {
            UI::ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, { 0, 0 });   // 树节点和底部水平线之间的 Spacing
            opened = ImGui::TreeNodeEx(strMaterialID.c_str(), flags, "");
            
            // 材质名
            ImGui::SameLine();
            UI::ShiftCursorX(8.0f);
            {
                UI::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[0]);    // TODO 封装 Fonts
                const std::string& displayMaterialName = std::format("{0} (Material)", material->GetName());
                ImGui::Text(displayMaterialName.c_str());
            }

            ImGui::Indent(-UI::Theme::Layout::IndentSpacing);
            UI::Draw::HorizontalLine(0.6f);
            ImGui::Indent(UI::Theme::Layout::IndentSpacing);
        }
        
        if (opened)
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
