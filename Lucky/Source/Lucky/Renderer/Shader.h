#pragma once

#include <string>

#include <glm/glm.hpp>

#include "Buffer.h"

namespace Lucky
{
    /// <summary>
    /// 纹理默认类型
    /// </summary>
    enum class TextureDefault : uint8_t
    {
        White,      // 白色 (255, 255, 255, 255) 乘法中性值，大多数纹理的默认值
        Black,      // 黑色 (0, 0, 0, 255)       加法中性值
        Normal,     // 法线 (128, 128, 255, 255) 平坦法线，解码后为 (0, 0, 1)
    };
    
    /// <summary>
    /// Shader Uniform 类型
    /// </summary>
    enum class ShaderUniformType
    {
        None = 0,
        Float, Float2, Float3, Float4,
        Int,
        Mat3, Mat4,
        Sampler2D
    };
    
    /// <summary>
    /// Shader Uniform 参数信息
    /// </summary>
    struct ShaderUniform
    {
        std::string Name;           // uniform 变量名
        ShaderUniformType Type;     // 数据类型
        int Location;               // uniform location（glGetUniformLocation 返回值）
        int Size;                   // 数组大小（非数组为 1）
        TextureDefault DefaultTexture = TextureDefault::White;  // 默认纹理类型（仅 Sampler2D 使用）
    };
    
    /// <summary>
    /// 着色器
    /// </summary>
    class Shader
    {
    public:
        /// <summary>
        /// 创建着色器
        /// </summary>
        /// <param name="filepath">着色器文件路径(不含后缀名)</param>
        /// <returns></returns>
        static Ref<Shader> Create(const std::string& filepath);

        Shader(const std::string& filepath);
        
        ~Shader();

        /// <summary>
        /// 绑定：使用着色器
        /// </summary>
        void Bind() const;

        /// <summary>
        /// 解除绑定：调试时使用
        /// </summary>
        void UnBind() const;

        /// <summary>
        /// 设置 uniform Int 变量
        /// </summary>
        /// <param name="name">变量名</param>
        /// <param name="value">变量值</param>
        void SetInt(const std::string& name, int value);

        /// <summary>
        /// 设置 uniform Int Array 变量
        /// </summary>
        /// <param name="name">变量名</param>
        /// <param name="values">变量值</param>
        /// <param name="count">Array 元素数量</param>
        void SetIntArray(const std::string& name, int* values, uint32_t count);

        /// <summary>
        /// 设置 uniform Float 变量
        /// </summary>
        /// <param name="name">变量名</param>
        /// <param name="value">变量值</param>
        void SetFloat(const std::string& name, float value);

        /// <summary>
        /// 设置 uniform Float2 变量
        /// </summary>
        /// <param name="name">变量名</param>
        /// <param name="value">变量值</param>
        void SetFloat2(const std::string& name, const glm::vec2& value);

        /// <summary>
        /// 设置 uniform Float3 变量
        /// </summary>
        /// <param name="name">变量名</param>
        /// <param name="value">变量值</param>
        void SetFloat3(const std::string& name, const glm::vec3& value);

        /// <summary>
        /// 设置 uniform Float4 变量
        /// </summary>
        /// <param name="name">变量名</param>
        /// <param name="value">变量值</param>
        void SetFloat4(const std::string& name, const glm::vec4& value);

        /// <summary>
        /// 设置 uniform Matrix3 变量
        /// </summary>
        /// <param name="name">变量名</param>
        /// <param name="value">变量值</param>
        void SetMat3(const std::string& name, const glm::mat3& value);
        
        /// <summary>
        /// 设置 uniform Matrix4 变量
        /// </summary>
        /// <param name="name">变量名</param>
        /// <param name="value">变量值</param>
        void SetMat4(const std::string& name, const glm::mat4& value);

        uint32_t GetRendererID() const { return m_RendererID; }
        const std::string GetName() const { return m_Name; }
        const std::vector<ShaderUniform>& GetUniforms() const { return m_Uniforms; }

        // ---- 下列方法：上传 Uniform 变量到 Shader ---- |（变量在 Shader 中的变量名，变量值）

        void UploadUniformInt(const std::string& name, int value);
        void UploadUniformIntArray(const std::string& name, int* values, uint32_t count);

        void UploadUniformFloat(const std::string& name, float value);
        void UploadUniformFloat2(const std::string& name, const glm::vec2& value);
        void UploadUniformFloat3(const std::string& name, const glm::vec3& value);
        void UploadUniformFloat4(const std::string& name, const glm::vec4& value);

        void UploadUniformMat3(const std::string& name, const glm::mat3& matrix);
        void UploadUniformMat4(const std::string& name, const glm::mat4& matrix);
    private:
        /// <summary>
        /// 读文件
        /// </summary>
        /// <param name="filepath">文件路径</param>
        /// <returns>文件内容</returns>
        std::string ReadFile(const std::string& filepath);

        /// <summary>
        /// 编译着色器
        /// </summary>
        /// <param name="shaderSources">着色器类型 - 着色器源码 map</param>
        void Compile(std::unordered_map<unsigned int, std::string>& shaderSources);
        
        /// <summary>
        /// 内省：查询 Shader 程序中所有 active uniform 参数
        /// 在 Compile() 成功后调用
        /// </summary>
        void Introspect();
        
        /// <summary>
        /// 解析 Shader 源码中的 @default 注释元数据
        /// 提取 sampler2D uniform 的默认纹理类型
        /// </summary>
        /// <param name="source">Shader 源码</param>
        void ParseTextureDefaults(const std::string& source);

        /// <summary>
        /// 将解析到的默认纹理类型应用到 m_Uniforms 列表
        /// </summary>
        void ApplyTextureDefaults();
    private:
        uint32_t m_RendererID;                  // 着色器 ID
        std::string m_Name;                     // 着色器名字
        std::vector<ShaderUniform> m_Uniforms;  // uniform 参数列表
        
        std::unordered_map<std::string, TextureDefault> m_TextureDefaultMap;    // 解析缓存：uniform 名 -> 默认纹理类型
    };

    /// <summary>
    /// 着色器库
    /// </summary>
    class ShaderLibrary
    {
    public:
        /// <summary>
        /// 添加着色器
        /// </summary>
        /// <param name="name">着色器名</param>
        /// <param name="shader">着色器</param>
        void Add(const std::string& name, const Ref<Shader>& shader);

        /// <summary>
        /// 添加着色器
        /// </summary>
        /// <param name="shader">着色器</param>
        void Add(const Ref<Shader>& shader);

        /// <summary>
        /// 加载着色器
        /// </summary>
        /// <param name="filepath">文件路径</param>
        /// <returns>着色器</returns>
        Ref<Shader> Load(const std::string& filepath);

        /// <summary>
        /// 加载着色器
        /// </summary>
        /// <param name="name">着色器名称</param>
        /// <param name="filepath">文件路径</param>
        /// <returns>着色器</returns>
        Ref<Shader> Load(const std::string& name, const std::string& filepath);

        /// <summary>
        /// 返回着色器
        /// </summary>
        /// <param name="name">着色器名称</param>
        /// <returns>着色器</returns>
        Ref<Shader> Get(const std::string& name);

        /// <summary>
        /// 着色器是否存在
        /// </summary>
        /// <param name="name">着色器名</param>
        /// <returns>是否存在</returns>
        bool Exists(const std::string& name) const;
        
        /// <summary>
        /// 获取所有着色器名称列表
        /// </summary>
        std::vector<std::string> GetShaderNameList() const;

        /// <summary>
        /// 获取所有着色器
        /// </summary>
        const std::unordered_map<std::string, Ref<Shader>>& GetShaders() const { return m_Shaders; }
    private:
        std::unordered_map<std::string, Ref<Shader>> m_Shaders;        // 着色器 map：着色器名 - 着色器
    };
}