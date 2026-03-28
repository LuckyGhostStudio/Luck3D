# Phase 4：Shader 切换 + 属性重建 + 默认材质完善

## 1. 概述

Phase 4 是材质系统的最后一个阶段，目标是：
1. 在 Inspector 中实现 Shader 下拉选择，用户可以为材质切换不同的 Shader
2. 切换 Shader 后自动重建材质属性列表（保留同名同类型的旧值）
3. 完善默认材质机制，确保所有渲染路径都有可靠的回退
4. 材质数量与 SubMesh 数量的自动同步

## 2. 涉及的文件

### 需要修改的文件
| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Renderer/Shader.h` | ShaderLibrary 添加遍历接口 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.h` | 暴露 ShaderLibrary 和默认材质的访问接口 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | 初始化 ShaderLibrary，注册所有可用 Shader |
| `Luck3DApp/Source/Panels/InspectorPanel.cpp` | 添加 Shader 下拉选择 UI |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | 添加材质数量同步逻辑 |

---

## 3. ShaderLibrary 遍历接口

### 3.1 问题

当前 `ShaderLibrary` 只有 `Add`, `Load`, `Get`, `Exists` 方法，没有遍历所有 Shader 的接口。Inspector 中的 Shader 下拉选择需要获取所有可用 Shader 的名称列表。

### 3.2 修改方案

在 `ShaderLibrary` 类中添加遍历接口：

```cpp
// Shader.h - ShaderLibrary 类中新增

/// <summary>
/// 获取所有着色器名称列表
/// </summary>
std::vector<std::string> GetAllShaderNames() const;

/// <summary>
/// 获取所有着色器
/// </summary>
const std::unordered_map<std::string, Ref<Shader>>& GetAllShaders() const { return m_Shaders; }
```

实现：

```cpp
// Shader.cpp
std::vector<std::string> ShaderLibrary::GetAllShaderNames() const
{
    std::vector<std::string> names;
    names.reserve(m_Shaders.size());
    for (const auto& [name, shader] : m_Shaders)
    {
        names.push_back(name);
    }
    return names;
}
```

---

## 4. Renderer3D 暴露 ShaderLibrary

### 4.1 方案分析

#### 方案 A：Renderer3D 持有全局 ShaderLibrary 并提供静态访问（推荐 ???）

```cpp
// Renderer3D.h
class Renderer3D
{
public:
    // ... 现有方法 ...

    /// <summary>
    /// 获取着色器库
    /// </summary>
    static ShaderLibrary& GetShaderLibrary();

    /// <summary>
    /// 获取默认材质
    /// </summary>
    static Ref<Material> GetDefaultMaterial();
};
```

```cpp
// Renderer3D.cpp - Renderer3DData 中添加
struct Renderer3DData
{
    // ... 现有成员 ...
    ShaderLibrary ShaderLib;        // 着色器库
    Ref<Material> DefaultMaterial;  // 默认材质
};

ShaderLibrary& Renderer3D::GetShaderLibrary()
{
    return s_Data.ShaderLib;
}

Ref<Material> Renderer3D::GetDefaultMaterial()
{
    return s_Data.DefaultMaterial;
}
```

**优点**：
- 集中管理，访问方便
- 与现有架构一致（Renderer3D 是静态类）

**缺点**：
- Renderer3D 职责增加

#### 方案 B：独立的全局 ShaderLibrary 单例

```cpp
class ShaderManager
{
public:
    static ShaderManager& Instance();
    ShaderLibrary& GetLibrary();
};
```

**优点**：职责分离
**缺点**：引入新的单例，增加复杂度

**推荐方案 A**，与现有架构一致。

---

## 5. Renderer3D::Init() 中注册 Shader

在 `Init()` 中将所有 Shader 加载到 ShaderLibrary：

```cpp
void Renderer3D::Init()
{
    // ... 现有初始化代码 ...

    // 加载 Shader 到着色器库
    s_Data.ShaderLib.Load("Assets/Shaders/TextureShader");
    // 后续添加更多 Shader 时在这里注册
    // s_Data.ShaderLib.Load("Assets/Shaders/UnlitShader");
    // s_Data.ShaderLib.Load("Assets/Shaders/PBRShader");

    // 获取默认 Shader
    s_Data.MeshShader = s_Data.ShaderLib.Get("TextureShader");

    // 创建默认材质
    s_Data.DefaultMaterial = Material::Create(s_Data.MeshShader);
    s_Data.DefaultMaterial->SetName("Default Material");
    // ... 设置默认材质参数 ...
}
```

---

## 6. Inspector 中的 Shader 下拉选择

### 6.1 实现方案

#### 方案 A：使用 ImGui::Combo（推荐 ???）

```cpp
// 在 DrawMaterialProperties() 中，替换原来的 Shader 显示代码

static void DrawShaderSelector(const Ref<Material>& material)
{
    // 获取所有可用 Shader 名称
    auto shaderNames = Renderer3D::GetShaderLibrary().GetAllShaderNames();

    // 当前 Shader 名称
    std::string currentShaderName = material->GetShader()
        ? material->GetShader()->GetName()
        : "None";

    // 查找当前 Shader 在列表中的索引
    int currentIndex = -1;
    for (int i = 0; i < (int)shaderNames.size(); i++)
    {
        if (shaderNames[i] == currentShaderName)
        {
            currentIndex = i;
            break;
        }
    }

    // 绘制下拉选择框
    if (ImGui::BeginCombo("Shader", currentShaderName.c_str()))
    {
        for (int i = 0; i < (int)shaderNames.size(); i++)
        {
            bool isSelected = (i == currentIndex);

            if (ImGui::Selectable(shaderNames[i].c_str(), isSelected))
            {
                // 用户选择了新的 Shader
                if (i != currentIndex)
                {
                    auto newShader = Renderer3D::GetShaderLibrary().Get(shaderNames[i]);
                    material->SetShader(newShader);  // 触发属性重建
                }
            }

            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }
}
```

**优点**：
- ImGui 原生控件，交互体验好
- 支持搜索（如果 Shader 数量多）

**缺点**：
- 每帧都要获取 Shader 名称列表（可以缓存优化）

#### 方案 B：使用 ImGui::ListBox

```cpp
if (ImGui::ListBox("Shader", &currentIndex, shaderNamesCStr.data(), (int)shaderNamesCStr.size()))
{
    // 选择变化
}
```

**优点**：一次显示所有选项
**缺点**：占用空间大，不适合 Inspector

**推荐方案 A**（Combo），更适合 Inspector 的紧凑布局。

### 6.2 完整的 DrawMaterialProperties 修改

将 Phase 3 中的 Shader 显示替换为下拉选择：

```cpp
static void DrawMaterialProperties(const Ref<Material>& material)
{
    // ---- Shader 下拉选择 ----
    DrawShaderSelector(material);

    ImGui::Separator();

    // ---- 材质属性 ----
    auto& properties = material->GetProperties();
    // ... 与 Phase 3 相同 ...
}
```

---

## 7. 属性重建（RebuildProperties）

### 7.1 核心逻辑

`Material::SetShader()` 调用 `RebuildProperties()`，该方法在 Phase 1 中已定义。这里补充一些细节和边界情况处理。

### 7.2 属性值保留策略

#### 方案 A：同名同类型保留（推荐 ???）

只有当旧属性和新属性的**名称和类型都相同**时，才保留旧值。

```cpp
void Material::RebuildProperties()
{
    auto oldProperties = m_Properties;
    m_Properties.clear();

    if (!m_Shader) return;

    for (const auto& uniform : m_Shader->GetUniforms())
    {
        if (IsEngineInternalUniform(uniform.Name))
            continue;

        MaterialProperty prop;
        prop.Name = uniform.Name;
        prop.Type = uniform.Type;
        prop.Value = GetDefaultValue(uniform.Type);

        // 同名同类型保留旧值
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
}
```

**优点**：安全，不会出现类型不匹配的问题
**缺点**：如果新 Shader 中同名 uniform 的类型变了，旧值会丢失

#### 方案 B：同名保留（尝试类型转换）

如果名称相同但类型不同，尝试进行类型转换（如 Float3 -> Float4）。

**优点**：更多情况下能保留旧值
**缺点**：类型转换逻辑复杂，可能产生意外结果

**推荐方案 A**，简单安全。

---

## 8. 材质数量与 SubMesh 数量同步

### 8.1 问题

当 `MeshFilterComponent` 的 Mesh 发生变化（SubMesh 数量改变）时，`MeshRendererComponent` 的材质列表大小可能不匹配。

### 8.2 同步方案

#### 方案 A：在 Scene::OnUpdate 中同步（推荐 ???）

每帧检查并同步材质数量：

```cpp
void Scene::OnUpdate(DeltaTime dt, EditorCamera& camera)
{
    // 同步材质数量
    auto syncView = m_Registry.view<MeshFilterComponent, MeshRendererComponent>();
    for (auto entity : syncView)
    {
        auto& meshFilter = syncView.get<MeshFilterComponent>(entity);
        auto& meshRenderer = syncView.get<MeshRendererComponent>(entity);

        if (meshFilter.Mesh)
        {
            // 计算 Mesh 中不同 MaterialIndex 的最大值 + 1
            uint32_t requiredMaterialCount = 0;
            for (const auto& sm : meshFilter.Mesh->GetSubMeshes())
            {
                requiredMaterialCount = std::max(requiredMaterialCount, sm.MaterialIndex + 1);
            }

            // 如果材质列表不够长，扩展并填充默认材质
            if (meshRenderer.Materials.size() < requiredMaterialCount)
            {
                meshRenderer.Materials.resize(requiredMaterialCount);
                for (auto& mat : meshRenderer.Materials)
                {
                    if (!mat)
                    {
                        mat = Renderer3D::GetDefaultMaterial();
                    }
                }
            }
        }
    }

    // ... 渲染逻辑 ...
}
```

**优点**：
- 自动同步，用户无需手动操作
- 确保渲染时材质列表总是足够长

**缺点**：
- 每帧都检查，有少量性能开销（但 view 遍历很快）

#### 方案 B：在 MeshFilter 变化时触发同步

通过观察者模式或回调，在 Mesh 变化时同步材质。

**优点**：只在变化时执行，性能更好
**缺点**：需要实现观察者/回调机制，增加复杂度

#### 方案 C：在 Inspector 中手动管理

在 Inspector 中提供 "+" / "-" 按钮，让用户手动添加/删除材质。

```cpp
// 在 MeshRenderer 的 DrawComponent 中
if (ImGui::Button("Add Material"))
{
    meshRenderer.Materials.push_back(Renderer3D::GetDefaultMaterial());
}
ImGui::SameLine();
if (ImGui::Button("Remove Material") && !meshRenderer.Materials.empty())
{
    meshRenderer.Materials.pop_back();
}
```

**优点**：用户完全控制
**缺点**：用户体验差，容易忘记同步

**推荐方案 A**（自动同步），用户体验最好。可以同时提供方案 C 的手动按钮作为补充。

---

## 9. 创建实体时自动添加默认材质

### 9.1 OnComponentAdded 回调

当添加 `MeshRendererComponent` 时，可以自动填充默认材质：

```cpp
template<>
void Scene::OnComponentAdded<MeshRendererComponent>(Entity entity, MeshRendererComponent& component)
{
    // 如果实体已有 MeshFilter，根据 SubMesh 数量初始化材质列表
    if (entity.HasComponent<MeshFilterComponent>())
    {
        auto& meshFilter = entity.GetComponent<MeshFilterComponent>();
        if (meshFilter.Mesh)
        {
            uint32_t requiredCount = 0;
            for (const auto& sm : meshFilter.Mesh->GetSubMeshes())
            {
                requiredCount = std::max(requiredCount, sm.MaterialIndex + 1);
            }

            component.Materials.resize(requiredCount);
            for (auto& mat : component.Materials)
            {
                if (!mat)
                {
                    mat = Renderer3D::GetDefaultMaterial();
                }
            }
        }
    }
}
```

---

## 10. 完整的文件修改清单

### `Lucky/Source/Lucky/Renderer/Shader.h`
- `ShaderLibrary` 添加 `GetAllShaderNames()` 方法
- `ShaderLibrary` 添加 `GetAllShaders()` 方法

### `Lucky/Source/Lucky/Renderer/Shader.cpp`
- 实现 `ShaderLibrary::GetAllShaderNames()`

### `Lucky/Source/Lucky/Renderer/Renderer3D.h`
- 添加 `GetShaderLibrary()` 静态方法
- 添加 `GetDefaultMaterial()` 静态方法

### `Lucky/Source/Lucky/Renderer/Renderer3D.cpp`
- `Renderer3DData` 添加 `ShaderLib` 和 `DefaultMaterial` 成员
- `Init()` 中加载 Shader 到 ShaderLibrary
- 实现 `GetShaderLibrary()` 和 `GetDefaultMaterial()`

### `Luck3DApp/Source/Panels/InspectorPanel.cpp`
- 添加 `DrawShaderSelector()` 函数
- 修改 `DrawMaterialProperties()` 使用 Shader 下拉选择

### `Lucky/Source/Lucky/Scene/Scene.cpp`
- `OnUpdate()` 中添加材质数量同步逻辑
- `OnComponentAdded<MeshRendererComponent>` 中添加默认材质初始化

---

## 11. 验证方法

### 11.1 Shader 切换验证

1. 在 Inspector 中选择一个材质
2. 点击 Shader 下拉框，应显示所有已注册的 Shader
3. 选择不同的 Shader，材质属性列表应自动更新
4. 切换回原来的 Shader，之前设置的属性值应该被保留

### 11.2 默认材质验证

1. 创建一个新实体，添加 MeshFilter 和 MeshRenderer
2. MeshRenderer 的材质列表应自动填充默认材质
3. 物体应使用默认材质正常渲染

### 11.3 材质同步验证

1. 修改 Mesh 的 SubMesh 数量
2. MeshRenderer 的材质列表应自动扩展
3. 新增的材质槽位应填充默认材质

---

## 12. 后续扩展方向

完成 Phase 4 后，材质系统的基础功能已经完整。以下是后续可以考虑的扩展：

1. **材质序列化**：将材质保存为 `.mat` 文件，支持场景保存/加载
2. **材质资产管理**：在 Content Browser 中管理材质资产
3. **光照系统分离**：将光照参数从材质中分离，使用 UBO 或 Light 组件管理
4. **材质实例化**：支持材质实例（共享 Shader 但有不同参数值）
5. **Shader 热重载**：修改 Shader 文件后自动重新编译和更新材质
6. **PBR 材质**：实现 PBR 工作流（Metallic/Roughness 或 Specular/Glossiness）
7. **材质预览**：在 Inspector 中显示材质球预览
