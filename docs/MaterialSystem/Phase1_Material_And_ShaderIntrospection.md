# Phase 1：Material 类 + Shader 内省 + MeshRendererComponent

## 1. 概述

Phase 1 是材质系统的基础层，目标是：
1. 为 `Shader` 类添加 Uniform 内省（Introspection）能力，能在运行时获取 Shader 的所有 active uniform 参数信息
2. 创建 `Material` 类，持有一个 Shader 引用和一组材质属性（对应 Shader 的 uniform 参数）
3. 创建 `MeshRendererComponent` 组件，持有材质列表

## 2. 涉及的文件

### 需要新建的文件
| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Renderer/Material.h` | Material 类头文件 |
| `Lucky/Source/Lucky/Renderer/Material.cpp` | Material 类实现 |
| `Lucky/Source/Lucky/Scene/Components/MeshRendererComponent.h` | MeshRenderer 组件 |

### 需要修改的文件
| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Renderer/Shader.h` | 添加 Uniform 内省相关结构和方法 |
| `Lucky/Source/Lucky/Renderer/Shader.cpp` | 实现 Uniform 内省逻辑 |

---

## 3. Shader 内省（Shader Introspection）

### 3.1 目标

在 Shader 编译链接成功后，通过 OpenGL API 查询该 Shader 程序中所有 active uniform 变量的名称、类型、location 等信息，供 Material 类使用。

### 3.2 Uniform 类型枚举

#### 方案 A：复用现有 `ShaderDataType`（推荐 ???）

**做法**：直接复用 `Buffer.h` 中已有的 `ShaderDataType` 枚举，并为其添加 `Sampler2D` 类型。

**优点**：
- 不引入新的枚举类型，减少概念冗余
- 项目中已有 `ShaderDataTypeSize()` 等工具函数可直接使用

**缺点**：
- `ShaderDataType` 原本是为顶点属性设计的，语义上不完全匹配 uniform 类型
- 需要修改已有的 `Buffer.h` 文件

**具体修改**：在 `Buffer.h` 的 `ShaderDataType` 枚举中添加：

```cpp
enum class ShaderDataType
{
    None = 0,
    Float, Float2, Float3, Float4,
    Mat3, Mat4,
    Int, Int2, Int3, Int4,
    Bool,
    Sampler2D   // 新增：2D 纹理采样器
};
```

同时在 `ShaderDataTypeSize()` 中添加对应 case：

```cpp
case ShaderDataType::Sampler2D: return 4;  // sampler 本质是一个 int（纹理槽索引）
```

#### 方案 B：新建独立的 `UniformType` 枚举

**做法**：在 `Material.h` 或单独的头文件中定义一个新的枚举。

```cpp
enum class UniformType
{
    None = 0,
    Float, Float2, Float3, Float4,
    Int,
    Mat3, Mat4,
    Sampler2D
};
```

**优点**：
- 语义清晰，专门用于 uniform 参数
- 不影响现有代码

**缺点**：
- 与 `ShaderDataType` 高度重复
- 需要在两个枚举之间做转换

#### 推荐

**推荐方案 A**。`ShaderDataType` 本质上就是描述"着色器中的数据类型"，用于 uniform 参数完全合理。添加 `Sampler2D` 是自然的扩展。

---

### 3.3 ShaderUniformInfo 结构体

在 `Shader.h` 中新增：

```cpp
/// <summary>
/// Shader Uniform 参数信息（通过 OpenGL 内省获取）
/// </summary>
struct ShaderUniformInfo
{
    std::string Name;           // uniform 变量名（如 "u_DiffuseCoeff"）
    ShaderDataType Type;        // 数据类型
    int Location;               // uniform location（glGetUniformLocation 返回值）
    int Size;                   // 数组大小（非数组为 1）
};
```

### 3.4 Shader 类修改

在 `Shader.h` 的 `Shader` 类中新增：

```cpp
public:
    /// <summary>
    /// 获取所有 active uniform 参数信息列表
    /// </summary>
    /// <returns>uniform 参数信息列表</returns>
    const std::vector<ShaderUniformInfo>& GetUniforms() const { return m_Uniforms; }

    /// <summary>
    /// 获取着色器程序 ID（供 Material 等外部类使用）
    /// </summary>
    uint32_t GetRendererID() const { return m_RendererID; }

private:
    /// <summary>
    /// 内省：查询 Shader 程序中所有 active uniform 参数
    /// 在 Compile() 成功后调用
    /// </summary>
    void Introspect();

    std::vector<ShaderUniformInfo> m_Uniforms;  // uniform 参数信息列表
```

### 3.5 Introspect() 实现

在 `Shader.cpp` 中实现。需要 `#include <glad/glad.h>`（已有）。

#### GLenum 到 ShaderDataType 的转换函数

```cpp
/// <summary>
/// 将 OpenGL uniform 类型转换为 ShaderDataType
/// </summary>
/// <param name="glType">OpenGL 类型枚举</param>
/// <returns>对应的 ShaderDataType</returns>
static ShaderDataType GLUniformTypeToShaderDataType(GLenum glType)
{
    switch (glType)
    {
        case GL_FLOAT:          return ShaderDataType::Float;
        case GL_FLOAT_VEC2:     return ShaderDataType::Float2;
        case GL_FLOAT_VEC3:     return ShaderDataType::Float3;
        case GL_FLOAT_VEC4:     return ShaderDataType::Float4;
        case GL_INT:            return ShaderDataType::Int;
        case GL_FLOAT_MAT3:     return ShaderDataType::Mat3;
        case GL_FLOAT_MAT4:     return ShaderDataType::Mat4;
        case GL_SAMPLER_2D:     return ShaderDataType::Sampler2D;
        default:
            LF_CORE_WARN("Unsupported GL uniform type: {0}", glType);
            return ShaderDataType::None;
    }
}
```

#### Introspect() 方法

```cpp
void Shader::Introspect()
{
    m_Uniforms.clear();

    int uniformCount = 0;
    glGetProgramiv(m_RendererID, GL_ACTIVE_UNIFORMS, &uniformCount);

    for (int i = 0; i < uniformCount; i++)
    {
        char name[256];
        int length = 0;
        int size = 0;
        GLenum type = 0;

        glGetActiveUniform(m_RendererID, (GLuint)i, sizeof(name), &length, &size, &type, name);

        int location = glGetUniformLocation(m_RendererID, name);

        // location == -1 表示该 uniform 属于 Uniform Block（UBO），跳过
        if (location == -1)
            continue;

        ShaderDataType dataType = GLUniformTypeToShaderDataType(type);
        if (dataType == ShaderDataType::None)
            continue;  // 不支持的类型，跳过

        ShaderUniformInfo info;
        info.Name = std::string(name);
        info.Type = dataType;
        info.Location = location;
        info.Size = size;

        m_Uniforms.push_back(info);

        LF_CORE_TRACE("  Uniform #{0}: name='{1}', type={2}, location={3}, size={4}",
            i, info.Name, (int)info.Type, info.Location, info.Size);
    }

    LF_CORE_INFO("Shader '{0}' introspection complete: {1} active uniforms", m_Name, m_Uniforms.size());
}
```

#### 调用时机

在 `Shader::Compile()` 方法的末尾，`m_RendererID = program;` 之后调用：

```cpp
m_RendererID = program;

Introspect();  // 内省：获取所有 active uniform 信息
```

### 3.6 关于 Sampler2D 数组的处理

当前着色器中有 `uniform sampler2D u_Textures[32]`，`glGetActiveUniform` 会将其报告为：
- 名称：`u_Textures[0]`（注意带 `[0]` 后缀）
- 类型：`GL_SAMPLER_2D`
- size：`32`

**处理方式**：在 `Introspect()` 中，对于 `size > 1` 的 sampler 数组，可以选择：

#### 方案 A：整体作为一个 uniform 记录（推荐 ???）

只记录一条 `ShaderUniformInfo`，`Size = 32`。Material 中不暴露此参数给用户编辑（纹理数组由引擎内部管理）。

#### 方案 B：展开为多个独立 uniform

为数组的每个元素创建一条记录（`u_Textures[0]`, `u_Textures[1]`, ...）。

**推荐方案 A**，因为纹理数组是引擎内部的纹理槽管理机制，不应暴露给用户。

---

## 4. Material 类

### 4.1 MaterialProperty 结构体

#### 属性值存储方案

##### 方案 A：使用 `std::variant`（推荐 ???）

```cpp
#include <variant>

/// <summary>
/// 材质属性值：使用 std::variant 存储多种类型的值
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
```

**优点**：
- 类型安全，编译期检查
- 标准库支持，无需额外依赖
- 使用 `std::get<T>()` 或 `std::visit()` 访问值

**缺点**：
- 需要 C++17 支持（你的项目已使用 C++17 特性如 structured bindings，所以没问题）
- `variant` 的大小等于最大成员的大小（`glm::mat4` = 64 字节），可能有些浪费

##### 方案 B：使用 `union` + 类型标记

```cpp
struct MaterialPropertyValue
{
    ShaderDataType Type;
    union
    {
        float FloatValue;
        glm::vec2 Float2Value;
        glm::vec3 Float3Value;
        glm::vec4 Float4Value;
        int IntValue;
        glm::mat3 Mat3Value;
        glm::mat4 Mat4Value;
    };
    Ref<Texture2D> TextureValue;  // 不能放在 union 中（有析构函数）
};
```

**优点**：
- 不需要 C++17
- 内存布局更紧凑

**缺点**：
- 手动管理类型标记，容易出错
- `Ref<Texture2D>` 不能放在 union 中，需要单独存储
- 代码更复杂

##### 方案 C：使用字节数组 + 类型标记

```cpp
struct MaterialPropertyValue
{
    ShaderDataType Type;
    std::vector<uint8_t> Data;  // 原始字节数据
    Ref<Texture2D> TextureValue;
};
```

**优点**：
- 灵活，可存储任意大小的数据

**缺点**：
- 需要手动序列化/反序列化
- 类型不安全
- 堆分配开销

**推荐方案 A**（`std::variant`），类型安全且代码简洁。

#### MaterialProperty 定义

```cpp
/// <summary>
/// 材质属性：对应 Shader 中的一个 uniform 参数
/// </summary>
struct MaterialProperty
{
    std::string Name;               // 属性名（与 Shader uniform 名一致）
    ShaderDataType Type;            // 属性类型
    MaterialPropertyValue Value;    // 属性值
};
```

### 4.2 Material 类定义

```cpp
// Material.h
#pragma once

#include <variant>

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Shader.h"
#include "Lucky/Renderer/Texture.h"

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 材质属性值
    /// </summary>
    using MaterialPropertyValue = std::variant<
        float,
        glm::vec2,
        glm::vec3,
        glm::vec4,
        int,
        glm::mat3,
        glm::mat4,
        Ref<Texture2D>
    >;

    /// <summary>
    /// 材质属性
    /// </summary>
    struct MaterialProperty
    {
        std::string Name;               // 属性名
        ShaderDataType Type;            // 属性类型
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

        /// <summary>
        /// 设置着色器：会触发属性列表重建
        /// </summary>
        /// <param name="shader">新的着色器</param>
        void SetShader(const Ref<Shader>& shader);

        /// <summary>
        /// 获取当前着色器
        /// </summary>
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
        glm::vec3 GetFloat3(const std::string& name) const;
        // ... 其他 Get 方法类似

        /// <summary>
        /// 应用材质：绑定 Shader 并将所有属性值上传到 GPU
        /// </summary>
        void Apply() const;

        /// <summary>
        /// 获取所有材质属性（供编辑器 Inspector 使用）
        /// </summary>
        std::vector<MaterialProperty>& GetProperties() { return m_Properties; }
        const std::vector<MaterialProperty>& GetProperties() const { return m_Properties; }

        /// <summary>
        /// 获取材质名称
        /// </summary>
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

        Ref<Shader> m_Shader;                       // 着色器
        std::vector<MaterialProperty> m_Properties;  // 材质属性列表
        std::string m_Name = "New Material";         // 材质名称
    };
}
```

### 4.3 Material 类实现

```cpp
// Material.cpp
#include "lcpch.h"
#include "Material.h"

namespace Lucky
{
    // ==================== 辅助函数 ====================

    /// <summary>
    /// 判断是否为引擎内部 uniform（不暴露给用户编辑）
    /// </summary>
    static bool IsEngineInternalUniform(const std::string& name)
    {
        // 引擎内部管理的 uniform 黑名单
        static const std::unordered_set<std::string> s_EngineUniforms = {
            "u_ObjectToWorldMatrix",    // 模型变换矩阵（由引擎每帧设置）
            "u_Textures[0]",           // 纹理数组（由引擎纹理槽系统管理）
        };

        return s_EngineUniforms.find(name) != s_EngineUniforms.end();
    }

    /// <summary>
    /// 根据类型返回默认值
    /// </summary>
    static MaterialPropertyValue GetDefaultValue(ShaderDataType type)
    {
        switch (type)
        {
            case ShaderDataType::Float:     return 0.0f;
            case ShaderDataType::Float2:    return glm::vec2(0.0f);
            case ShaderDataType::Float3:    return glm::vec3(0.0f);
            case ShaderDataType::Float4:    return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            case ShaderDataType::Int:       return 0;
            case ShaderDataType::Mat3:      return glm::mat3(1.0f);
            case ShaderDataType::Mat4:      return glm::mat4(1.0f);
            case ShaderDataType::Sampler2D: return Ref<Texture2D>(nullptr);
            default:                        return 0.0f;
        }
    }

    // ==================== Material 实现 ====================

    Ref<Material> Material::Create(const Ref<Shader>& shader)
    {
        return CreateRef<Material>(shader);
    }

    Material::Material(const Ref<Shader>& shader)
        : m_Shader(shader)
    {
        if (m_Shader)
        {
            m_Name = m_Shader->GetName() + " Material";
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
        if (prop && prop->Type == ShaderDataType::Float)
        {
            prop->Value = value;
        }
    }

    void Material::SetFloat2(const std::string& name, const glm::vec2& value)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderDataType::Float2)
        {
            prop->Value = value;
        }
    }

    void Material::SetFloat3(const std::string& name, const glm::vec3& value)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderDataType::Float3)
        {
            prop->Value = value;
        }
    }

    void Material::SetFloat4(const std::string& name, const glm::vec4& value)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderDataType::Float4)
        {
            prop->Value = value;
        }
    }

    void Material::SetInt(const std::string& name, int value)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderDataType::Int)
        {
            prop->Value = value;
        }
    }

    void Material::SetMat3(const std::string& name, const glm::mat3& value)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderDataType::Mat3)
        {
            prop->Value = value;
        }
    }

    void Material::SetMat4(const std::string& name, const glm::mat4& value)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderDataType::Mat4)
        {
            prop->Value = value;
        }
    }

    void Material::SetTexture(const std::string& name, const Ref<Texture2D>& texture)
    {
        MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderDataType::Sampler2D)
        {
            prop->Value = texture;
        }
    }

    // ---- 属性获取方法（示例） ----

    float Material::GetFloat(const std::string& name) const
    {
        const MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderDataType::Float)
        {
            return std::get<float>(prop->Value);
        }
        return 0.0f;
    }

    glm::vec3 Material::GetFloat3(const std::string& name) const
    {
        const MaterialProperty* prop = FindProperty(name);
        if (prop && prop->Type == ShaderDataType::Float3)
        {
            return std::get<glm::vec3>(prop->Value);
        }
        return glm::vec3(0.0f);
    }

    // ---- Apply：上传所有属性到 GPU ----

    void Material::Apply() const
    {
        if (!m_Shader) return;

        m_Shader->Bind();

        int textureSlot = 1;  // 0 号槽位保留给引擎白色纹理

        for (const auto& prop : m_Properties)
        {
            switch (prop.Type)
            {
                case ShaderDataType::Float:
                    m_Shader->SetFloat(prop.Name, std::get<float>(prop.Value));
                    break;
                case ShaderDataType::Float2:
                    m_Shader->SetFloat2(prop.Name, std::get<glm::vec2>(prop.Value));
                    break;
                case ShaderDataType::Float3:
                    m_Shader->SetFloat3(prop.Name, std::get<glm::vec3>(prop.Value));
                    break;
                case ShaderDataType::Float4:
                    m_Shader->SetFloat4(prop.Name, std::get<glm::vec4>(prop.Value));
                    break;
                case ShaderDataType::Int:
                    m_Shader->SetInt(prop.Name, std::get<int>(prop.Value));
                    break;
                case ShaderDataType::Mat4:
                    m_Shader->SetMat4(prop.Name, std::get<glm::mat4>(prop.Value));
                    break;
                case ShaderDataType::Sampler2D:
                {
                    const auto& texture = std::get<Ref<Texture2D>>(prop.Value);
                    if (texture)
                    {
                        texture->Bind(textureSlot);
                        m_Shader->SetInt(prop.Name, textureSlot);
                        textureSlot++;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    // ---- RebuildProperties：根据 Shader 的 uniform 列表重建属性 ----

    void Material::RebuildProperties()
    {
        auto oldProperties = m_Properties;  // 保存旧属性
        m_Properties.clear();

        if (!m_Shader) return;

        const auto& uniforms = m_Shader->GetUniforms();

        for (const auto& uniform : uniforms)
        {
            // 跳过引擎内部 uniform
            if (IsEngineInternalUniform(uniform.Name))
                continue;

            MaterialProperty prop;
            prop.Name = uniform.Name;
            prop.Type = uniform.Type;
            prop.Value = GetDefaultValue(uniform.Type);

            // 尝试从旧属性中恢复同名同类型的值
            for (const auto& oldProp : oldProperties)
            {
                if (oldProp.Name == prop.Name && oldProp.Type == prop.Type)
                {
                    prop.Value = oldProp.Value;
                    break;
                }
            }

            m_Properties.push_back(prop);
        }

        LF_CORE_INFO("Material '{0}' properties rebuilt: {1} properties", m_Name, m_Properties.size());
    }

    // ---- FindProperty ----

    MaterialProperty* Material::FindProperty(const std::string& name)
    {
        for (auto& prop : m_Properties)
        {
            if (prop.Name == name)
                return &prop;
        }
        return nullptr;
    }

    const MaterialProperty* Material::FindProperty(const std::string& name) const
    {
        for (const auto& prop : m_Properties)
        {
            if (prop.Name == name)
                return &prop;
        }
        return nullptr;
    }
}
```

### 4.4 关于引擎内部 Uniform 过滤策略

#### 方案 A：黑名单过滤（推荐 ???）

维护一个 `std::unordered_set<std::string>` 黑名单，列出所有引擎内部管理的 uniform 名称。

```cpp
static const std::unordered_set<std::string> s_EngineUniforms = {
    "u_ObjectToWorldMatrix",
    "u_Textures[0]",
};
```

**优点**：
- 简单直接，易于维护
- 新增引擎 uniform 时只需添加到集合中

**缺点**：
- 需要手动维护，可能遗漏

#### 方案 B：命名约定前缀过滤

约定引擎内部 uniform 使用特定前缀（如 `_Engine_` 或 `u_Engine_`），材质参数使用另一个前缀（如 `u_` 或无前缀）。

**优点**：
- 自动化，不需要维护列表
- 规则清晰

**缺点**：
- 需要修改现有着色器代码中的 uniform 命名
- 约束了着色器编写者的命名自由度

#### 方案 C：在 Shader 源码中使用注释标记

在 GLSL 源码中用特殊注释标记哪些 uniform 是材质参数：

```glsl
uniform vec3 u_DiffuseCoeff;  // @material
uniform mat4 u_ObjectToWorldMatrix;  // @engine
```

然后在 `Shader::ReadFile()` 时解析这些标记。

**优点**：
- 最灵活，着色器编写者完全控制
- 可以附加更多元数据（如显示名称、范围等）

**缺点**：
- 需要解析 GLSL 源码，增加复杂度
- 当前阶段过于复杂

**推荐方案 A**（黑名单），简单可靠，后续可以升级到方案 C。

---

## 5. MeshRendererComponent

### 5.1 定义

```cpp
// MeshRendererComponent.h
#pragma once

#include "Lucky/Renderer/Material.h"

namespace Lucky
{
    /// <summary>
    /// 网格渲染器组件：持有材质列表，材质索引对应 Mesh 中 SubMesh 的 MaterialIndex
    /// </summary>
    struct MeshRendererComponent
    {
        std::vector<Ref<Material>> Materials;  // 材质列表

        MeshRendererComponent() = default;
        MeshRendererComponent(const MeshRendererComponent& other) = default;

        /// <summary>
        /// 获取指定索引的材质，索引越界返回 nullptr
        /// </summary>
        Ref<Material> GetMaterial(uint32_t index) const
        {
            if (index < Materials.size())
                return Materials[index];
            return nullptr;
        }

        /// <summary>
        /// 设置指定索引的材质，自动扩展列表大小
        /// </summary>
        void SetMaterial(uint32_t index, const Ref<Material>& material)
        {
            if (index >= Materials.size())
                Materials.resize(index + 1);
            Materials[index] = material;
        }
    };
}
```

### 5.2 注册到 ECS 系统

需要在 `Scene.cpp` 中添加 `OnComponentAdded` 特化：

```cpp
#include "Components/MeshRendererComponent.h"

template<>
void Scene::OnComponentAdded<MeshRendererComponent>(Entity entity, MeshRendererComponent& component)
{
    // 暂时为空，后续可以在这里做初始化
}
```

---

## 6. 完整的文件修改清单

### 6.1 新建文件

#### `Lucky/Source/Lucky/Renderer/Material.h`
- 定义 `MaterialPropertyValue`（`std::variant`）
- 定义 `MaterialProperty` 结构体
- 定义 `Material` 类

#### `Lucky/Source/Lucky/Renderer/Material.cpp`
- 实现 `Material` 类的所有方法
- 实现 `IsEngineInternalUniform()` 辅助函数
- 实现 `GetDefaultValue()` 辅助函数

#### `Lucky/Source/Lucky/Scene/Components/MeshRendererComponent.h`
- 定义 `MeshRendererComponent` 结构体

### 6.2 修改文件

#### `Lucky/Source/Lucky/Renderer/Buffer.h`
- 在 `ShaderDataType` 枚举中添加 `Sampler2D`
- 在 `ShaderDataTypeSize()` 中添加 `Sampler2D` 的 case

#### `Lucky/Source/Lucky/Renderer/Shader.h`
- 添加 `#include "Buffer.h"`（为了使用 `ShaderDataType`）
- 添加 `ShaderUniformInfo` 结构体
- 在 `Shader` 类中添加 `GetUniforms()` 公有方法
- 在 `Shader` 类中添加 `GetRendererID()` 公有方法
- 在 `Shader` 类中添加 `Introspect()` 私有方法
- 在 `Shader` 类中添加 `m_Uniforms` 私有成员

#### `Lucky/Source/Lucky/Renderer/Shader.cpp`
- 添加 `GLUniformTypeToShaderDataType()` 静态函数
- 实现 `Shader::Introspect()` 方法
- 在 `Shader::Compile()` 末尾调用 `Introspect()`

#### `Lucky/Source/Lucky/Scene/Scene.cpp`
- 添加 `#include "Components/MeshRendererComponent.h"`
- 添加 `OnComponentAdded<MeshRendererComponent>` 特化

---

## 7. 编译验证

Phase 1 完成后，可以通过以下方式验证：

1. **编译通过**：确保所有新增文件和修改文件能正确编译
2. **Shader 内省验证**：在 `Renderer3D::Init()` 中，创建 Shader 后打印 uniform 列表：
   ```cpp
   // 在 Init() 中，创建 MeshShader 后
   const auto& uniforms = s_Data.MeshShader->GetUniforms();
   for (const auto& u : uniforms)
   {
       LF_CORE_INFO("Uniform: {0}, Type: {1}, Location: {2}", u.Name, (int)u.Type, u.Location);
   }
   ```
   预期输出应包含：`u_ObjectToWorldMatrix`, `u_TextureIndex`, `u_LightDirection`, `u_LightColor`, `u_LightIntensity`, `u_AmbientCoeff`, `u_DiffuseCoeff`, `u_SpecularCoeff`, `u_Shininess`, `u_Textures[0]`
   
   **不应包含**：`u_ViewProjectionMatrix`, `u_CameraPos`（这些在 UBO 中，location 为 -1）

3. **Material 创建验证**：
   ```cpp
   auto material = Material::Create(s_Data.MeshShader);
   const auto& props = material->GetProperties();
   for (const auto& p : props)
   {
       LF_CORE_INFO("Material Property: {0}, Type: {1}", p.Name, (int)p.Type);
   }
   ```
   预期输出应包含除 `u_ObjectToWorldMatrix` 和 `u_Textures[0]` 之外的所有 uniform

---

## 8. 注意事项

1. **头文件循环依赖**：`Material.h` 包含 `Shader.h`，`Shader.h` 包含 `Buffer.h`，确保不会形成循环
2. **`std::variant` 需要 C++17**：确认项目的编译标准设置为 C++17 或更高（检查 premake/vcxproj 配置）
3. **`Ref<Texture2D>(nullptr)` 的 variant 存储**：`std::variant` 可以存储 `Ref<Texture2D>` 的空指针，但需要确保在 `Apply()` 中检查空指针
4. **线程安全**：当前设计不考虑多线程，所有操作在主线程（渲染线程）执行
