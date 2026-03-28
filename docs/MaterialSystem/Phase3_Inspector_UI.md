# Phase 3：Inspector UI 绘制

## 1. 概述

Phase 3 的目标是在编辑器的 Inspector 面板中绘制 `MeshRendererComponent` 的 UI，使用户能够：
1. 查看当前实体的所有材质
2. 查看每个材质当前使用的 Shader
3. 编辑每个材质的 uniform 参数（根据参数类型显示对应的 UI 控件）

## 2. 涉及的文件

### 需要修改的文件
| 文件路径 | 说明 |
|---------|------|
| `Luck3DApp/Source/Panels/InspectorPanel.h` | 可能需要添加辅助方法声明 |
| `Luck3DApp/Source/Panels/InspectorPanel.cpp` | 添加 MeshRendererComponent 的绘制逻辑 |

---

## 3. MeshRendererComponent 的 Inspector 绘制

### 3.1 整体结构

在 `InspectorPanel::DrawComponents()` 方法中，添加 `MeshRendererComponent` 的绘制：

```cpp
#include "Lucky/Scene/Components/MeshRendererComponent.h"

// 在 DrawComponents() 方法末尾添加

// MeshRenderer 组件
DrawComponent<MeshRendererComponent>("Mesh Renderer", entity, [](MeshRendererComponent& meshRenderer)
{
    // 显示材质数量
    ImGui::Text("Materials: %d", (int)meshRenderer.Materials.size());
    ImGui::Separator();

    // 遍历每个材质
    for (size_t i = 0; i < meshRenderer.Materials.size(); i++)
    {
        auto& material = meshRenderer.Materials[i];

        // 材质折叠面板
        ImGui::PushID((int)i);

        std::string materialLabel = material
            ? (material->GetName() + " [" + std::to_string(i) + "]")
            : ("Empty Material [" + std::to_string(i) + "]");

        if (ImGui::TreeNodeEx(materialLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (material)
            {
                DrawMaterialProperties(material);  // 绘制材质属性
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No material assigned");
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }
});
```

### 3.2 DrawMaterialProperties 方法

#### 方案 A：作为 InspectorPanel 的私有方法（推荐 ???）

在 `InspectorPanel` 类中添加一个静态/私有方法专门绘制材质属性。

**优点**：
- 代码组织清晰
- 可以在多处复用

**缺点**：
- 需要修改头文件

#### 方案 B：作为 lambda 内的局部函数

直接在 `DrawComponent` 的 lambda 中实现所有逻辑。

**优点**：
- 不需要修改头文件

**缺点**：
- lambda 会很长，可读性差

#### 方案 C：作为独立的自由函数

在 `InspectorPanel.cpp` 中定义一个 `static` 自由函数。

**优点**：
- 不需要修改头文件
- 代码组织合理

**缺点**：
- 不属于类的成员，无法访问类的私有成员（但这里不需要）

**推荐方案 C**，最简洁且不需要修改头文件。

---

## 4. 材质属性 UI 绘制

### 4.1 Shader 显示

```cpp
/// <summary>
/// 绘制材质属性 UI
/// </summary>
/// <param name="material">材质</param>
static void DrawMaterialProperties(const Ref<Material>& material)
{
    // ---- 1. 显示 Shader 名称 ----
    std::string shaderName = material->GetShader() ? material->GetShader()->GetName() : "None";
    ImGui::Text("Shader: %s", shaderName.c_str());

    // TODO Phase 4: Shader 下拉选择

    ImGui::Separator();

    // ---- 2. 遍历材质属性并绘制 UI ----
    auto& properties = material->GetProperties();

    for (auto& prop : properties)
    {
        ImGui::PushID(prop.Name.c_str());

        switch (prop.Type)
        {
            case ShaderDataType::Float:
                DrawFloatProperty(prop);
                break;
            case ShaderDataType::Float2:
                DrawFloat2Property(prop);
                break;
            case ShaderDataType::Float3:
                DrawFloat3Property(prop);
                break;
            case ShaderDataType::Float4:
                DrawFloat4Property(prop);
                break;
            case ShaderDataType::Int:
                DrawIntProperty(prop);
                break;
            case ShaderDataType::Mat3:
                // Mat3 通常不在 Inspector 中编辑
                ImGui::Text("%s: [Mat3]", prop.Name.c_str());
                break;
            case ShaderDataType::Mat4:
                // Mat4 通常不在 Inspector 中编辑
                ImGui::Text("%s: [Mat4]", prop.Name.c_str());
                break;
            case ShaderDataType::Sampler2D:
                DrawTextureProperty(prop);
                break;
            default:
                ImGui::Text("%s: [Unknown Type]", prop.Name.c_str());
                break;
        }

        ImGui::PopID();
    }
}
```

### 4.2 各类型属性的 UI 绘制函数

#### Float 属性

```cpp
static void DrawFloatProperty(MaterialProperty& prop)
{
    float value = std::get<float>(prop.Value);
    if (ImGui::DragFloat(prop.Name.c_str(), &value, 0.01f))
    {
        prop.Value = value;
    }
}
```

#### Float2 属性

```cpp
static void DrawFloat2Property(MaterialProperty& prop)
{
    glm::vec2 value = std::get<glm::vec2>(prop.Value);
    if (ImGui::DragFloat2(prop.Name.c_str(), &value.x, 0.01f))
    {
        prop.Value = value;
    }
}
```

#### Float3 属性

Float3 比较特殊，可能是颜色也可能是方向/位置。

##### 方案 A：统一使用 DragFloat3（推荐 ?? 简单方案）

```cpp
static void DrawFloat3Property(MaterialProperty& prop)
{
    glm::vec3 value = std::get<glm::vec3>(prop.Value);
    if (ImGui::DragFloat3(prop.Name.c_str(), &value.x, 0.01f))
    {
        prop.Value = value;
    }
}
```

**优点**：简单统一
**缺点**：颜色属性没有颜色选择器

##### 方案 B：根据命名约定自动选择控件（推荐 ??? 最优方案）

根据 uniform 名称中的关键词自动判断是否为颜色属性：

```cpp
/// <summary>
/// 判断属性名是否表示颜色
/// </summary>
static bool IsColorProperty(const std::string& name)
{
    // 转小写后检查是否包含颜色相关关键词
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    return lower.find("color") != std::string::npos
        || lower.find("colour") != std::string::npos
        || lower.find("tint") != std::string::npos
        || lower.find("albedo") != std::string::npos;
}

static void DrawFloat3Property(MaterialProperty& prop)
{
    glm::vec3 value = std::get<glm::vec3>(prop.Value);

    if (IsColorProperty(prop.Name))
    {
        // 颜色属性：使用颜色编辑器
        if (ImGui::ColorEdit3(prop.Name.c_str(), &value.x))
        {
            prop.Value = value;
        }
    }
    else
    {
        // 非颜色属性：使用拖拽控件
        if (ImGui::DragFloat3(prop.Name.c_str(), &value.x, 0.01f))
        {
            prop.Value = value;
        }
    }
}
```

**优点**：
- 自动识别颜色属性，提供更好的编辑体验
- 不需要额外的元数据

**缺点**：
- 依赖命名约定，可能误判

##### 方案 C：在 MaterialProperty 中添加 UI 提示元数据

```cpp
struct MaterialProperty
{
    std::string Name;
    ShaderDataType Type;
    MaterialPropertyValue Value;

    // UI 提示
    enum class UIHint { Default, Color, Slider, Range };
    UIHint Hint = UIHint::Default;
    float MinValue = 0.0f;
    float MaxValue = 1.0f;
};
```

**优点**：最灵活，完全可控
**缺点**：增加了 MaterialProperty 的复杂度，需要手动设置

**推荐方案 B**，在当前阶段提供最好的用户体验且实现简单。

#### Float4 属性

```cpp
static void DrawFloat4Property(MaterialProperty& prop)
{
    glm::vec4 value = std::get<glm::vec4>(prop.Value);

    if (IsColorProperty(prop.Name))
    {
        if (ImGui::ColorEdit4(prop.Name.c_str(), &value.x))
        {
            prop.Value = value;
        }
    }
    else
    {
        if (ImGui::DragFloat4(prop.Name.c_str(), &value.x, 0.01f))
        {
            prop.Value = value;
        }
    }
}
```

#### Int 属性

```cpp
static void DrawIntProperty(MaterialProperty& prop)
{
    int value = std::get<int>(prop.Value);
    if (ImGui::DragInt(prop.Name.c_str(), &value))
    {
        prop.Value = value;
    }
}
```

#### Sampler2D（纹理）属性

##### 方案 A：仅显示纹理信息（推荐 ??? 当前阶段）

```cpp
static void DrawTextureProperty(MaterialProperty& prop)
{
    const auto& texture = std::get<Ref<Texture2D>>(prop.Value);

    ImGui::Text("%s:", prop.Name.c_str());
    ImGui::SameLine();

    if (texture)
    {
        ImGui::Text("[%dx%d] %s", texture->GetWidth(), texture->GetHeight(), texture->GetPath().c_str());

        // 显示纹理预览（小缩略图）
        ImGui::Image(
            (ImTextureID)(uint64_t)texture->GetRendererID(),
            ImVec2(64, 64),
            ImVec2(0, 1),   // UV 翻转（OpenGL 纹理坐标 Y 轴朝上）
            ImVec2(1, 0)
        );
    }
    else
    {
        ImGui::Text("[None]");
    }

    // TODO: 纹理拖拽/文件选择
}
```

**优点**：简单，能显示基本信息
**缺点**：不能更换纹理

##### 方案 B：支持文件拖拽加载纹理

```cpp
static void DrawTextureProperty(MaterialProperty& prop)
{
    const auto& texture = std::get<Ref<Texture2D>>(prop.Value);

    ImGui::Text("%s:", prop.Name.c_str());

    // 纹理预览区域
    ImVec2 previewSize(64, 64);
    if (texture)
    {
        ImGui::Image(
            (ImTextureID)(uint64_t)texture->GetRendererID(),
            previewSize,
            ImVec2(0, 1), ImVec2(1, 0)
        );
    }
    else
    {
        // 空纹理占位符
        ImGui::Button("Drop Texture", previewSize);
    }

    // 拖拽目标：接受文件拖拽
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
        {
            const char* path = (const char*)payload->Data;
            auto newTexture = Texture2D::Create(std::string(path));
            prop.Value = newTexture;
        }
        ImGui::EndDragDropTarget();
    }
}
```

**优点**：支持拖拽更换纹理
**缺点**：需要实现资产拖拽系统（可能超出当前范围）

**推荐方案 A**（当前阶段），先实现基本显示，拖拽功能后续添加。

---

## 5. 完整的 DrawMaterialProperties 实现

```cpp
// InspectorPanel.cpp 顶部添加
#include "Lucky/Scene/Components/MeshRendererComponent.h"
#include <algorithm>  // for std::transform

// ==================== 材质属性 UI 辅助函数 ====================

/// <summary>
/// 判断属性名是否表示颜色
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

/// <summary>
/// 绘制 Float 属性
/// </summary>
static void DrawFloatProperty(MaterialProperty& prop)
{
    float value = std::get<float>(prop.Value);
    if (ImGui::DragFloat(prop.Name.c_str(), &value, 0.01f))
    {
        prop.Value = value;
    }
}

/// <summary>
/// 绘制 Float2 属性
/// </summary>
static void DrawFloat2Property(MaterialProperty& prop)
{
    glm::vec2 value = std::get<glm::vec2>(prop.Value);
    if (ImGui::DragFloat2(prop.Name.c_str(), &value.x, 0.01f))
    {
        prop.Value = value;
    }
}

/// <summary>
/// 绘制 Float3 属性（自动识别颜色属性）
/// </summary>
static void DrawFloat3Property(MaterialProperty& prop)
{
    glm::vec3 value = std::get<glm::vec3>(prop.Value);

    if (IsColorProperty(prop.Name))
    {
        if (ImGui::ColorEdit3(prop.Name.c_str(), &value.x))
        {
            prop.Value = value;
        }
    }
    else
    {
        if (ImGui::DragFloat3(prop.Name.c_str(), &value.x, 0.01f))
        {
            prop.Value = value;
        }
    }
}

/// <summary>
/// 绘制 Float4 属性（自动识别颜色属性）
/// </summary>
static void DrawFloat4Property(MaterialProperty& prop)
{
    glm::vec4 value = std::get<glm::vec4>(prop.Value);

    if (IsColorProperty(prop.Name))
    {
        if (ImGui::ColorEdit4(prop.Name.c_str(), &value.x))
        {
            prop.Value = value;
        }
    }
    else
    {
        if (ImGui::DragFloat4(prop.Name.c_str(), &value.x, 0.01f))
        {
            prop.Value = value;
        }
    }
}

/// <summary>
/// 绘制 Int 属性
/// </summary>
static void DrawIntProperty(MaterialProperty& prop)
{
    int value = std::get<int>(prop.Value);
    if (ImGui::DragInt(prop.Name.c_str(), &value))
    {
        prop.Value = value;
    }
}

/// <summary>
/// 绘制纹理属性
/// </summary>
static void DrawTextureProperty(MaterialProperty& prop)
{
    const auto& texture = std::get<Ref<Texture2D>>(prop.Value);

    ImGui::Text("%s:", prop.Name.c_str());
    ImGui::SameLine();

    if (texture)
    {
        ImGui::Text("[%dx%d]", texture->GetWidth(), texture->GetHeight());

        // 纹理预览缩略图
        ImGui::Image(
            (ImTextureID)(uint64_t)texture->GetRendererID(),
            ImVec2(64, 64),
            ImVec2(0, 1),
            ImVec2(1, 0)
        );
    }
    else
    {
        ImGui::Text("[None]");
    }
}

/// <summary>
/// 绘制材质的所有属性
/// </summary>
static void DrawMaterialProperties(const Ref<Material>& material)
{
    // Shader 名称
    std::string shaderName = material->GetShader() ? material->GetShader()->GetName() : "None";
    ImGui::Text("Shader: %s", shaderName.c_str());

    // TODO Phase 4: Shader 下拉选择

    ImGui::Separator();

    // 遍历材质属性
    auto& properties = material->GetProperties();

    if (properties.empty())
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No editable properties");
        return;
    }

    for (auto& prop : properties)
    {
        ImGui::PushID(prop.Name.c_str());

        switch (prop.Type)
        {
            case ShaderDataType::Float:     DrawFloatProperty(prop);    break;
            case ShaderDataType::Float2:    DrawFloat2Property(prop);   break;
            case ShaderDataType::Float3:    DrawFloat3Property(prop);   break;
            case ShaderDataType::Float4:    DrawFloat4Property(prop);   break;
            case ShaderDataType::Int:       DrawIntProperty(prop);      break;
            case ShaderDataType::Mat3:
                ImGui::Text("%s: [Mat3] (not editable)", prop.Name.c_str());
                break;
            case ShaderDataType::Mat4:
                ImGui::Text("%s: [Mat4] (not editable)", prop.Name.c_str());
                break;
            case ShaderDataType::Sampler2D: DrawTextureProperty(prop);  break;
            default:
                ImGui::Text("%s: [Unknown]", prop.Name.c_str());
                break;
        }

        ImGui::PopID();
    }
}
```

---

## 6. DrawComponents 中添加 MeshRendererComponent

在 `InspectorPanel::DrawComponents()` 方法的末尾（MeshFilter 组件之后）添加：

```cpp
// MeshRenderer 组件
DrawComponent<MeshRendererComponent>("Mesh Renderer", entity, [](MeshRendererComponent& meshRenderer)
{
    // 材质数量
    int materialCount = (int)meshRenderer.Materials.size();
    ImGui::Text("Materials: %d", materialCount);

    ImGui::Separator();

    // 遍历每个材质
    for (size_t i = 0; i < meshRenderer.Materials.size(); i++)
    {
        auto& material = meshRenderer.Materials[i];

        ImGui::PushID((int)i);

        // 材质标题
        std::string label = material
            ? ("Material " + std::to_string(i) + ": " + material->GetName())
            : ("Material " + std::to_string(i) + ": [Empty]");

        bool opened = ImGui::TreeNodeEx(
            (void*)(intptr_t)i,
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth,
            "%s", label.c_str()
        );

        if (opened)
        {
            if (material)
            {
                DrawMaterialProperties(material);
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No material assigned");
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    // 如果没有材质，显示提示
    if (meshRenderer.Materials.empty())
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No materials");
    }
});
```

---

## 7. 属性显示名称美化

### 7.1 问题

Shader 中的 uniform 名称如 `u_DiffuseCoeff` 直接显示在 Inspector 中不够友好。

### 7.2 美化方案

#### 方案 A：去掉前缀并添加空格（推荐 ???）

```cpp
/// <summary>
/// 将 uniform 名称转换为友好的显示名称
/// 例如：u_DiffuseCoeff -> Diffuse Coeff
///       u_LightDirection -> Light Direction
/// </summary>
static std::string GetDisplayName(const std::string& uniformName)
{
    std::string name = uniformName;

    // 去掉 "u_" 前缀
    if (name.size() > 2 && name[0] == 'u' && name[1] == '_')
    {
        name = name.substr(2);
    }

    // 在大写字母前插入空格（驼峰转空格分隔）
    std::string result;
    for (size_t i = 0; i < name.size(); i++)
    {
        if (i > 0 && std::isupper(name[i]) && !std::isupper(name[i - 1]))
        {
            result += ' ';
        }
        result += name[i];
    }

    return result;
}
```

使用方式：在绘制属性时，用 `GetDisplayName(prop.Name)` 替代 `prop.Name` 作为 ImGui 控件的标签。

**注意**：`prop.Name` 仍然保持原始的 uniform 名称（用于 `SetFloat` 等方法），只是 UI 显示时使用美化后的名称。

```cpp
static void DrawFloatProperty(MaterialProperty& prop)
{
    float value = std::get<float>(prop.Value);
    std::string displayName = GetDisplayName(prop.Name);
    if (ImGui::DragFloat(displayName.c_str(), &value, 0.01f))
    {
        prop.Value = value;
    }
}
```

**优点**：自动美化，不需要额外配置
**缺点**：可能不适用于所有命名风格

#### 方案 B：在 MaterialProperty 中存储显示名称

```cpp
struct MaterialProperty
{
    std::string Name;           // 原始 uniform 名称
    std::string DisplayName;    // 显示名称
    // ...
};
```

**优点**：完全可控
**缺点**：需要手动设置或在 RebuildProperties 时自动生成

**推荐方案 A**，简单有效。

---

## 8. 完整的文件修改清单

### `Luck3DApp/Source/Panels/InspectorPanel.cpp`
- 添加 `#include "Lucky/Scene/Components/MeshRendererComponent.h"`
- 添加 `#include <algorithm>`
- 添加辅助函数：`IsColorProperty()`, `GetDisplayName()`
- 添加属性绘制函数：`DrawFloatProperty()`, `DrawFloat2Property()`, `DrawFloat3Property()`, `DrawFloat4Property()`, `DrawIntProperty()`, `DrawTextureProperty()`
- 添加 `DrawMaterialProperties()` 函数
- 在 `DrawComponents()` 中添加 `MeshRendererComponent` 的 `DrawComponent` 调用

### `Luck3DApp/Source/Panels/InspectorPanel.h`（可选）
- 如果选择方案 A（自由函数），不需要修改头文件
- 如果选择方案 A（私有方法），需要添加 `DrawMaterialProperties` 声明

---

## 9. UI 布局参考

最终的 Inspector 面板布局应类似：

```
┌─────────────────────────────────────┐
│ [Name Input: "Cube"]                │
├─────────────────────────────────────┤
│ ▼ Transform                     [+] │
│   Position  [0.0] [0.0] [0.0]      │
│   Rotation  [0.0] [0.0] [0.0]      │
│   Scale     [1.0] [1.0] [1.0]      │
├─────────────────────────────────────┤
│ ▼ Cube (Mesh Filter)            [+] │
│   Mesh  [Cube]                      │
├─────────────────────────────────────┤
│ ▼ Mesh Renderer                 [+] │
│   Materials: 1                      │
│   ─────────────────────────         │
│   ▼ Material 0: TextureShader Mat   │
│     Shader: TextureShader           │
│     ─────────────────────────       │
│     Texture Index    [1]            │
│     Light Direction  [-0.8][-1.0].. │
│     Light Color      [■■■■■■■■■■]  │
│     Light Intensity  [1.0]          │
│     Ambient Coeff    [-0.1][-0.1].. │
│     Diffuse Coeff    [-0.8][-0.8].. │
│     Specular Coeff   [-0.5][-0.5].. │
│     Shininess        [32.0]         │
└─────────────────────────────────────┘
```

---

## 10. 验证方法

1. **基本显示**：选中一个有 `MeshRendererComponent` 的实体，Inspector 应显示 "Mesh Renderer" 折叠面板
2. **材质属性显示**：展开材质，应显示 Shader 名称和所有可编辑的 uniform 参数
3. **实时编辑**：拖动 `u_DiffuseCoeff` 的值，场景中的物体颜色应实时变化
4. **颜色编辑器**：`u_LightColor` 等包含 "Color" 的属性应显示颜色选择器
5. **多材质**：如果 Mesh 有多个 SubMesh，应显示多个材质面板

---

## 11. 注意事项

1. **ImGui ID 冲突**：使用 `ImGui::PushID` / `ImGui::PopID` 确保每个材质和每个属性有唯一的 ID，避免 UI 交互冲突

2. **属性值的实时性**：由于 `MaterialProperty::Value` 是直接引用（通过 `GetProperties()` 返回引用），在 ImGui 中修改值后，下一帧渲染时 `Material::Apply()` 会自动使用新值，无需额外的同步机制

3. **性能**：每帧都会遍历所有属性并绘制 UI，对于少量属性（< 20 个）不会有性能问题

4. **`std::get` 异常**：如果 `variant` 中存储的类型与 `std::get<T>` 的类型不匹配，会抛出 `std::bad_variant_access` 异常。由于我们根据 `prop.Type` 来选择 `std::get` 的类型，只要 `Type` 和 `Value` 保持一致就不会出问题。但建议在 Debug 模式下添加断言检查。
