#include "lcpch.h"
#include "Material.h"

#include "Renderer3D.h"

namespace Lucky
{
    /// <summary>
    /// 判断是否为内部 uniform（不暴露给用户编辑）
    /// </summary>
    static bool IsInternalUniform(const std::string& name)
    {
        // 引擎内部管理的 uniform 黑名单 TODO 持续更新
        static const std::unordered_set<std::string> s_InternalUniforms = {
            "u_ObjectToWorldMatrix",    // 模型变换矩阵（由引擎每帧设置）
            "u_EntityID",               // Entity ID（拾取系统，由引擎每帧设置）
        };

        return s_InternalUniforms.find(name) != s_InternalUniforms.end();
    }

    /// <summary>
    /// 根据类型返回材质属性默认值
    /// </summary>
    /// <param name="type">属性类型</param>
    /// <returns>默认值</returns>
    static MaterialPropertyValue GetMaterialPropertyDefaultValue(ShaderUniformType type)
    {
        switch (type)
        {
            case ShaderUniformType::Float:      return 0.0f;
            case ShaderUniformType::Float2:     return glm::vec2(0.0f);
            case ShaderUniformType::Float3:     return glm::vec3(0.0f);
            case ShaderUniformType::Float4:     return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            case ShaderUniformType::Int:        return 0;
            case ShaderUniformType::Mat3:       return glm::mat3(1.0f);
            case ShaderUniformType::Mat4:       return glm::mat4(1.0f);
            case ShaderUniformType::Sampler2D:  return Ref<Texture2D>(nullptr);
            default:                            return 0.0f;
        }
    }

    // ---- Material ----

    Ref<Material> Material::Create(const Ref<Shader>& shader)
    {
        return CreateRef<Material>(shader);
    }

    Material::Material(const Ref<Shader>& shader)
        : m_Shader(shader)
    {
        if (m_Shader)
        {
            m_Name = "New Material";
            RebuildProperties();
        }
    }

    Material::Material(const std::string& name, const Ref<Shader>& shader)
        : m_Shader(shader)
    {
        if (m_Shader)
        {
            m_Name = name;
            RebuildProperties();
        }
    }

    void Material::SetShader(const Ref<Shader>& shader)
    {
        m_Shader = shader;
        RebuildProperties();
    }

    // ---- 属性设置方法 ----

    void Material::SetFloat(const std::string& name, float value)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Float)
        {
            prop->Value = value;
        }
    }

    void Material::SetFloat2(const std::string& name, const glm::vec2& value)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Float2)
        {
            prop->Value = value;
        }
    }

    void Material::SetFloat3(const std::string& name, const glm::vec3& value)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Float3)
        {
            prop->Value = value;
        }
    }

    void Material::SetFloat4(const std::string& name, const glm::vec4& value)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Float4)
        {
            prop->Value = value;
        }
    }

    void Material::SetInt(const std::string& name, int value)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Int)
        {
            prop->Value = value;
        }
    }

    void Material::SetMat3(const std::string& name, const glm::mat3& value)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Mat3)
        {
            prop->Value = value;
        }
    }

    void Material::SetMat4(const std::string& name, const glm::mat4& value)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Mat4)
        {
            prop->Value = value;
        }
    }

    void Material::SetTexture(const std::string& name, const Ref<Texture2D>& texture)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Sampler2D)
        {
            prop->Value = texture;
        }
    }

    // ---- 属性获取方法 Begin ----

    float Material::GetFloat(const std::string& name) const
    {
        const MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Float)
        {
            return std::get<float>(prop->Value);
        }
        return 0.0f;
    }

    glm::vec2 Material::GetFloat2(const std::string& name) const
    {
        const MaterialProperty* prop= FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Float2)
        {
            return std::get<glm::vec2>(prop->Value);
        }
        return glm::vec2(0.0f); 
    }

    glm::vec3 Material::GetFloat3(const std::string& name) const
    {
        const MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Float3)
        {
            return std::get<glm::vec3>(prop->Value);
        }
        return glm::vec3(0.0f);
    }

    glm::vec4 Material::GetFloat4(const std::string& name) const
    {
        const MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Float4)
        {
            return std::get<glm::vec4>(prop->Value);
        }
        return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    int Material::GetInt(const std::string& name) const
    {
        const MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Int)
        {
            return std::get<int>(prop->Value);
        }
        return 0;
    }

    glm::mat3 Material::GetMat3(const std::string& name) const
    {
        const MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Mat3)
        {
            return std::get<glm::mat3>(prop->Value);
        }
        return glm::mat3(1.0f);
    }

    glm::mat4 Material::GetMat4(const std::string& name) const
    {
        const MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Mat4)
        {
            return std::get<glm::mat4>(prop->Value);
        }
        return glm::mat4(1.0f);
    }

    Ref<Texture2D> Material::GetTexture(const std::string& name) const
    {
        const MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderUniformType::Sampler2D)
        {
            return std::get<Ref<Texture2D>>(prop->Value);
        }
        return nullptr;
    }
    
    // ---- 获取属性方法 End ----

    MaterialProperty* Material::GetProperty(const std::string& name)
    {
        return FindProperty(name);
    }

    const MaterialProperty* Material::GetProperty(const std::string& name) const
    {
        return FindProperty(name);
    }
    
    void Material::Apply() const
    {
        if (!m_Shader)
        {
            return;
        }

        int textureSlot = 0;

        // 按声明顺序遍历属性，设置 Shader uniform
        for (const std::string& name : m_PropertyOrder)
        {
            auto it = m_PropertyMap.find(name);
            if (it == m_PropertyMap.end())
            {
                continue;
            }

            const MaterialProperty& prop = it->second;  // 当前属性
            
            switch (prop.Type)
            {
                case ShaderUniformType::Float:
                    m_Shader->SetFloat(prop.Name, std::get<float>(prop.Value));
                    break;
                case ShaderUniformType::Float2:
                    m_Shader->SetFloat2(prop.Name, std::get<glm::vec2>(prop.Value));
                    break;
                case ShaderUniformType::Float3:
                    m_Shader->SetFloat3(prop.Name, std::get<glm::vec3>(prop.Value));
                    break;
                case ShaderUniformType::Float4:
                    m_Shader->SetFloat4(prop.Name, std::get<glm::vec4>(prop.Value));
                    break;
                case ShaderUniformType::Int:
                    m_Shader->SetInt(prop.Name, std::get<int>(prop.Value));
                    break;
                case ShaderUniformType::Mat3:
                    m_Shader->SetMat3(prop.Name, std::get<glm::mat3>(prop.Value));
                    break;
                case ShaderUniformType::Mat4:
                    m_Shader->SetMat4(prop.Name, std::get<glm::mat4>(prop.Value));
                    break;
                case ShaderUniformType::Sampler2D:
                {
                    // 纹理数量超出上限
                    if (textureSlot >= 32)
                    {
                        LF_CORE_WARN("Texture slot count out of max 32! Texture '{0}' will be ignored", prop.Name);
                        break;
                    }

                    const Ref<Texture2D>& texture = std::get<Ref<Texture2D>>(prop.Value);

                    if (texture)
                    {
                        texture->Bind(textureSlot);
                    }
                    else
                    {
                        // 未设置纹理: 从 Shader 的 uniform 元数据获取默认纹理类型
                        TextureDefault defaultType = TextureDefault::White;  // 默认白色

                        // 查找 Shader 中该 uniform 的默认纹理类型
                        for (const auto& uniform : m_Shader->GetUniforms())
                        {
                            if (uniform.Name == prop.Name)
                            {
                                defaultType = uniform.DefaultTexture;
                                break;
                            }
                        }

                        Renderer3D::GetDefaultTexture(defaultType)->Bind(textureSlot);  // 绑定当前纹理的默认纹理
                    }

                    m_Shader->SetInt(prop.Name, textureSlot);  // 设置纹理槽位 index
                    textureSlot++;
                        
                    break;
                }
                default:
                    break;
            }
        }
    }

    void Material::RebuildProperties()
    {
        // 保存旧属性 Map
        std::unordered_map<std::string, MaterialProperty> oldPropertyMap = std::move(m_PropertyMap);

        m_PropertyMap.clear();
        m_PropertyOrder.clear();

        if (!m_Shader)
        {
            return;
        }
        
        const std::vector<ShaderUniform>& uniforms = m_Shader->GetUniforms();

        for (const ShaderUniform& uniform : uniforms)
        {
            // 跳过内部 uniform
            if (IsInternalUniform(uniform.Name))
            {
                continue;
            }

            // 构造材质属性
            MaterialProperty prop;
            prop.Name = uniform.Name;
            prop.Type = uniform.Type;
            prop.Value = GetMaterialPropertyDefaultValue(uniform.Type);

            // 尝试从旧属性中恢复同名同类型的值
            auto oldIt = oldPropertyMap.find(uniform.Name);
            if (oldIt != oldPropertyMap.end() && oldIt->second.Type == prop.Type)
            {
                prop.Value = oldIt->second.Value;
            }

            m_PropertyMap[uniform.Name] = std::move(prop);  // 添加到属性 Map
            m_PropertyOrder.push_back(uniform.Name);        // 记录声明顺序
        }

        LF_CORE_INFO("Material '{0}' properties rebuilt: {1} properties", m_Name, m_PropertyMap.size());
    }

    // ---- FindProperty ----

    MaterialProperty* Material::FindProperty(const std::string& name)
    {
        auto it = m_PropertyMap.find(name);
        if (it != m_PropertyMap.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    const MaterialProperty* Material::FindProperty(const std::string& name) const
    {
        auto it = m_PropertyMap.find(name);
        if (it != m_PropertyMap.end())
        {
            return &it->second;
        }
        return nullptr;
    }
}
