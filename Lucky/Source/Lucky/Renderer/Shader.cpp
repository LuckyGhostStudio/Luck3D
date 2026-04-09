#include "lcpch.h"
#include "Shader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <regex>

namespace Lucky
{
    /// <summary>
    /// 将 OpenGL uniform 类型转换为 ShaderUniformType
    /// </summary>
    /// <param name="type">OpenGL 类型枚举</param>
    /// <returns>对应的 ShaderUniformType</returns>
    static ShaderUniformType GLUniformTypeToShaderUniformType(GLenum type)
    {
        switch (type)
        {
            case GL_FLOAT:          return ShaderUniformType::Float;
            case GL_FLOAT_VEC2:     return ShaderUniformType::Float2;
            case GL_FLOAT_VEC3:     return ShaderUniformType::Float3;
            case GL_FLOAT_VEC4:     return ShaderUniformType::Float4;
            case GL_INT:            return ShaderUniformType::Int;
            case GL_FLOAT_MAT3:     return ShaderUniformType::Mat3;
            case GL_FLOAT_MAT4:     return ShaderUniformType::Mat4;
            case GL_SAMPLER_2D:     return ShaderUniformType::Sampler2D;
            default:
                LF_CORE_WARN("Unsupported GL uniform type: {0}", type);
                return ShaderUniformType::None;
        }
    }
    
    /// <summary>
    /// 将字符串转换为 TextureDefault 枚举
    /// </summary>
    /// <param name="str">字符串</param>
    /// <returns>TextureDefault 枚举</returns>
    static TextureDefault StringToTextureDefault(const std::string& str)
    {
        // 转小写比较
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower == "black")
        {
            return TextureDefault::Black;
        }
        if (lower == "normal")
        {
            return TextureDefault::Normal;
        }

        return TextureDefault::White;  // 默认白色
    }
    
    Ref<Shader> Shader::Create(const std::string& filepath)
    {
        return CreateRef<Shader>(filepath);
    }

    Shader::Shader(const std::string& filepath)
        : m_RendererID(0)
    {
        std::string vertexSrc = ReadFile(filepath + ".vert");   // 读取顶点着色器文件
        std::string fragmentSrc = ReadFile(filepath + ".frag"); // 读取片元着色器文件

        // 编译前解析 @default 注释元数据
        ParseTextureDefaults(vertexSrc);
        ParseTextureDefaults(fragmentSrc);
        
        std::unordered_map<GLenum, std::string> shaderSources;  // 着色器类型 - 源码 map

        shaderSources[GL_VERTEX_SHADER] = vertexSrc;            // 顶点着色器
        shaderSources[GL_FRAGMENT_SHADER] = fragmentSrc;        // 片元着色器

        // 计算着色器名
        auto lastSlash = filepath.find_last_of("/\\");                      // 最后一个 / 的索引
        lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;     // 最后一个 / 不存在 最后一个 / 存在
        m_Name = filepath.substr(lastSlash, filepath.size() - lastSlash);   // 着色器名称
        
        Compile(shaderSources); // 编译着色器源码
        
        ApplyTextureDefaults(); // 将解析到的默认纹理类型应用到 uniform 列表
    }

    Shader::~Shader()
    {
        glDeleteProgram(m_RendererID);    // 删除着色器程序
    }

    void Shader::Bind() const
    {
        glUseProgram(m_RendererID);     // 使用着色器程序
    }

    void Shader::UnBind() const
    {
        glUseProgram(0);
    }

    void Shader::SetInt(const std::string& name, int value)
    {
        UploadUniformInt(name, value);
    }

    void Shader::SetIntArray(const std::string& name, int* values, uint32_t count)
    {
        UploadUniformIntArray(name, values, count);
    }

    void Shader::SetFloat(const std::string& name, float value)
    {
        UploadUniformFloat(name, value);
    }

    void Shader::SetFloat2(const std::string& name, const glm::vec2& value)
    {
        UploadUniformFloat2(name, value);
    }

    void Shader::SetFloat3(const std::string& name, const glm::vec3& value)
    {
        UploadUniformFloat3(name, value);
    }

    void Shader::SetFloat4(const std::string& name, const glm::vec4& value)
    {
        UploadUniformFloat4(name, value);
    }

    void Shader::SetMat3(const std::string& name, const glm::mat3& value)
    {
        UploadUniformMat3(name, value);
    }

    void Shader::SetMat4(const std::string& name, const glm::mat4& value)
    {
        UploadUniformMat4(name, value);
    }

    // ---- 下列方法：上传 Uniform 变量到 Shader ----

    void Shader::UploadUniformInt(const std::string& name, int value)
    {
        int location = glGetUniformLocation(m_RendererID, name.c_str());    // 获取 Uniform 变量位置
        glUniform1i(location, value);                                       // 设置 Uniform 变量（位置，变量个数，是否转置，变量地址）
    }

    void Shader::UploadUniformIntArray(const std::string& name, int* values, uint32_t count)
    {
        int location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniform1iv(location, count, values);
    }

    void Shader::UploadUniformFloat(const std::string& name, float value)
    {
        int location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniform1f(location, value);
    }

    void Shader::UploadUniformFloat2(const std::string& name, const glm::vec2& value)
    {
        int location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniform2f(location, value.x, value.y);
    }

    void Shader::UploadUniformFloat3(const std::string& name, const glm::vec3& value)
    {
        int location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniform3f(location, value.x, value.y, value.z);
    }

    void Shader::UploadUniformFloat4(const std::string& name, const glm::vec4& value)
    {
        int location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniform4f(location, value.x, value.y, value.z, value.w);
    }

    void Shader::UploadUniformMat3(const std::string& name, const glm::mat3& matrix)
    {
        int location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }

    void Shader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix)
    {
        int location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }

    std::string Shader::ReadFile(const std::string& filepath)
    {
        std::string result;        // 文件内容

        std::ifstream in(filepath, std::ios::in | std::ios::binary);    // 输入流 二进制

        if (in)
        {
            in.seekg(0, std::ios::end);         // 文件指针移动到文件末尾
            result.resize(in.tellg());          // 重置 string 大小为文件大小
            in.seekg(0, std::ios::beg);         // 文件指针移动到文件开头
            in.read(&result[0], result.size()); // 读文件到 result 大小 size

            in.close();                         // 关闭文件输入流
        }
        else
        {
            LF_CORE_ERROR("Could not open file '{0}'", filepath);    // 无法打开文件
        }

        return result;
    }

    void Shader::Compile(std::unordered_map<GLenum, std::string>& shaderSources)
    {
        LF_CORE_ASSERT(shaderSources.size() <= 2, "Only support 2 shaders now!");

        unsigned int program = glCreateProgram();   // 创建程序;

        std::array<GLenum, 2> glShaderIDs;          // 着色器 ID 列表

        int shaderIDIndex = 0;

        // 遍历所有类型着色器源码
        for (auto& kv : shaderSources)
        {
            GLenum type = kv.first;                         // 着色器类型
            const std::string& source = kv.second;          // 着色器源码

            unsigned int shader = glCreateShader(type);     // 创建 type 类型着色器 返回 id

            const char* sourceCStr = source.c_str();
            glShaderSource(shader, 1, &sourceCStr, 0);      // 着色器源代码发送到GL

            glCompileShader(shader);                        // 编译着色器

            int isCompiled = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);              // 获取编译状态

            if (isCompiled == GL_FALSE)     // 编译失败
            {
                int maxLength = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);          // 获取编译日志信息长度

                std::vector<char> infoLog(maxLength);
                glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]); // 获取编译日志信息

                glDeleteShader(shader);     // 删除着色器

                LF_CORE_ERROR("{0}", infoLog.data());                           // 失败信息
                LF_CORE_ASSERT(false, "Shader Compilation Failure!");           // 着色器编译失败

                break;
            }

            glAttachShader(program, shader);        // 将着色器添加到程序
            glShaderIDs[shaderIDIndex] = shader;    // 着色器 ID 添加到列表

            shaderIDIndex++;
        }

        glLinkProgram(program);     // 链接程序

        int isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);   // 获取链接状态

        if (isLinked == GL_FALSE)   // 链接失败
        {
            int maxLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<char> infoLog(maxLength);
            glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

            glDeleteProgram(program);

            // 删除着色器
            for (auto id : glShaderIDs) {
                glDeleteShader(id);
            }

            LF_CORE_ERROR("{0}", infoLog.data());           // 失败信息
            LF_CORE_ASSERT(false, "Shader Link Failure!");  // 着色器链接失败

            return;
        }

        // 分离着色器
        for (auto id : glShaderIDs)
        {
            glDetachShader(program, id);
        }

        m_RendererID = program;
        
        Introspect();  // 内省：获取所有 active uniform 信息
    }
    
    void Shader::Introspect()
    {
        m_Uniforms.clear();

        int uniformCount = 0;
        glGetProgramiv(m_RendererID, GL_ACTIVE_UNIFORMS, &uniformCount);    // 获取 active uniform 变量数量

        for (int i = 0; i < uniformCount; i++)
        {
            char name[256];
            int length = 0;
            int size = 0;
            GLenum type = 0;

            // 获取第 i 个 active uniform 信息（名称大小，实际名称长度，uniform 数组大小，uniform 类型，名称）
            glGetActiveUniform(m_RendererID, (GLuint)i, sizeof(name), &length, &size, &type, name);

            int location = glGetUniformLocation(m_RendererID, name);

            // location == -1 表示该 uniform 属于 Uniform Block（UBO），跳过
            if (location == -1)
            {
                continue;
            }

            ShaderUniformType dataType = GLUniformTypeToShaderUniformType(type);
            if (dataType == ShaderUniformType::None)
            {
                continue;  // 不支持的类型，跳过
            }

            ShaderUniform uniform;
            uniform.Name = std::string(name);
            uniform.Type = dataType;
            uniform.Location = location;
            uniform.Size = size;

            m_Uniforms.push_back(uniform);  // 添加 uniform 到列表

            LF_CORE_TRACE("  Uniform #{0}: name = '{1}', type = {2}, location = {3}, size = {4}", i, uniform.Name, (int)uniform.Type, uniform.Location, uniform.Size);
        }

        LF_CORE_INFO("Shader '{0}' introspection complete: {1} active uniforms", m_Name, m_Uniforms.size());
    }

    void Shader::ParseTextureDefaults(const std::string& source)
    {
        // 正则匹配：// @default: <type> 后紧跟 uniform sampler2D <name>;
        // 支持中间有空行或其他注释
        static const std::regex pattern(
            R"(//\s*@default\s*:\s*(\w+)\s*\n\s*uniform\s+sampler2D\s+(\w+)\s*;)",
            std::regex::ECMAScript
        );

        auto begin = std::sregex_iterator(source.begin(), source.end(), pattern);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it)
        {
            const std::smatch& match = *it;
            std::string type = match[1].str();  // white / black / normal
            std::string name = match[2].str();  // uniform 名

            m_TextureDefaultMap[name] = StringToTextureDefault(type);

            LF_CORE_TRACE("  Texture default: '{0}' -> {1}", name, type);
        }
    }

    void Shader::ApplyTextureDefaults()
    {
        for (ShaderUniform& uniform : m_Uniforms)
        {
            if (uniform.Type != ShaderUniformType::Sampler2D)
            {
                continue;
            }

            auto it = m_TextureDefaultMap.find(uniform.Name);
            if (it != m_TextureDefaultMap.end())
            {
                uniform.DefaultTexture = it->second;
            }
            // 没有 @default 注释的 sampler2D 使用默认值 TextureDefault::White
        }
    }

    // ---- ShaderLibrary ----

    void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
    {
        LF_CORE_ASSERT(!Exists(name), "Shader already exist!"); // 着色器已存在

        m_Shaders[name] = shader;   // 添加着色器到map
    }

    void ShaderLibrary::Add(const Ref<Shader>& shader)
    {
        auto& name = shader->GetName();
        Add(name, shader);
    }

    Ref<Shader> ShaderLibrary::Load(const std::string& filepath)
    {
        auto shader = Shader::Create(filepath);   // 创建着色器
        Add(shader);                                        // 添加着色器

        return shader;
    }

    Ref<Shader> ShaderLibrary::Load(const std::string& name, const std::string& filepath)
    {
        auto shader = Shader::Create(filepath);
        Add(name, shader);

        return shader;
    }

    Ref<Shader> ShaderLibrary::Get(const std::string& name)
    {
        LF_CORE_ASSERT(Exists(name), "Shader not found!");  // 着色器找不到

        return m_Shaders[name];
    }

    bool ShaderLibrary::Exists(const std::string& name) const
    {
        return m_Shaders.find(name) != m_Shaders.end();
    }

    std::vector<std::string> ShaderLibrary::GetShaderNameList() const
    {
        std::vector<std::string> names;
        names.reserve(m_Shaders.size());
        
        for (const auto& [name, shader] : m_Shaders)
        {
            names.push_back(name);
        }
        
        return names;
    }
}
