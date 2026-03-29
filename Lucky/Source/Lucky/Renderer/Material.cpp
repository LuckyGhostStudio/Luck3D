#include "lcpch.h"
#include "Material.h"

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
            "u_Textures[0]",            // 纹理数组（由引擎纹理槽系统管理）
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

    void Material::Apply() const
    {
        if (!m_Shader)
        {
            return;
        }

        int textureSlot = 1;  // 0 号槽位保留给引擎白色纹理 TODO 获取第一个可用槽位

        // 设置 Shader uniform
        for (const MaterialProperty& prop : m_Properties)
        {
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
                    const Ref<Texture2D>& texture = std::get<Ref<Texture2D>>(prop.Value);
                    if (texture)
                    {
                        texture->Bind(textureSlot);                 // 绑定纹理槽位
                        m_Shader->SetInt(prop.Name, textureSlot);   // 设置纹理槽位索引
                        textureSlot++;  // TODO 限制最大值
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    void Material::RebuildProperties()
    {
        std::vector<MaterialProperty> oldProperties = m_Properties;  // 保存旧属性
        m_Properties.clear();

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
            for (const MaterialProperty& oldProp : oldProperties)
            {
                if (oldProp.Name == prop.Name && oldProp.Type == prop.Type)
                {
                    prop.Value = oldProp.Value;
                    break;
                }
            }

            m_Properties.push_back(prop);   // 添加到属性列表
        }

        LF_CORE_INFO("Material '{0}' properties rebuilt: {1} properties", m_Name, m_Properties.size());
    }

    // ---- FindProperty ----

    MaterialProperty* Material::FindProperty(const std::string& name)
    {
        for (MaterialProperty& prop : m_Properties)
        {
            if (prop.Name == name)
            {
                return &prop;
            }
        }
        return nullptr;
    }

    const MaterialProperty* Material::FindProperty(const std::string& name) const
    {
        for (const MaterialProperty& prop : m_Properties)
        {
            if (prop.Name == name)
            {
                return &prop;
            }
        }
        return nullptr;
    }
}
