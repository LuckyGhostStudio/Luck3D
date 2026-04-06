# 场景序列化系统增强设计文档

> **文档版本**：v1.1  
> **创建日期**：2026-04-02  
> **最后更新**：2026-04-02  
> **前置依赖**：P1-1 材质属性 Map 改造（已完成）、P1-2 场景序列化基础（已完成）  
> **文档说明**：本文档在现有场景序列化基础上，补充 MeshRendererComponent（材质）序列化、MeshFilterComponent 完善、场景文件版本号、错误处理等增强功能的详细设计。所有代码可直接对照实现。
>
> **v1.1 变更**：将材质序列化逻辑从 `SceneSerializer` 中独立为 `MaterialSerializer` 类，遵循单一职责原则，便于未来迁移到独立 `.mat` 文件格式。

---

## 目录

- [一、现有序列化系统分析](#一现有序列化系统分析)
  - [1.1 已实现的功能](#11-已实现的功能)
  - [1.2 现有问题清单](#12-现有问题清单)
  - [1.3 现有场景文件格式](#13-现有场景文件格式)
- [二、增强目标](#二增强目标)
- [三、场景文件格式升级](#三场景文件格式升级)
  - [3.1 版本号字段](#31-版本号字段)
  - [3.2 完整的目标场景文件格式](#32-完整的目标场景文件格式)
- [四、MeshRendererComponent 材质序列化（核心）](#四meshrenderercomponent-材质序列化核心)
  - [4.1 设计决策：内联序列化 + 独立 MaterialSerializer](#41-设计决策内联序列化--独立-materialserializer)
  - [4.2 材质属性类型与 YAML 映射](#42-材质属性类型与-yaml-映射)
  - [4.3 MaterialSerializer 类设计](#43-materialserializer-类设计)
  - [4.4 MaterialSerializer 序列化实现](#44-materialserializer-序列化实现)
  - [4.5 MaterialSerializer 反序列化实现](#45-materialserializer-反序列化实现)
  - [4.6 在 SceneSerializer 中集成调用](#46-在-sceneserializer-中集成调用)
- [五、MeshFilterComponent 序列化完善](#五meshfiltercomponent-序列化完善)
  - [5.1 当前问题](#51-当前问题)
  - [5.2 改进方案](#52-改进方案)
- [六、错误处理与健壮性增强](#六错误处理与健壮性增强)
  - [6.1 反序列化安全读取](#61-反序列化安全读取)
  - [6.2 版本兼容策略](#62-版本兼容策略)
- [七、完整代码修改清单](#七完整代码修改清单)
  - [7.1 新增文件：MaterialSerializer.h](#71-新增文件materialserializerh)
  - [7.2 新增文件：MaterialSerializer.cpp](#72-新增文件materialserializercpp)
  - [7.3 修改文件：SceneSerializer.cpp](#73-修改文件sceneserializercpp)
  - [7.4 文件变更总览](#74-文件变更总览)
- [八、验证方法](#八验证方法)
- [九、后续扩展预留](#九后续扩展预留)

---

## 一、现有序列化系统分析

### 1.1 已实现的功能

| 组件 | 序列化 | 反序列化 | 状态 |
|------|--------|---------|------|
| `IDComponent` | ? UUID 作为 Entity Key | ? 从 Entity Key 读取 | 完整 |
| `NameComponent` | ? Name | ? Name | 完整 |
| `TransformComponent` | ? Position/Rotation(四元数)/Scale | ? 对应读取 | 完整 |
| `RelationshipComponent` | ? Parent + Children UUID 列表 | ? 对应读取 | 完整 |
| `DirectionalLightComponent` | ? Direction/Color/Intensity | ? 对应读取 | 完整 |
| `MeshFilterComponent` | ? Name + PrimitiveType | ? 仅通过 PrimitiveType 重建 | **部分完成** |
| `MeshRendererComponent` | ? 代码被注释 | ? 空 TODO | **未实现** |

### 1.2 现有问题清单

| 编号 | 问题 | 影响 | 优先级 |
|------|------|------|--------|
| **S-01** | `MeshRendererComponent` 序列化完全未实现 | 保存场景后再打开，所有材质属性丢失，回退到默认材质 | ?? 高 |
| **S-02** | `MeshFilterComponent` 仅支持 `PrimitiveType::Cube` | 其他图元类型（Plane/Sphere 等）反序列化后 Mesh 为空 | ?? 中 |
| **S-03** | 场景文件缺少版本号 | 未来格式变更时无法做向后兼容 | ?? 中 |
| **S-04** | 反序列化缺少错误处理 | YAML 节点缺失或类型错误时可能崩溃 | ?? 中 |
| **S-05** | `MeshRendererComponent` 反序列化时未调用 `AddComponent` | 反序列化逻辑不完整，直接 `GetComponent` 会失败 | ?? 高 |

### 1.3 现有场景文件格式

当前 `.luck3d` 文件格式（以 `TestScene.luck3d` 为例）：

```yaml
Scene: New Scene
Entitys:
  - Entity: 12488380559870933478
    NameComponent:
      Name: CubeTTT
    TransformComponent:
      Position: [0, 0, 0]
      Rotation: [-0.606519401, -0.171553805, -0.76431042, 0.136136726]
      Scale: [1.29999995, 2, 0.100000001]
    RelationshipComponent:
      Parent: 5488691528831279916
      Children:
        []
    MeshFilterComponent:
      Name: Cube
      PrimitiveType: 1
    # MeshRendererComponent 缺失！
```

---

## 二、增强目标

本次增强需要实现以下目标：

1. **实现 `MeshRendererComponent` 的完整序列化/反序列化**（材质属性内联到场景文件）
2. **完善 `MeshFilterComponent` 的序列化**（支持所有已定义的 `PrimitiveType`）
3. **添加场景文件版本号**（为未来格式变更做准备）
4. **增强反序列化的错误处理**（防止节点缺失或类型错误导致崩溃）

---

## 三、场景文件格式升级

### 3.1 版本号字段

在场景文件顶层添加 `Version` 字段：

```yaml
Scene: New Scene
Version: 1
Entitys:
  - Entity: ...
```

**版本号规则**：
- `Version: 1` — 当前版本（本次增强后的格式）
- 反序列化时，如果 `Version` 字段不存在，视为 `Version: 0`（旧格式，向后兼容）
- 未来格式变更时递增版本号，并在反序列化中根据版本号走不同的解析路径

### 3.2 完整的目标场景文件格式

增强后的 `.luck3d` 文件完整格式示例：

```yaml
Scene: My Scene
Version: 1
Entitys:
  - Entity: 12488380559870933478
    NameComponent:
      Name: Cube
    TransformComponent:
      Position: [0, 0, 0]
      Rotation: [0, 0, 0, 1]
      Scale: [1, 1, 1]
    RelationshipComponent:
      Parent: 0
      Children: []
    MeshFilterComponent:
      PrimitiveType: 1
    MeshRendererComponent:
      Materials:
        - Name: "New Material"
          Shader: "Standard"
          Properties:
            - Name: "u_AmbientCoeff"
              Type: Float3
              Value: [0.2, 0.2, 0.2]
            - Name: "u_DiffuseCoeff"
              Type: Float3
              Value: [0.8, 0.8, 0.8]
            - Name: "u_SpecularCoeff"
              Type: Float3
              Value: [0.5, 0.5, 0.5]
            - Name: "u_Shininess"
              Type: Float
              Value: 32.0
            - Name: "u_MainTexture"
              Type: Sampler2D
              Value: ""

  - Entity: 5787875344811688918
    NameComponent:
      Name: Directional Light
    TransformComponent:
      Position: [0, 0, 0]
      Rotation: [0.406246752, -0.249812275, 0.116489381, 0.871198952]
      Scale: [1, 1, 1]
    RelationshipComponent:
      Parent: 0
      Children: []
    DirectionalLightComponent:
      Direction: [0.406246752, -0.249812275, 0.116489381, 0.871198952]
      Color: [1, 1, 1]
      Intensity: 1
```

---

## 四、MeshRendererComponent 材质序列化（核心）

### 4.1 设计决策：内联序列化 + 独立 MaterialSerializer

#### 4.1.1 数据格式：内联序列化

**选择内联序列化（材质属性直接写在场景文件中）的原因**：

| 考量 | 说明 |
|------|------|
| 当前阶段需求 | 材质没有 UUID / AssetHandle，也没有独立的 `.mat` 文件格式 |
| 数据完整性 | 一个 `.luck3d` 文件包含场景的全部信息，不依赖外部文件 |
| 后续迁移 | 未来引入资产系统后，可以将内联材质提取为独立 `.mat` 文件，场景中改为存引用路径 |

**不选择独立 `.mat` 文件的原因**：
- 当前没有资产管理系统，无法统一管理材质文件的创建、引用、删除
- 材质复用需求尚不明确（当前每个实体的材质是独立的）
- 增加不必要的文件 I/O 复杂度

#### 4.1.2 代码架构：独立 MaterialSerializer 类

**将材质序列化逻辑从 `SceneSerializer` 中分离为独立的 `MaterialSerializer` 类**，而非全部写在 `SceneSerializer.cpp` 中。

**分离的原因**：

| 维度 | 全部写在 SceneSerializer | 独立 MaterialSerializer |
|------|------------------------|------------------------|
| **单一职责** | ? SceneSerializer 既管场景结构又管材质细节 | ? 各司其职，SceneSerializer 只关心"场景有哪些实体和组件"，MaterialSerializer 只关心"材质怎么读写" |
| **未来迁移成本** | ?? 引入 `.mat` 独立文件时，需要从 SceneSerializer 中**剥离**代码，改动面大 | ?? 直接在 MaterialSerializer 中新增 `SerializeToFile` / `DeserializeFromFile` 方法即可，**场景序列化代码零改动** |
| **复用性** | ? 如果编辑器面板、资产浏览器等也需要序列化材质，无法复用 | ? 任何地方都可以调用 `MaterialSerializer::Serialize` / `Deserialize` |
| **可测试性** | ? 测试材质序列化必须构造完整的 Scene + Entity | ? 可以单独测试材质的序列化/反序列化 |
| **代码体量** | SceneSerializer.cpp 膨胀到 700+ 行 | SceneSerializer.cpp 保持精简，MaterialSerializer.cpp 约 200 行 |

**文件位置**：`MaterialSerializer` 放在 `Renderer` 模块下，因为材质是渲染器的概念，不依赖 Scene/Entity：

```
Lucky/Source/Lucky/Renderer/
├── Material.h
├── Material.cpp
├── MaterialSerializer.h    ← 新增
├── MaterialSerializer.cpp  ← 新增
├── Shader.h
├── ...
```

### 4.2 材质属性类型与 YAML 映射

`ShaderUniformType` 枚举与 YAML 序列化格式的对应关系：

| ShaderUniformType | YAML Type 字符串 | YAML Value 格式 | 示例 |
|-------------------|------------------|-----------------|------|
| `Float` | `"Float"` | 标量 | `32.0` |
| `Float2` | `"Float2"` | 二元数组 | `[0.5, 0.5]` |
| `Float3` | `"Float3"` | 三元数组 | `[0.8, 0.8, 0.8]` |
| `Float4` | `"Float4"` | 四元数组 | `[1.0, 0.0, 0.0, 1.0]` |
| `Int` | `"Int"` | 整数标量 | `5` |
| `Mat3` | `"Mat3"` | 9 元数组（列主序） | `[1,0,0, 0,1,0, 0,0,1]` |
| `Mat4` | `"Mat4"` | 16 元数组（列主序） | `[1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]` |
| `Sampler2D` | `"Sampler2D"` | 纹理文件路径字符串 | `"Assets/Textures/wood.png"` |

### 4.3 MaterialSerializer 类设计

#### 4.3.1 头文件：MaterialSerializer.h

**文件位置**：`Lucky/Source/Lucky/Renderer/MaterialSerializer.h`

```cpp
#pragma once

#include "Material.h"
#include <yaml-cpp/yaml.h>

namespace Lucky
{
    /// <summary>
    /// 材质序列化器：负责 Material 与 YAML 之间的转换
    /// 当前阶段用于内联到场景文件中，未来可扩展为独立 .mat 文件的读写
    /// </summary>
    class MaterialSerializer
    {
    public:
        /// <summary>
        /// 将材质序列化到 YAML Emitter（内联到场景文件中使用）
        /// </summary>
        /// <param name="out">YAML 发射器</param>
        /// <param name="material">材质</param>
        static void Serialize(YAML::Emitter& out, const Ref<Material>& material);

        /// <summary>
        /// 从 YAML 节点反序列化材质（从场景文件中读取使用）
        /// </summary>
        /// <param name="materialNode">材质 YAML 节点</param>
        /// <returns>反序列化的材质（失败返回 nullptr）</returns>
        static Ref<Material> Deserialize(const YAML::Node& materialNode);

        // ---- 未来扩展：独立 .mat 文件 ----

        // /// <summary>
        // /// 将材质序列化到独立的 .mat 文件
        // /// </summary>
        // static void SerializeToFile(const std::string& filepath, const Ref<Material>& material);

        // /// <summary>
        // /// 从独立的 .mat 文件反序列化材质
        // /// </summary>
        // static Ref<Material> DeserializeFromFile(const std::string& filepath);
    };
}
```

#### 4.3.2 接口说明

| 方法 | 用途 | 调用方 |
|------|------|--------|
| `Serialize(out, material)` | 将材质写入 YAML Emitter 流 | `SceneSerializer::SerializeEntity` |
| `Deserialize(materialNode)` | 从 YAML 节点重建材质对象 | `SceneSerializer::Deserialize` |
| `SerializeToFile` (未来) | 将材质写入独立 `.mat` 文件 | 资产系统 / 编辑器 |
| `DeserializeFromFile` (未来) | 从独立 `.mat` 文件读取材质 | 资产系统 / 编辑器 |

> **设计要点**：`Serialize` 和 `Deserialize` 方法接收的是 YAML 的 Emitter/Node，而非文件路径。这使得它们既可以内联到场景文件中使用，也可以被未来的 `SerializeToFile` / `DeserializeFromFile` 复用。

### 4.4 MaterialSerializer 序列化实现

以下所有辅助函数和实现代码均位于 `MaterialSerializer.cpp` 中。

#### 4.4.1 ShaderUniformType 转字符串

```cpp
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
```

#### 4.4.2 字符串转 ShaderUniformType

```cpp
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
```

#### 4.4.3 序列化材质属性值（内部辅助函数）

```cpp
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
                for (int row = 0; row < 3; row++)
                    out << mat[col][row];
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
                for (int row = 0; row < 4; row++)
                    out << mat[col][row];
            out << YAML::EndSeq;
            break;
        }
        case ShaderUniformType::Sampler2D:
        {
            // 纹理序列化为文件路径字符串，空纹理序列化为空字符串
            const Ref<Texture2D>& texture = std::get<Ref<Texture2D>>(prop.Value);
            std::string texturePath = "";
            if (texture)
            {
                texturePath = texture->GetPath();
            }
            out << YAML::Key << "Value" << YAML::Value << texturePath;
            break;
        }
        default:
            break;
    }
}
```

#### 4.4.4 MaterialSerializer::Serialize 实现

```cpp
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
```

### 4.5 MaterialSerializer 反序列化实现

#### 4.5.1 反序列化材质属性值（内部辅助函数）

```cpp
/// <summary>
/// 从 YAML 节点反序列化材质属性值，并设置到材质上
/// </summary>
/// <param name="material">目标材质</param>
/// <param name="propName">属性名</param>
/// <param name="type">属性类型</param>
/// <param name="valueNode">YAML Value 节点</param>
static void DeserializeMaterialPropertyValue(
    const Ref<Material>& material,
    const std::string& propName,
    ShaderUniformType type,
    const YAML::Node& valueNode)
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
                    for (int row = 0; row < 3; row++)
                        mat[col][row] = valueNode[i++].as<float>();
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
                    for (int row = 0; row < 4; row++)
                        mat[col][row] = valueNode[i++].as<float>();
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
                Ref<Texture2D> texture = Texture2D::Create(texturePath);
                material->SetTexture(propName, texture);
            }
            break;
        }
        default:
            break;
    }
}
```

#### 4.5.2 MaterialSerializer::Deserialize 实现

```cpp
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
        LF_CORE_WARN("Deserialize Material: Shader '{0}' not found in ShaderLibrary, using default material.", shaderName);
        return Renderer3D::GetDefaultMaterial();
    }

    Ref<Shader> shader = shaderLib->Get(shaderName);

    // 创建材质
    std::string materialName = "New Material";
    if (materialNode["Name"])
    {
        materialName = materialNode["Name"].as<std::string>();
    }

    Ref<Material> material = CreateRef<Material>(materialName, shader);

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
```

#### 4.5.3 MaterialSerializer.cpp 需要的 #include

```cpp
#include "lcpch.h"
#include "MaterialSerializer.h"
#include "Renderer3D.h"      // 用于获取 ShaderLibrary 和 DefaultMaterial
#include "Texture.h"         // 用于 Texture2D::Create

#include <yaml-cpp/yaml.h>
```

> **关于 YAML::convert 模板特化**：`MaterialSerializer.cpp` 中需要使用 `glm::vec2/vec3/vec4` 的 YAML 序列化。这些 `YAML::convert` 模板特化和 `operator<<` 重载当前定义在 `SceneSerializer.cpp` 中。为了让 `MaterialSerializer.cpp` 也能使用，需要将这些通用的 YAML 工具代码提取到一个独立的头文件 `YamlHelpers.h` 中（见第七章 7.1 节）。

### 4.6 在 SceneSerializer 中集成调用

#### 4.6.1 SceneSerializer.cpp 新增 #include

```cpp
#include "Lucky/Renderer/MaterialSerializer.h"  // 材质序列化
#include "Lucky/Renderer/Renderer3D.h"          // 用于获取 DefaultMaterial
```

#### 4.6.2 SerializeEntity 中的 MeshRendererComponent 序列化

替换 `SerializeEntity` 函数中被注释的 `MeshRendererComponent` 部分：

```cpp
// MeshRenderer 组件
if (entity.HasComponent<MeshRendererComponent>())
{
    out << YAML::Key << "MeshRendererComponent";

    out << YAML::BeginMap;

    const auto& meshRendererComponent = entity.GetComponent<MeshRendererComponent>();

    // 序列化材质列表
    out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;

    for (const auto& material : meshRendererComponent.Materials)
    {
        if (material)
        {
            MaterialSerializer::Serialize(out, material);   // 调用 MaterialSerializer
        }
        else
        {
            // 空材质占位（保持索引对应关系）
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << "";
            out << YAML::Key << "Shader" << YAML::Value << "";
            out << YAML::Key << "Properties" << YAML::Value << YAML::BeginSeq << YAML::EndSeq;
            out << YAML::EndMap;
        }
    }

    out << YAML::EndSeq;    // 材质列表结束

    out << YAML::EndMap;
}
```

#### 4.6.3 Deserialize 中的 MeshRendererComponent 反序列化

替换反序列化中 `MeshRendererComponent` 的 TODO 部分：

```cpp
// MeshRenderer 组件
YAML::Node meshRendererComponentNode = entity["MeshRendererComponent"];
if (meshRendererComponentNode)
{
    // 注意：如果 MeshFilterComponent 的反序列化触发了 OnComponentAdded<MeshRendererComponent>，
    // 此时实体已经有 MeshRendererComponent 了，直接 GetComponent 即可。
    // 如果没有（例如实体只有 MeshRendererComponent 没有 MeshFilterComponent），则需要 AddComponent。
    MeshRendererComponent* meshRendererComponent = nullptr;
    if (deserializedEntity.HasComponent<MeshRendererComponent>())
    {
        meshRendererComponent = &deserializedEntity.GetComponent<MeshRendererComponent>();
    }
    else
    {
        meshRendererComponent = &deserializedEntity.AddComponent<MeshRendererComponent>();
    }

    // 反序列化材质列表
    YAML::Node materialsNode = meshRendererComponentNode["Materials"];
    if (materialsNode && materialsNode.IsSequence())
    {
        meshRendererComponent->Materials.clear();
        meshRendererComponent->Materials.reserve(materialsNode.size());

        for (auto materialNode : materialsNode)
        {
            Ref<Material> material = MaterialSerializer::Deserialize(materialNode);  // 调用 MaterialSerializer

            if (!material)
            {
                // 反序列化失败，使用默认材质
                material = Renderer3D::GetDefaultMaterial();
            }

            meshRendererComponent->Materials.push_back(material);
        }
    }
}
```

> **注意**：`SceneSerializer.cpp` 中不再包含任何材质序列化的辅助函数（`ShaderUniformTypeToString`、`SerializeMaterialPropertyValue` 等），这些全部由 `MaterialSerializer` 内部管理。SceneSerializer 只需要调用 `MaterialSerializer::Serialize` 和 `MaterialSerializer::Deserialize` 两个静态方法。

---

## 五、MeshFilterComponent 序列化完善

### 5.1 当前问题

当前 `MeshFilterComponent` 的序列化/反序列化存在以下问题：

1. **序列化了冗余的 `Name` 字段**：`Name` 可以从 `PrimitiveType` 推导，不需要单独序列化
2. **反序列化只支持 `PrimitiveType::Cube`**：`MeshFilterComponent` 构造函数的 switch 只处理了 `Cube`
3. **未预留外部模型路径字段**：未来导入外部模型时，`PrimitiveType` 为 `None`，需要通过文件路径重建 Mesh

### 5.2 改进方案

#### 5.2.1 序列化格式调整

移除冗余的 `Name` 字段，仅保留 `PrimitiveType`：

```yaml
MeshFilterComponent:
  PrimitiveType: 1
```

> **说明**：`Name` 字段在当前阶段是冗余的，因为 Mesh 名称可以从 `PrimitiveType` 推导。未来引入模型导入后，会添加 `MeshPath` 字段。

#### 5.2.2 序列化代码修改

```cpp
// MeshFilter 组件
if (entity.HasComponent<MeshFilterComponent>())
{
    out << YAML::Key << "MeshFilterComponent";

    out << YAML::BeginMap;

    const auto& meshFilterComponent = entity.GetComponent<MeshFilterComponent>();

    out << YAML::Key << "PrimitiveType" << YAML::Value << (int)meshFilterComponent.Primitive;

    out << YAML::EndMap;
}
```

#### 5.2.3 反序列化代码（无需修改）

当前反序列化代码已经只使用 `PrimitiveType` 重建 Mesh，逻辑正确：

```cpp
// MeshFilter 组件
YAML::Node meshFilterComponentNode = entity["MeshFilterComponent"];
if (meshFilterComponentNode)
{
    PrimitiveType primitiveType = (PrimitiveType)meshFilterComponentNode["PrimitiveType"].as<int>();

    auto& meshFilterComponent = deserializedEntity.AddComponent<MeshFilterComponent>(primitiveType);
}
```

#### 5.2.4 MeshFilterComponent 构造函数完善（可选，当前阶段非必须）

当 `MeshFactory` 实现了更多图元类型后，需要同步更新 `MeshFilterComponent` 构造函数：

```cpp
MeshFilterComponent(PrimitiveType primitive)
    : Primitive(primitive)
{
    switch (primitive)
    {
        case PrimitiveType::Cube:
        {
            Mesh = MeshFactory::CreateCube();
            Mesh->SetName("Cube");
            break;
        }
        // 未来扩展：
        // case PrimitiveType::Plane:
        // {
        //     Mesh = MeshFactory::CreatePlane();
        //     Mesh->SetName("Plane");
        //     break;
        // }
        // case PrimitiveType::Sphere:
        // {
        //     Mesh = MeshFactory::CreateSphere();
        //     Mesh->SetName("Sphere");
        //     break;
        // }
        default:
            break;
    }
}
```

> **注意**：当前 `MeshFactory` 只实现了 `CreateCube()`，其他图元类型的工厂方法尚未实现。在实现新的图元类型时，需要同步更新此处的 switch 分支。

---

## 六、错误处理与健壮性增强

### 6.1 反序列化安全读取

#### 6.1.1 问题

当前反序列化代码直接调用 `node.as<T>()`，如果节点不存在或类型不匹配，yaml-cpp 会抛出异常，可能导致程序崩溃。

#### 6.1.2 解决方案：整体 try-catch

在 `Deserialize` 方法的实体遍历循环中添加 try-catch，确保单个实体的反序列化失败不会影响其他实体：

```cpp
for (auto entity : entitys)
{
    try
    {
        // ... 现有的反序列化逻辑 ...
    }
    catch (const YAML::Exception& e)
    {
        LF_CORE_ERROR("Failed to deserialize entity: {0}", e.what());
        continue;   // 跳过该实体，继续反序列化下一个
    }
}
```

#### 6.1.3 关键节点的存在性检查

对于非必须的组件节点，在读取前检查是否存在：

```cpp
// 示例：安全读取 float 值
float intensity = 1.0f;  // 默认值
if (directionalLightComponentNode["Intensity"])
{
    intensity = directionalLightComponentNode["Intensity"].as<float>();
}
directionalLight.Intensity = intensity;
```

> **注意**：当前代码中大部分节点读取已经有了 `if (node)` 的检查，但组件内部的字段读取缺少这种保护。建议在材质反序列化的新代码中全面使用这种模式（上面第四章的代码已经包含了这些检查）。

### 6.2 版本兼容策略

#### 6.2.1 序列化时写入版本号

在 `Serialize` 方法中，`Scene` 字段之后写入 `Version`：

```cpp
void SceneSerializer::Serialize(const std::string& filepath)
{
    YAML::Emitter out;

    out << YAML::BeginMap;

    out << YAML::Key << "Scene" << YAML::Value << m_Scene->GetName();
    out << YAML::Key << "Version" << YAML::Value << 1;  // 当前版本号

    out << YAML::Key << "Entitys" << YAML::Value << YAML::BeginSeq;
    // ... 实体序列化 ...
}
```

#### 6.2.2 反序列化时读取版本号

```cpp
bool SceneSerializer::Deserialize(const std::string& filepath)
{
    YAML::Node data = YAML::LoadFile(filepath);

    if (!data["Scene"])
    {
        return false;
    }

    std::string sceneName = data["Scene"].as<std::string>();
    m_Scene->SetName(sceneName);

    // 读取版本号（不存在则视为 Version 0：旧格式）
    int version = 0;
    if (data["Version"])
    {
        version = data["Version"].as<int>();
    }

    LF_CORE_TRACE("Deserializing scene '{0}' (version {1})", sceneName, version);

    // ... 后续反序列化逻辑 ...
}
```

> **当前阶段**：Version 0（旧格式）和 Version 1（新格式）的唯一区别是 Version 1 包含 `MeshRendererComponent` 的材质数据。反序列化逻辑不需要根据版本号走不同路径，因为新代码对 `MeshRendererComponent` 节点做了存在性检查——旧格式文件中没有该节点，自然会跳过。

---

## 七、完整代码修改清单

### 7.1 新增文件：MaterialSerializer.h

**文件路径**：`Lucky/Source/Lucky/Renderer/MaterialSerializer.h`

完整代码见 [4.3.1 头文件：MaterialSerializer.h](#431-头文件materialserializerh)。

### 7.2 新增文件：MaterialSerializer.cpp

**文件路径**：`Lucky/Source/Lucky/Renderer/MaterialSerializer.cpp`

完整文件结构：

```cpp
#include "lcpch.h"
#include "MaterialSerializer.h"
#include "Renderer3D.h"
#include "Texture.h"

#include <yaml-cpp/yaml.h>

namespace Lucky
{
    // ---- 内部辅助函数（static，仅本文件可见） ----

    // 4.4.1 ShaderUniformTypeToString          （完整代码见第四章）
    // 4.4.2 StringToShaderUniformType           （完整代码见第四章）
    // 4.4.3 SerializeMaterialPropertyValue      （完整代码见第四章）
    // 4.5.1 DeserializeMaterialPropertyValue    （完整代码见第四章）

    // ---- 公开接口实现 ----

    // 4.4.4 MaterialSerializer::Serialize       （完整代码见第四章）
    // 4.5.2 MaterialSerializer::Deserialize     （完整代码见第四章）
}
```

> **关于 YAML glm 类型支持**：`MaterialSerializer.cpp` 中的 `SerializeMaterialPropertyValue` 函数需要使用 `operator<<(YAML::Emitter&, const glm::vec2/vec3/vec4&)` 重载，以及 `DeserializeMaterialPropertyValue` 函数需要使用 `YAML::convert<glm::vec2/vec3/vec4>` 模板特化。这些通用的 YAML 工具代码当前定义在 `SceneSerializer.cpp` 中。
>
> **推荐做法**：将这些 YAML 类型转换代码提取到一个独立的头文件 `YamlHelpers.h` 中，供 `SceneSerializer.cpp` 和 `MaterialSerializer.cpp` 共同使用。具体做法见下方 7.3 节的修改 1。
>
> **备选做法**：如果暂时不想新增 `YamlHelpers.h`，也可以在 `MaterialSerializer.cpp` 中重复定义这些模板特化和重载（但不推荐，因为违反 DRY 原则）。

### 7.3 修改文件：SceneSerializer.cpp

以下是需要修改的所有位置，按代码顺序排列：

#### 修改 1：提取 YAML 类型转换代码（推荐）

将 `SceneSerializer.cpp` 中 `namespace YAML { ... }` 内的所有 `convert` 模板特化，以及 `namespace Lucky { ... }` 中的 `operator<<` 重载函数，提取到独立头文件：

**新增文件**：`Lucky/Source/Lucky/Scene/YamlHelpers.h`

```cpp
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <yaml-cpp/yaml.h>

#include "Lucky/Core/UUID.h"

// ---- YAML::convert 模板特化 ----

namespace YAML
{
    // convert<glm::vec2>  （从 SceneSerializer.cpp 中移出）
    // convert<glm::vec3>  （从 SceneSerializer.cpp 中移出）
    // convert<glm::vec4>  （从 SceneSerializer.cpp 中移出）
    // convert<glm::quat>  （从 SceneSerializer.cpp 中移出）
    // convert<Lucky::UUID>（从 SceneSerializer.cpp 中移出）
}

// ---- YAML::Emitter operator<< 重载 ----

namespace Lucky
{
    // operator<<(YAML::Emitter&, const glm::vec2&)  （从 SceneSerializer.cpp 中移出）
    // operator<<(YAML::Emitter&, const glm::vec3&)  （从 SceneSerializer.cpp 中移出）
    // operator<<(YAML::Emitter&, const glm::vec4&)  （从 SceneSerializer.cpp 中移出）
    // operator<<(YAML::Emitter&, const glm::quat&)  （从 SceneSerializer.cpp 中移出）
}
```

然后在 `SceneSerializer.cpp` 和 `MaterialSerializer.cpp` 中都 `#include "YamlHelpers.h"`（或对应的相对路径），并删除 `SceneSerializer.cpp` 中原来的这些代码。

> **注意**：如果选择暂不提取，可以跳过此步骤，但需要在 `MaterialSerializer.cpp` 中重复定义所需的 `operator<<` 重载和 `YAML::convert` 特化。

#### 修改 2：新增 #include

**位置**：`SceneSerializer.cpp` 文件顶部 include 区域

**新增**：

```cpp
#include "Lucky/Renderer/MaterialSerializer.h"  // 材质序列化
#include "Lucky/Renderer/Renderer3D.h"          // 用于获取 DefaultMaterial
```

#### 修改 3：修改 SerializeEntity 中的 MeshFilterComponent

**位置**：`SerializeEntity` 函数中 `MeshFilterComponent` 序列化部分

**修改前**：

```cpp
// MeshFilter 组件
if (entity.HasComponent<MeshFilterComponent>())
{
    out << YAML::Key << "MeshFilterComponent";
    
    out << YAML::BeginMap;
    
    const auto& meshFilterComponent = entity.GetComponent<MeshFilterComponent>();
    
    out << YAML::Key << "Name" << YAML::Value << meshFilterComponent.Mesh->GetName();
    out << YAML::Key << "PrimitiveType" << YAML::Value << (int)meshFilterComponent.Primitive;
    
    out << YAML::EndMap;
}
```

**修改后**：

```cpp
// MeshFilter 组件
if (entity.HasComponent<MeshFilterComponent>())
{
    out << YAML::Key << "MeshFilterComponent";
    
    out << YAML::BeginMap;
    
    const auto& meshFilterComponent = entity.GetComponent<MeshFilterComponent>();
    
    out << YAML::Key << "PrimitiveType" << YAML::Value << (int)meshFilterComponent.Primitive;
    
    out << YAML::EndMap;
}
```

#### 修改 4：修改 SerializeEntity 中的 MeshRendererComponent

**位置**：`SerializeEntity` 函数中 `MeshRendererComponent` 序列化部分

**修改前**：

```cpp
// MeshRenderer 组件
if (entity.HasComponent<MeshRendererComponent>())
{
    // out << YAML::Key << "MeshRendererComponent";
    //
    // out << YAML::BeginMap;
    //
    // const auto& meshRendererComponent = entity.GetComponent<MeshRendererComponent>();
    //
    // // TODO 序列化材质
    //
    // out << YAML::EndMap;
}
```

**修改后**（调用 `MaterialSerializer::Serialize`）：

```cpp
// MeshRenderer 组件
if (entity.HasComponent<MeshRendererComponent>())
{
    out << YAML::Key << "MeshRendererComponent";

    out << YAML::BeginMap;

    const auto& meshRendererComponent = entity.GetComponent<MeshRendererComponent>();

    // 序列化材质列表
    out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;

    for (const auto& material : meshRendererComponent.Materials)
    {
        if (material)
        {
            MaterialSerializer::Serialize(out, material);   // 调用 MaterialSerializer
        }
        else
        {
            // 空材质占位（保持索引对应关系）
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << "";
            out << YAML::Key << "Shader" << YAML::Value << "";
            out << YAML::Key << "Properties" << YAML::Value << YAML::BeginSeq << YAML::EndSeq;
            out << YAML::EndMap;
        }
    }

    out << YAML::EndSeq;

    out << YAML::EndMap;
}
```

#### 修改 5：Serialize 方法添加版本号

**位置**：`SceneSerializer::Serialize` 方法

**修改前**：

```cpp
void SceneSerializer::Serialize(const std::string& filepath)
{
    YAML::Emitter out;

    out << YAML::BeginMap;
    
    out << YAML::Key << "Scene" << YAML::Value << m_Scene->GetName();
    
    out << YAML::Key << "Entitys" << YAML::Value << YAML::BeginSeq;
```

**修改后**：

```cpp
void SceneSerializer::Serialize(const std::string& filepath)
{
    YAML::Emitter out;

    out << YAML::BeginMap;
    
    out << YAML::Key << "Scene" << YAML::Value << m_Scene->GetName();
    out << YAML::Key << "Version" << YAML::Value << 1;
    
    out << YAML::Key << "Entitys" << YAML::Value << YAML::BeginSeq;
```

#### 修改 6：Deserialize 方法添加版本号读取

**位置**：`SceneSerializer::Deserialize` 方法

**修改前**：

```cpp
std::string sceneName = data["Scene"].as<std::string>();
m_Scene->SetName(sceneName);

LF_CORE_TRACE("Deserializing scene '{0}'", sceneName);

YAML::Node entitys = data["Entitys"];
```

**修改后**：

```cpp
std::string sceneName = data["Scene"].as<std::string>();
m_Scene->SetName(sceneName);

// 读取版本号（不存在则视为旧格式 Version 0）
int version = 0;
if (data["Version"])
{
    version = data["Version"].as<int>();
}

LF_CORE_TRACE("Deserializing scene '{0}' (version {1})", sceneName, version);

YAML::Node entitys = data["Entitys"];
```

#### 修改 7：Deserialize 中 MeshRendererComponent 反序列化

**位置**：`SceneSerializer::Deserialize` 方法中 `MeshRendererComponent` 反序列化部分

**修改前**：

```cpp
// MeshRenderer 组件
YAML::Node meshRendererComponentNode = entity["MeshRendererComponent"];
if (meshRendererComponentNode)
{
    auto& meshRendererComponent = deserializedEntity.GetComponent<MeshRendererComponent>();
    
    // TODO 反序列化材质
}
```

**修改后**（调用 `MaterialSerializer::Deserialize`）：

```cpp
// MeshRenderer 组件
YAML::Node meshRendererComponentNode = entity["MeshRendererComponent"];
if (meshRendererComponentNode)
{
    // MeshFilterComponent 反序列化时可能已触发 OnComponentAdded<MeshRendererComponent>
    // 因此需要先检查组件是否已存在
    MeshRendererComponent* meshRendererComponent = nullptr;
    if (deserializedEntity.HasComponent<MeshRendererComponent>())
    {
        meshRendererComponent = &deserializedEntity.GetComponent<MeshRendererComponent>();
    }
    else
    {
        meshRendererComponent = &deserializedEntity.AddComponent<MeshRendererComponent>();
    }

    // 反序列化材质列表
    YAML::Node materialsNode = meshRendererComponentNode["Materials"];
    if (materialsNode && materialsNode.IsSequence())
    {
        meshRendererComponent->Materials.clear();
        meshRendererComponent->Materials.reserve(materialsNode.size());

        for (auto materialNode : materialsNode)
        {
            Ref<Material> material = MaterialSerializer::Deserialize(materialNode);  // 调用 MaterialSerializer

            if (!material)
            {
                // 反序列化失败，使用默认材质
                material = Renderer3D::GetDefaultMaterial();
            }

            meshRendererComponent->Materials.push_back(material);
        }
    }
}
```

#### 修改 8：反序列化实体循环添加 try-catch

**位置**：`SceneSerializer::Deserialize` 方法中实体遍历循环

**修改前**：

```cpp
for (auto entity : entitys)
{
    uint64_t uuid = entity["Entity"].as<uint64_t>();
    
    std::string entityName;
    // ... 后续反序列化逻辑 ...
}
```

**修改后**：

```cpp
for (auto entity : entitys)
{
    try
    {
        uint64_t uuid = entity["Entity"].as<uint64_t>();
        
        std::string entityName;
        // ... 后续反序列化逻辑（缩进增加一级） ...
    }
    catch (const YAML::Exception& e)
    {
        LF_CORE_ERROR("Failed to deserialize entity: {0}", e.what());
        continue;
    }
}
```

### 7.4 文件变更总览

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| **新增** | `Lucky/Source/Lucky/Renderer/MaterialSerializer.h` | 材质序列化器头文件（~40 行） |
| **新增** | `Lucky/Source/Lucky/Renderer/MaterialSerializer.cpp` | 材质序列化器实现（~200 行） |
| **新增**（推荐） | `Lucky/Source/Lucky/Scene/YamlHelpers.h` | YAML 通用类型转换工具（从 SceneSerializer.cpp 提取，~170 行） |
| **修改** | `Lucky/Source/Lucky/Scene/SceneSerializer.cpp` | 新增 #include、调用 MaterialSerializer、版本号、try-catch、MeshFilter 精简 |

> **依赖关系**：
> - `MaterialSerializer.cpp` → `Material.h`, `Renderer3D.h`, `Texture.h`, `YamlHelpers.h`
> - `SceneSerializer.cpp` → `MaterialSerializer.h`, `Renderer3D.h`, `YamlHelpers.h`
> - `MaterialSerializer` 不依赖 `Scene`、`Entity` 等任何场景概念

---

## 八、验证方法

### 8.1 基本序列化/反序列化验证

1. 在编辑器中创建一个包含 Cube 实体的场景
2. 在 Inspector 中修改材质属性（如 `u_DiffuseCoeff` 改为红色 `[1.0, 0.0, 0.0]`）
3. **File → Save As** 保存场景为 `.luck3d` 文件
4. 用文本编辑器打开 `.luck3d` 文件，验证：
   - 文件顶部有 `Version: 1`
   - `MeshRendererComponent` 节点存在
   - `Materials` 列表中包含正确的 Shader 名称和属性值
5. **File → New Scene** 创建空场景
6. **File → Open** 打开刚才保存的文件
7. 验证 Cube 的材质属性是否正确恢复（漫反射应为红色）

### 8.2 多材质验证

1. 创建一个 Cube 实体
2. 修改其材质的多个属性（颜色、光泽度等）
3. 保存并重新打开
4. 验证所有属性值是否正确恢复

### 8.3 纹理属性验证

1. 在材质中设置一个纹理（如 `u_MainTexture`）
2. 保存场景
3. 验证 `.luck3d` 文件中纹理路径正确
4. 重新打开场景，验证纹理是否正确加载

### 8.4 旧格式兼容性验证

1. 使用现有的 `TestScene.luck3d`（不含 `Version` 和 `MeshRendererComponent`）
2. 打开该文件，验证不会崩溃
3. 验证实体正确加载（材质使用默认材质）

### 8.5 错误处理验证

1. 手动编辑 `.luck3d` 文件，故意删除某个实体的 `TransformComponent` 节点
2. 打开该文件，验证：
   - 该实体仍然被创建（使用默认 Transform）
   - 其他实体正常加载
   - 控制台输出错误日志

---

## 九、后续扩展预留

### 9.1 独立材质文件（.mat）

当引入资产系统后，材质可以从内联格式迁移为独立文件引用：

```yaml
# 场景文件中的引用格式（未来）
MeshRendererComponent:
  Materials:
    - Path: "Assets/Materials/MyMaterial.mat"
    - Path: "Assets/Materials/Default.mat"
```

```yaml
# 独立的 .mat 文件格式（未来）
Material: My Material
Shader: Standard
Properties:
  - Name: "u_DiffuseCoeff"
    Type: Float3
    Value: [0.8, 0.2, 0.2]
  # ...
```

### 9.2 外部模型 Mesh 引用

当引入模型导入（P3-1）后，`MeshFilterComponent` 需要支持文件路径引用：

```yaml
MeshFilterComponent:
  PrimitiveType: 0          # None，表示外部模型
  MeshPath: "Assets/Models/character.fbx"
  SubMeshIndex: 0            # 可选：指定模型中的哪个子网格
```

### 9.3 场景文件版本迁移

当 `Version` 递增时，可以在 `Deserialize` 中添加版本迁移逻辑：

```cpp
// 版本迁移示例（未来）
if (version == 0)
{
    // Version 0 → 1：旧格式没有 MeshRendererComponent，使用默认材质
    // 当前代码已经通过节点存在性检查自动处理
}

if (version <= 1)
{
    // Version 1 → 2：例如 TransformComponent 格式变更
    // 在此处添加迁移逻辑
}
```

---

## 附录 A：完整的序列化/反序列化流程图

```
序列化流程：
┌──────────────────────────────────────────────────────────────────────┐
│ SceneSerializer::Serialize(filepath)                                 │
│   ├─ 写入 Scene 名称                                                │
│   ├─ 写入 Version: 1                                                │
│   └─ 遍历所有实体 → SerializeEntity(out, entity)                    │
│       ├─ Entity UUID                                                │
│       ├─ NameComponent → Name                                       │
│       ├─ TransformComponent → Position/Rotation(quat)/Scale         │
│       ├─ RelationshipComponent → Parent/Children                    │
│       ├─ DirectionalLightComponent → Direction/Color/Intensity      │
│       ├─ MeshFilterComponent → PrimitiveType                        │
│       └─ MeshRendererComponent → Materials[]                        │
│           └─ 每个 Material:                                         │
│               └─ MaterialSerializer::Serialize(out, material) ──┐   │
│                   ├─ Name                                       │   │
│                   ├─ Shader（名称引用）                           │   │
│                   └─ Properties[]                               │   │
│                       └─ 每个 Property: Name + Type + Value     │   │
│                                                                 │   │
│                   （MaterialSerializer.cpp 内部完成）         ?──┘   │
└──────────────────────────────────────────────────────────────────────┘

反序列化流程：
┌──────────────────────────────────────────────────────────────────────┐
│ SceneSerializer::Deserialize(filepath)                               │
│   ├─ 读取 Scene 名称                                                │
│   ├─ 读取 Version（默认 0）                                          │
│   └─ 遍历所有实体节点（带 try-catch）                                 │
│       ├─ 读取 UUID → CreateEntity(uuid, name)                       │
│       ├─ NameComponent → 设置 Name                                  │
│       ├─ TransformComponent → 设置 Position/Rotation/Scale          │
│       ├─ RelationshipComponent → 设置 Parent/Children               │
│       ├─ DirectionalLightComponent → AddComponent + 设置属性         │
│       ├─ MeshFilterComponent → AddComponent(PrimitiveType)          │
│       │   └─ 触发 OnComponentAdded<MeshRendererComponent>           │
│       │       └─ 自动创建默认材质                                    │
│       └─ MeshRendererComponent                                      │
│           ├─ HasComponent 检查（可能已由上一步自动添加）               │
│           └─ 遍历 Materials 节点                                     │
│               └─ MaterialSerializer::Deserialize(materialNode) ──┐  │
│                   ├─ 从 ShaderLibrary 获取 Shader                │  │
│                   ├─ 创建 Material(name, shader)                 │  │
│                   └─ 遍历 Properties 设置属性值                   │  │
│                       └─ DeserializeMaterialPropertyValue()      │  │
│                                                                  │  │
│                   （MaterialSerializer.cpp 内部完成）          ?──┘  │
└──────────────────────────────────────────────────────────────────────┘
```

## 附录 B：关键类关系图

```
┌──────────────────┐     ┌──────────────────┐
│  SceneSerializer │────→│      Scene       │
│                  │     │  m_Registry      │
│  Serialize()     │     │  m_Name          │
│  Deserialize()   │     └──────────────────┘
└──────────────────┘              │
         │                        │ 包含多个
         │                        ▼
         │               ┌──────────────────┐
         │               │     Entity       │
         │               │  IDComponent     │
         │               │  NameComponent   │
         │               │  TransformComp.  │
         │               │  RelationshipC.  │
         │               │  MeshFilterComp. │──→ Mesh (PrimitiveType 重建)
         │               │  MeshRendererC.  │──→ Materials[]
         │               │  DirLightComp.   │         │
         │               └──────────────────┘         │
         │                                            ▼
         │  调用                              ┌──────────────────┐
         ├────────────────────────────────────→│    Material      │
         │                                    │  m_Name          │
         │  ┌────────────────────────┐        │  m_Shader ───────┼──→ ShaderLibrary.Get(name)
         │  │  MaterialSerializer   │        │  m_PropertyMap   │
         ├─→│  (Renderer 模块)       │───────→│  m_PropertyOrder │
         │  │                        │  读/写  └──────────────────┘
         │  │  Serialize(out, mat)   │                 │
         │  │  Deserialize(node)     │                 │ 包含多个
         │  │                        │                 ▼
         │  │  // 未来扩展：          │        ┌──────────────────┐
         │  │  SerializeToFile()     │        │ MaterialProperty │
         │  │  DeserializeFromFile() │        │  Name            │
         │  └────────────────────────┘        │  Type            │
         │           │                        │  Value (variant) │
         │           │ 内部引用                └──────────────────┘
         │           ▼
         │  ┌──────────────────┐
         └─→│   Renderer3D     │
            │ GetShaderLibrary │──→ 反序列化时通过 Shader 名称查找
            │ GetDefaultMat.   │──→ 反序列化失败时的 fallback
            └──────────────────┘

  ┌────────────────────────────────────────────────────────────────┐
  │                      YamlHelpers.h                            │
  │  YAML::convert<glm::vec2/vec3/vec4/quat/UUID>                │
  │  operator<<(YAML::Emitter&, glm::vec2/vec3/vec4/quat)        │
  │                                                              │
  │  被 SceneSerializer.cpp 和 MaterialSerializer.cpp 共同引用    │
  └────────────────────────────────────────────────────────────────┘
```
