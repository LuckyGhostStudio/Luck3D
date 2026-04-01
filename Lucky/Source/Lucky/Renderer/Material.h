#pragma once

#include <variant>
#include <glm/glm.hpp>

#include "Shader.h"
#include "Texture.h"

namespace Lucky
{
    /// <summary>
    /// 材质属性值
    /// </summary>
    using MaterialPropertyValue = std::variant<
        float,              // Float
        glm::vec2,          // Float2
        glm::vec3,          // Float3
        glm::vec4,          // Float4
        int,                // Int / Sampler2D（纹理槽索引）
        glm::mat3,          // Mat3
        glm::mat4,          // Mat4
        Ref<Texture2D>      // Sampler2D（纹理对象引用）
    >;
    
    /// <summary>
    /// 材质属性：对应 Shader 中的一个 uniform 参数
    /// </summary>
    struct MaterialProperty
    {
        std::string Name;               // 属性名（与 Shader uniform 名一致）
        ShaderUniformType Type;         // 属性类型
        MaterialPropertyValue Value;    // 属性值
    };
    
    /// <summary>
    /// 材质：持有一个 Shader 和一组材质属性
    /// </summary>
    class Material
    {
    public:
        /// <summary>
        /// 创建材质
        /// </summary>
        /// <param name="shader">着色器</param>
        /// <returns>材质共享指针</returns>
        static Ref<Material> Create(const Ref<Shader>& shader);

        Material() = default;
        Material(const Ref<Shader>& shader);
        Material(const std::string& name, const Ref<Shader>& shader);

        /// <summary>
        /// 设置着色器：会触发属性列表重建
        /// </summary>
        /// <param name="shader">新的着色器</param>
        void SetShader(const Ref<Shader>& shader);
        
        Ref<Shader> GetShader() const { return m_Shader; }

        // ---- 设置材质属性值 ----

        void SetFloat(const std::string& name, float value);
        void SetFloat2(const std::string& name, const glm::vec2& value);
        void SetFloat3(const std::string& name, const glm::vec3& value);
        void SetFloat4(const std::string& name, const glm::vec4& value);
        void SetInt(const std::string& name, int value);
        void SetMat3(const std::string& name, const glm::mat3& value);
        void SetMat4(const std::string& name, const glm::mat4& value);
        void SetTexture(const std::string& name, const Ref<Texture2D>& texture);

        // ---- 获取材质属性值 ----

        float GetFloat(const std::string& name) const;
        glm::vec2 GetFloat2(const std::string& name) const;
        glm::vec3 GetFloat3(const std::string& name) const;
        glm::vec4 GetFloat4(const std::string& name) const;
        int GetInt(const std::string& name) const;
        glm::mat3 GetMat3(const std::string& name) const;
        glm::mat4 GetMat4(const std::string& name) const;
        Ref<Texture2D> GetTexture(const std::string& name) const;

        /// <summary>
        /// 按名获取属性指针（未找到返回 nullptr）
        /// </summary>
        MaterialProperty* GetProperty(const std::string& name);
        const MaterialProperty* GetProperty(const std::string& name) const;
        
        /// <summary>
        /// 应用材质：将所有属性值（用户可编辑的 uniform）上传到 GPU
        /// </summary>
        void Apply() const;
        
        /// <summary>
        /// 获取属性的有序名称列表（按 Shader uniform 声明顺序）
        /// </summary>
        const std::vector<std::string>& GetPropertyOrder() const { return m_PropertyOrder; }

        /// <summary>
        /// 获取属性 Map（按名索引）
        /// </summary>
        const std::unordered_map<std::string, MaterialProperty>& GetPropertyMap() const { return m_PropertyMap; }
        
        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }
    private:
        /// <summary>
        /// 根据当前 Shader 的 uniform 列表重建属性列表
        /// 切换 Shader 时调用，会尽量保留同名同类型的旧属性值
        /// </summary>
        void RebuildProperties();
        
        /// <summary>
        /// 查找指定名称的属性，返回指针（未找到返回 nullptr）
        /// </summary>
        MaterialProperty* FindProperty(const std::string& name);
        const MaterialProperty* FindProperty(const std::string& name) const;
    private:
        Ref<Shader> m_Shader;                                               // 着色器
        std::unordered_map<std::string, MaterialProperty> m_PropertyMap;    // 材质属性 Map：属性名 - 属性
        std::vector<std::string> m_PropertyOrder;                           // 属性名有序列表（按 Shader uniform 声明顺序）
        std::string m_Name;                                                 // 材质名称
    };
}
