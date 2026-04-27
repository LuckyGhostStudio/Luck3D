#include "lcpch.h"
#include "MaterialSerializer.h"

#include "Lucky/Renderer/Shader.h"
#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/RenderState.h"

#include "YamlHelpers.h"

#include <filesystem>
#include <yaml-cpp/yaml.h>

namespace Lucky
{
    /// <summary>
    /// ShaderUniformType 转字符串（用于序列化）
    /// </summary>
    /// <param name="type">Shader Uniform 类型</param>
    /// <returns>类型字符串</returns>
    static std::string ShaderUniformTypeToString(ShaderUniformType type)
    {
        switch (type)
        {
        case ShaderUniformType::Float:      return "Float";
        case ShaderUniformType::Float2:     return "Float2";
        case ShaderUniformType::Float3:     return "Float3";
        case ShaderUniformType::Float4:     return "Float4";
        case ShaderUniformType::Int:        return "Int";
        case ShaderUniformType::Mat3:       return "Mat3";
        case ShaderUniformType::Mat4:       return "Mat4";
        case ShaderUniformType::Sampler2D:  return "Sampler2D";
        default:                            return "None";
        }
    }
    
    /// <summary>
    /// 字符串转 ShaderUniformType（用于反序列化）
    /// </summary>
    /// <param name="str">类型字符串</param>
    /// <returns>Shader Uniform 类型</returns>
    static ShaderUniformType StringToShaderUniformType(const std::string& str)
    {
        if (str == "Float")     return ShaderUniformType::Float;
        if (str == "Float2")    return ShaderUniformType::Float2;
        if (str == "Float3")    return ShaderUniformType::Float3;
        if (str == "Float4")    return ShaderUniformType::Float4;
        if (str == "Int")       return ShaderUniformType::Int;
        if (str == "Mat3")      return ShaderUniformType::Mat3;
        if (str == "Mat4")      return ShaderUniformType::Mat4;
        if (str == "Sampler2D") return ShaderUniformType::Sampler2D;
    
        return ShaderUniformType::None;
    }
    
    /// <summary>
    /// 序列化单个材质属性值到 YAML
    /// </summary>
    /// <param name="out">YAML 发射器</param>
    /// <param name="prop">材质属性</param>
    static void SerializeMaterialPropertyValue(YAML::Emitter& out, const MaterialProperty& prop)
    {
        switch (prop.Type)
        {
            case ShaderUniformType::Float:
                out << YAML::Key << "Value" << YAML::Value << std::get<float>(prop.Value);
                break;
            case ShaderUniformType::Float2:
                out << YAML::Key << "Value" << YAML::Value << std::get<glm::vec2>(prop.Value);
                break;
            case ShaderUniformType::Float3:
                out << YAML::Key << "Value" << YAML::Value << std::get<glm::vec3>(prop.Value);
                break;
            case ShaderUniformType::Float4:
                out << YAML::Key << "Value" << YAML::Value << std::get<glm::vec4>(prop.Value);
                break;
            case ShaderUniformType::Int:
                out << YAML::Key << "Value" << YAML::Value << std::get<int>(prop.Value);
                break;
            case ShaderUniformType::Mat3:
            {
                // Mat3 序列化为 9 元素数组（列主序）
                const glm::mat3& mat = std::get<glm::mat3>(prop.Value);
                out << YAML::Key << "Value" << YAML::Value;
                out << YAML::Flow << YAML::BeginSeq;
                for (int col = 0; col < 3; col++)
                {
                    for (int row = 0; row < 3; row++)
                    {
                        out << mat[col][row];
                    }
                }
                out << YAML::EndSeq;
                break;
            }
            case ShaderUniformType::Mat4:
            {
                // Mat4 序列化为 16 元素数组（列主序）
                const glm::mat4& mat = std::get<glm::mat4>(prop.Value);
                out << YAML::Key << "Value" << YAML::Value;
                out << YAML::Flow << YAML::BeginSeq;
                for (int col = 0; col < 4; col++)
                {
                    for (int row = 0; row < 4; row++)
                    {
                        out << mat[col][row];
                    }
                }
                out << YAML::EndSeq;
                break;
            }
            case ShaderUniformType::Sampler2D:
            {
                // 纹理序列化为文件路径字符串，空纹理序列化为空字符串
                const Ref<Texture2D>& texture = std::get<Ref<Texture2D>>(prop.Value);
                std::string texturePath = "";
                if (texture && !texture->GetPath().empty())
                {
                    // 绝对路径转换为相对于工作目录的相对路径
                    std::filesystem::path absPath(texture->GetPath());
                    std::filesystem::path relPath = std::filesystem::relative(absPath);

                    texturePath = relPath.generic_string();  // 使用正斜杠
                }
                out << YAML::Key << "Value" << YAML::Value << texturePath;
                break;
            }
            default:
                break;
        }
    }
    
    /// <summary>
    /// 从 YAML 节点反序列化材质属性值，并设置到材质上
    /// </summary>
    /// <param name="material">目标材质</param>
    /// <param name="propName">属性名</param>
    /// <param name="type">属性类型</param>
    /// <param name="valueNode">YAML Value 节点</param>
    static void DeserializeMaterialPropertyValue(const Ref<Material>& material, const std::string& propName, ShaderUniformType type, const YAML::Node& valueNode)
    {
        if (!valueNode)
        {
            return;
        }

        switch (type)
        {
            case ShaderUniformType::Float:
                material->SetFloat(propName, valueNode.as<float>());
                break;
            case ShaderUniformType::Float2:
                material->SetFloat2(propName, valueNode.as<glm::vec2>());
                break;
            case ShaderUniformType::Float3:
                material->SetFloat3(propName, valueNode.as<glm::vec3>());
                break;
            case ShaderUniformType::Float4:
                material->SetFloat4(propName, valueNode.as<glm::vec4>());
                break;
            case ShaderUniformType::Int:
                material->SetInt(propName, valueNode.as<int>());
                break;
            case ShaderUniformType::Mat3:
            {
                // 从 9 元素数组反序列化（列主序）
                if (valueNode.IsSequence() && valueNode.size() == 9)
                {
                    glm::mat3 mat;
                    int i = 0;
                    for (int col = 0; col < 3; col++)
                    {
                        for (int row = 0; row < 3; row++)
                        {
                            mat[col][row] = valueNode[i++].as<float>();
                        }
                    }
                    material->SetMat3(propName, mat);
                }
                break;
            }
            case ShaderUniformType::Mat4:
            {
                // 从 16 元素数组反序列化（列主序）
                if (valueNode.IsSequence() && valueNode.size() == 16)
                {
                    glm::mat4 mat;
                    int i = 0;
                    for (int col = 0; col < 4; col++)
                    {
                        for (int row = 0; row < 4; row++)
                        {
                            mat[col][row] = valueNode[i++].as<float>();
                        }
                    }
                    material->SetMat4(propName, mat);
                }
                break;
            }
            case ShaderUniformType::Sampler2D:
            {
                // 从文件路径字符串反序列化纹理
                std::string texturePath = valueNode.as<std::string>();
                if (!texturePath.empty())
                {
                    std::filesystem::path path(texturePath);

                    // 基于工作目录解析
                    std::string absolutePath = std::filesystem::absolute(path).string();

                    Ref<Texture2D> texture = Texture2D::Create(absolutePath);
                    material->SetTexture(propName, texture);
                }
                break;
            }
            default:
                break;
        }
    }
    
    void MaterialSerializer::Serialize(YAML::Emitter& out, const Ref<Material>& material)
    {
        out << YAML::BeginMap;  // 材质 Map 开始

        // 材质名称
        out << YAML::Key << "Name" << YAML::Value << material->GetName();

        // Shader 名称（通过 ShaderLibrary 中的名称引用）
        std::string shaderName = "";
        if (material->GetShader())
        {
            shaderName = material->GetShader()->GetName();
        }
        out << YAML::Key << "Shader" << YAML::Value << shaderName;

        // ---- 序列化渲染状态 ----
        const RenderState& state = material->GetRenderState();
        out << YAML::Key << "RenderState" << YAML::BeginMap;
        out << YAML::Key << "RenderingMode" << YAML::Value << static_cast<int>(material->GetRenderingMode());
        out << YAML::Key << "CullMode" << YAML::Value << static_cast<int>(state.Cull);
        out << YAML::Key << "DepthWrite" << YAML::Value << state.DepthWrite;
        out << YAML::Key << "DepthTest" << YAML::Value << static_cast<int>(state.DepthTest);
        out << YAML::Key << "BlendMode" << YAML::Value << static_cast<int>(state.Blend);
        out << YAML::Key << "RenderQueue" << YAML::Value << state.Queue;
        out << YAML::EndMap;

        // 材质属性列表
        out << YAML::Key << "Properties" << YAML::Value << YAML::BeginSeq;

        // 按声明顺序遍历属性
        const auto& propertyOrder = material->GetPropertyOrder();
        const auto& propertyMap = material->GetPropertyMap();

        for (const std::string& propName : propertyOrder)
        {
            auto it = propertyMap.find(propName);
            if (it == propertyMap.end())
            {
                continue;
            }

            const MaterialProperty& prop = it->second;

            out << YAML::BeginMap;  // 属性 Map 开始

            out << YAML::Key << "Name" << YAML::Value << prop.Name;
            out << YAML::Key << "Type" << YAML::Value << ShaderUniformTypeToString(prop.Type);
            
            SerializeMaterialPropertyValue(out, prop);

            out << YAML::EndMap;    // 属性 Map 结束
        }

        out << YAML::EndSeq;    // 属性列表结束

        out << YAML::EndMap;    // 材质 Map 结束
    }

    Ref<Material> MaterialSerializer::Deserialize(const YAML::Node& materialNode)
    {
        if (!materialNode)
        {
            return nullptr;
        }

        // 读取 Shader 名称
        std::string shaderName = "";
        if (materialNode["Shader"])
        {
            shaderName = materialNode["Shader"].as<std::string>();
        }

        // Shader 名称为空，返回空材质
        if (shaderName.empty())
        {
            return nullptr;
        }

        // 从 ShaderLibrary 获取 Shader
        Ref<ShaderLibrary>& shaderLib = Renderer3D::GetShaderLibrary();
        if (!shaderLib->Exists(shaderName))
        {
            LF_CORE_WARN("Deserialize Material: Shader '{0}' not found in ShaderLibrary, using internal error material.", shaderName);
            return Renderer3D::GetInternalErrorMaterial();  // 材质丢失 使用错误材质
        }

        Ref<Shader> shader = shaderLib->Get(shaderName);

        // 创建材质
        std::string materialName = "New Material";
        if (materialNode["Name"])
        {
            materialName = materialNode["Name"].as<std::string>();
        }

        Ref<Material> material = CreateRef<Material>(materialName, shader);

        // ---- 反序列化渲染状态 ----
        YAML::Node renderStateNode = materialNode["RenderState"];
        if (renderStateNode)
        {
            if (renderStateNode["RenderingMode"])
            {
                material->SetRenderingMode(static_cast<RenderingMode>(renderStateNode["RenderingMode"].as<int>()));
            }
            
            RenderState& state = material->GetRenderState();
            
            if (renderStateNode["CullMode"])
            {
                state.Cull = static_cast<CullMode>(renderStateNode["CullMode"].as<int>());
            }
            if (renderStateNode["DepthWrite"])
            {
                state.DepthWrite = renderStateNode["DepthWrite"].as<bool>();
            }
            if (renderStateNode["DepthTest"])
            {
                state.DepthTest = static_cast<DepthCompareFunc>(renderStateNode["DepthTest"].as<int>());
            }
            if (renderStateNode["BlendMode"])
            {
                state.Blend = static_cast<BlendMode>(renderStateNode["BlendMode"].as<int>());
            }
            if (renderStateNode["RenderQueue"])
            {
                state.Queue = renderStateNode["RenderQueue"].as<int>();
            }
        }

        // 反序列化属性列表
        YAML::Node propertiesNode = materialNode["Properties"];
        if (propertiesNode && propertiesNode.IsSequence())
        {
            for (auto propNode : propertiesNode)
            {
                // 读取属性名
                if (!propNode["Name"])
                {
                    continue;
                }
                std::string propName = propNode["Name"].as<std::string>();

                // 读取属性类型
                if (!propNode["Type"])
                {
                    continue;
                }
                ShaderUniformType propType = StringToShaderUniformType(propNode["Type"].as<std::string>());

                if (propType == ShaderUniformType::None)
                {
                    continue;
                }

                // 反序列化属性值
                DeserializeMaterialPropertyValue(material, propName, propType, propNode["Value"]);
            }
        }

        return material;
    }
}
