# Phase 5：材质属性存储结构优化（vector → unordered_map）

## 1. 概述

### 1.1 目标

将 `Material` 类中材质属性的存储结构从 `std::vector<MaterialProperty>` 改为 `std::unordered_map<std::string, MaterialProperty>` + `std::vector<std::string>`（有序键列表），以提升按名查找的代码清晰度和可维护性。

### 1.2 改动范围

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| `Lucky/Source/Lucky/Renderer/Material.h` | **修改** | 更换成员变量类型，调整公开接口 |
| `Lucky/Source/Lucky/Renderer/Material.cpp` | **修改** | 重写所有方法实现 |
| `Luck3DApp/Source/Panels/InspectorPanel.cpp` | **修改** | 适配新的遍历接口 |

### 1.3 不改动的部分

- `Shader.h` / `Shader.cpp`：`m_Uniforms` 仅用于顺序遍历，无按名查找需求，保持 `std::vector<ShaderUniform>` 不变。
- `InspectorPanel.h`：接口声明无需变化。

---

## 2. 设计方案

### 2.1 核心数据结构

```
┌─────────────────────────────────────────────────────┐
│                    Material                          │
│                                                     │
│  m_PropertyMap:  unordered_map<string, Property>    │
│  ┌──────────────┬───────────────────────────┐       │
│  │ "u_Color"    │ { Name, Type, Value }     │       │
│  │ "u_Shininess"│ { Name, Type, Value }     │       │
│  │ "u_AlbedoTex"│ { Name, Type, Value }     │       │
│  └──────────────┴───────────────────────────┘       │
│                                                     │
│  m_PropertyOrder:  vector<string>                   │
│  ┌─────────────────────────────────────────┐        │
│  │ "u_Color" → "u_Shininess" → "u_AlbedoTex"│       │
│  └─────────────────────────────────────────┘        │
│                                                     │
│  查找：m_PropertyMap.find(name)        → O(1)       │
│  遍历：for (name : m_PropertyOrder)    → 有序       │
└─────────────────────────────────────────────────────┘
```

### 2.2 设计原则

1. **按名查找 O(1)**：所有 `SetXxx` / `GetXxx` 方法通过 `m_PropertyMap.find()` 直接定位，不再线性遍历。
2. **保持声明顺序**：`m_PropertyOrder` 记录属性在 Shader 中的声明顺序，`Apply()` 和 Inspector 遍历时按此顺序访问。
3. **对外接口兼容**：`SetFloat` / `GetFloat` 等公开方法签名不变，调用方无需修改。
4. **Inspector 遍历接口调整**：原 `GetProperties()` 返回 `vector<MaterialProperty>&`，改为提供有序遍历接口。

---

## 3. 详细改动

### 3.1 Material.h 完整改动后代码

```cpp
#pragma once

#include <variant>
#include <unordered_map>
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
        /// 设置着色器：会触发属性重建
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

        /// <summary>
        /// 按名获取属性指针（未找到返回 nullptr）
        /// </summary>
        MaterialProperty* GetProperty(const std::string& name);
        const MaterialProperty* GetProperty(const std::string& name) const;
        
        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }
    private:
        /// <summary>
        /// 根据当前 Shader 的 uniform 列表重建属性
        /// 切换 Shader 时调用，会尽量保留同名同类型的旧属性值
        /// </summary>
        void RebuildProperties();
        
        /// <summary>
        /// 查找指定名称的属性，返回指针（未找到返回 nullptr）
        /// </summary>
        MaterialProperty* FindProperty(const std::string& name);
        const MaterialProperty* FindProperty(const std::string& name) const;
    private:
        Ref<Shader> m_Shader;                                                   // 着色器
        std::unordered_map<std::string, MaterialProperty> m_PropertyMap;        // 材质属性 Map：属性名 - 属性
        std::vector<std::string> m_PropertyOrder;                               // 属性名有序列表（按 Shader uniform 声明顺序）
        std::string m_Name;                                                     // 材质名称
    };
}
```

#### 3.1.1 Material.h 改动点说明

| # | 改动 | 说明 |
|---|------|------|
| 1 | 新增 `#include <unordered_map>` | 引入 `std::unordered_map` 头文件 |
| 2 | 删除 `GetProperties()` | 原返回 `std::vector<MaterialProperty>&`，不再适用 |
| 3 | 新增 `GetPropertyOrder()` | 返回 `const std::vector<std::string>&`，供外部有序遍历 |
| 4 | 新增 `GetPropertyMap()` | 返回 `const std::unordered_map<std::string, MaterialProperty>&`，供外部按名访问 |
| 5 | 新增 `GetProperty(name)` | 公开的按名查找接口，返回指针（替代原来的 private `FindProperty`） |
| 6 | 成员变量 `m_Properties` → `m_PropertyMap` + `m_PropertyOrder` | 核心改动 |

---

### 3.2 Material.cpp 完整改动后代码

```cpp
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
        const MaterialProperty* prop = FindProperty(name);
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
    
    // ---- 属性获取方法 End ----

    // ---- 公开的按名获取属性方法 ----

    MaterialProperty* Material::GetProperty(const std::string& name)
    {
        return FindProperty(name);
    }

    const MaterialProperty* Material::GetProperty(const std::string& name) const
    {
        return FindProperty(name);
    }

    // ---- Apply ----

    void Material::Apply() const
    {
        if (!m_Shader)
        {
            return;
        }

        int textureSlot = 1;  // 0 号槽位保留给引擎白色纹理 TODO 获取第一个可用槽位

        // 按声明顺序遍历属性，设置 Shader uniform
        for (const std::string& name : m_PropertyOrder)
        {
            auto it = m_PropertyMap.find(name);
            if (it == m_PropertyMap.end())
            {
                continue;
            }

            const MaterialProperty& prop = it->second;

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
                        textureSlot++;  // TODO 限制最大值32
                    }
                    else
                    {
                        m_Shader->SetInt(prop.Name, 0);   // 纹理为空时 设置纹理槽位索引为 0
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    // ---- RebuildProperties ----

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

            // 尝试从旧属性中恢复同名同类型的值（O(1) 查找）
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
        return it != m_PropertyMap.end() ? &it->second : nullptr;
    }

    const MaterialProperty* Material::FindProperty(const std::string& name) const
    {
        auto it = m_PropertyMap.find(name);
        return it != m_PropertyMap.end() ? &it->second : nullptr;
    }
}
```

#### 3.2.1 Material.cpp 改动点说明

| # | 方法 | 改动 | 说明 |
|---|------|------|------|
| 1 | `FindProperty` | **重写** | 从 `for` 循环线性遍历改为 `m_PropertyMap.find()` 一行代码 |
| 2 | `Apply` | **重写遍历方式** | 从 `for (prop : m_Properties)` 改为 `for (name : m_PropertyOrder)` + `m_PropertyMap.find(name)` |
| 3 | `RebuildProperties` | **重写** | 旧属性保存为 map（`std::move`），恢复旧值时用 `find()` 替代内层 `for` 循环 |
| 4 | `GetProperty` | **新增** | 公开的按名查找方法，委托给 `FindProperty` |
| 5 | 所有 `SetXxx` / `GetXxx` | **不变** | 内部调用 `FindProperty`，签名不变，但 `FindProperty` 已从 O(n) 变为 O(1) |
| 6 | 静态函数 | **不变** | `IsInternalUniform`、`GetMaterialPropertyDefaultValue` 无需改动 |

---

### 3.3 InspectorPanel.cpp 改动

仅需修改 `DrawMaterialEditor` 方法中遍历材质属性的部分。

#### 3.3.1 改动前（当前代码）

```cpp
// 绘制材质属性
for (const MaterialProperty& prop : material->GetProperties())
{
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
    // ... 其他类型 ...
    }
}
```

#### 3.3.2 改动后

```cpp
// 绘制材质属性（按 Shader uniform 声明顺序遍历）
const auto& propertyMap = material->GetPropertyMap();
for (const std::string& propName : material->GetPropertyOrder())
{
    auto it = propertyMap.find(propName);
    if (it == propertyMap.end())
    {
        continue;
    }

    const MaterialProperty& prop = it->second;

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
            texture = Renderer3D::GetWhiteTexture();    // 默认使用白色纹理
        }
            
        uint32_t textureID = texture->GetRendererID();
        if (ImGui::ImageButton((ImTextureID)textureID, { 64, 64 }, { 0, 1 }, { 1, 0 }))
        {
            std::string filepath = FileDialogs::OpenFile("Albedo Texture(*.png;*.jpg)\0*.png;*.jpg\0");
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
}
```

#### 3.3.3 InspectorPanel.cpp 改动点说明

| # | 改动 | 说明 |
|---|------|------|
| 1 | 遍历方式 | 从 `for (prop : material->GetProperties())` 改为 `for (name : material->GetPropertyOrder())` + `propertyMap.find(name)` |
| 2 | switch 内部逻辑 | **完全不变**，仍然通过 `material->SetFloat(prop.Name, value)` 等方法设置值 |

---

## 4. 改动前后对比

### 4.1 FindProperty 对比

```
改动前（O(n) 线性遍历）：                    改动后（O(1) 哈希查找）：
                                            
MaterialProperty* FindProperty(name)        MaterialProperty* FindProperty(name)
{                                           {
    for (prop : m_Properties)                   auto it = m_PropertyMap.find(name);
    {                                           return it != m_PropertyMap.end()
        if (prop.Name == name)                      ? &it->second : nullptr;
            return &prop;                   }
    }                                       
    return nullptr;                         
}                                           
```

### 4.2 RebuildProperties 恢复旧值对比

```
改动前（O(n) 内层循环）：                    改动后（O(1) 哈希查找）：
                                            
// 恢复旧值                                 // 恢复旧值
for (oldProp : oldProperties)               auto oldIt = oldPropertyMap.find(name);
{                                           if (oldIt != oldPropertyMap.end()
    if (oldProp.Name == prop.Name               && oldIt->second.Type == prop.Type)
        && oldProp.Type == prop.Type)        {
    {                                           prop.Value = oldIt->second.Value;
        prop.Value = oldProp.Value;          }
        break;                              
    }                                       
}                                           
```

### 4.3 Apply 遍历对比

```
改动前：                                     改动后：
                                            
for (const auto& prop : m_Properties)       for (const auto& name : m_PropertyOrder)
{                                           {
    switch (prop.Type) { ... }                  auto it = m_PropertyMap.find(name);
}                                               const auto& prop = it->second;
                                                switch (prop.Type) { ... }
                                            }
```

---

## 5. 公开接口变更总结

### 5.1 删除的接口

```cpp
// 删除：不再返回 vector 引用
std::vector<MaterialProperty>& GetProperties();
const std::vector<MaterialProperty>& GetProperties() const;
```

### 5.2 新增的接口

```cpp
// 新增：获取属性的有序名称列表
const std::vector<std::string>& GetPropertyOrder() const;

// 新增：获取属性 Map
const std::unordered_map<std::string, MaterialProperty>& GetPropertyMap() const;

// 新增：按名获取属性指针（公开版本的 FindProperty）
MaterialProperty* GetProperty(const std::string& name);
const MaterialProperty* GetProperty(const std::string& name) const;
```

### 5.3 不变的接口

```cpp
// 以下接口签名完全不变，调用方无需修改
void SetFloat(const std::string& name, float value);
void SetFloat2(const std::string& name, const glm::vec2& value);
// ... 其他 SetXxx / GetXxx 方法 ...
void Apply() const;
void SetShader(const Ref<Shader>& shader);
Ref<Shader> GetShader() const;
const std::string& GetName() const;
void SetName(const std::string& name);
static Ref<Material> Create(const Ref<Shader>& shader);
```

---

## 6. 实施步骤

按以下顺序修改，每步完成后可编译验证：

### Step 1：修改 Material.h

1. 添加 `#include <unordered_map>`
2. 删除 `GetProperties()` 的两个重载
3. 新增 `GetPropertyOrder()`、`GetPropertyMap()`、`GetProperty()` 接口
4. 将成员变量 `std::vector<MaterialProperty> m_Properties` 替换为 `std::unordered_map<std::string, MaterialProperty> m_PropertyMap` + `std::vector<std::string> m_PropertyOrder`

### Step 2：修改 Material.cpp

1. 重写 `FindProperty`：改为 `m_PropertyMap.find()`
2. 新增 `GetProperty` 实现（委托给 `FindProperty`）
3. 重写 `RebuildProperties`：使用 `std::move` 保存旧 map，恢复旧值用 `find()` 替代内层循环
4. 重写 `Apply`：按 `m_PropertyOrder` 顺序遍历，从 `m_PropertyMap` 中取属性

### Step 3：修改 InspectorPanel.cpp

1. 将 `material->GetProperties()` 遍历改为 `material->GetPropertyOrder()` + `material->GetPropertyMap().find()` 遍历

### Step 4：编译 & 测试

1. 编译整个解决方案，确保无编译错误
2. 运行程序，在 Inspector 中选中带材质的物体
3. 验证材质属性正常显示、编辑、切换 Shader 后属性正确重建

---

## 7. 注意事项

1. **`std::unordered_map` 迭代器稳定性**：`unordered_map` 在 `insert` / `erase` 后，已有元素的引用和指针仍然有效（不像 `vector` 可能因扩容而失效）。因此 `FindProperty` 返回的指针在后续操作中是安全的。

2. **`Shader::m_Uniforms` 不改**：Shader 的 uniform 列表只在 `RebuildProperties` 中顺序遍历，没有按名查找需求，保持 `std::vector<ShaderUniform>` 即可。

3. **线程安全**：当前引擎为单线程渲染，`unordered_map` 的非线程安全不构成问题。若未来引入多线程，需额外考虑。

4. **序列化兼容**：如果未来需要序列化材质属性（保存/加载），`m_PropertyOrder` 保证了序列化顺序的确定性，避免 `unordered_map` 遍历顺序不确定的问题。
