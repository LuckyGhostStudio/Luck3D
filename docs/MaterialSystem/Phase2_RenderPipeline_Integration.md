# Phase 2：渲染流程改造（DrawMesh 使用材质）

## 1. 概述

Phase 2 的目标是将 Phase 1 创建的 `Material` 类和 `MeshRendererComponent` 集成到渲染流程中，使 `Renderer3D::DrawMesh` 能够根据每个 SubMesh 的 `MaterialIndex` 使用对应的材质进行渲染。

## 2. 涉及的文件

### 需要修改的文件
| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Renderer/Renderer3D.h` | 修改 `DrawMesh` 接口签名 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | 重写 `DrawMesh` 实现，集成材质系统 |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | 修改 `OnUpdate` 渲染循环，使用 `MeshRendererComponent` |

---

## 3. Renderer3D 接口改造

### 3.1 DrawMesh 接口设计

#### 方案 A：新增带材质参数的重载（推荐 ???）

保留旧的 `DrawMesh` 接口（向后兼容），新增一个接受材质列表的重载。

```cpp
// Renderer3D.h
class Renderer3D
{
public:
    // ... 其他方法不变 ...

    /// <summary>
    /// 绘制网格（使用默认材质/Shader）
    /// </summary>
    static void DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh);

    /// <summary>
    /// 绘制网格（使用指定材质列表）
    /// 材质列表的索引对应 SubMesh 的 MaterialIndex
    /// </summary>
    /// <param name="transform">模型变换矩阵</param>
    /// <param name="mesh">网格</param>
    /// <param name="materials">材质列表</param>
    static void DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials);
};
```

**优点**：
- 向后兼容，不影响现有代码
- 无材质时使用默认渲染路径
- 渐进式迁移

**缺点**：
- 两个重载可能导致代码重复

##### 旧接口的处理

旧的 `DrawMesh(transform, mesh)` 可以内部调用新接口，传入空材质列表：

```cpp
void Renderer3D::DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh)
{
    static std::vector<Ref<Material>> emptyMaterials;
    DrawMesh(transform, mesh, emptyMaterials);
}
```

#### 方案 B：直接修改现有接口

将现有 `DrawMesh` 的签名改为接受材质列表，所有调用处都需要修改。

```cpp
static void DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials);
```

**优点**：
- 接口统一，无冗余

**缺点**：
- 破坏性修改，需要修改所有调用处
- 没有材质时也需要传空列表

#### 方案 C：使用默认参数

```cpp
static void DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials = {});
```

**优点**：
- 一个接口，向后兼容

**缺点**：
- 默认参数在头文件中，每次调用都会构造空 vector（虽然编译器可能优化）
- 头文件需要包含 `Material.h`

**推荐方案 A**（重载），最干净的向后兼容方式。

---

### 3.2 Renderer3DData 修改

需要在 `Renderer3DData` 中添加默认材质：

```cpp
struct Renderer3DData
{
    // ... 现有成员不变 ...

    Ref<Material> DefaultMaterial;  // 默认材质（当 SubMesh 没有对应材质时使用）
};
```

---

## 4. DrawMesh 实现

### 4.1 核心渲染逻辑

```cpp
// Renderer3D.cpp

#include "Material.h"  // 新增

void Renderer3D::DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials)
{
    // ---- 1. 准备顶点数据（与原逻辑相同） ----
    s_Data.MeshVertexBufferData.clear();
    for (const Vertex& vertex : mesh->GetVertices())
    {
        Vertex v = vertex;
        s_Data.MeshVertexBufferData.push_back(v);
    }

    uint32_t dataSize = sizeof(Vertex) * static_cast<uint32_t>(s_Data.MeshVertexBufferData.size());
    mesh->SetVertexBufferData(s_Data.MeshVertexBufferData.data(), dataSize);

    // ---- 2. 遍历每个 SubMesh 进行绘制 ----
    for (uint32_t i = 0; i < mesh->GetSubMeshCount(); ++i)
    {
        SubMesh sm = mesh->GetSubMesh(i);

        // 获取该 SubMesh 对应的材质
        Ref<Material> material = nullptr;
        if (sm.MaterialIndex < materials.size())
        {
            material = materials[sm.MaterialIndex];
        }

        if (material && material->GetShader())
        {
            // ---- 使用材质渲染 ----

            // 设置引擎内部 uniform（变换矩阵）
            material->GetShader()->Bind();
            material->GetShader()->SetMat4("u_ObjectToWorldMatrix", transform);

            // 应用材质属性（上传所有材质参数到 GPU）
            material->Apply();
        }
        else
        {
            // ---- 使用默认 Shader 渲染（回退路径） ----
            s_Data.MeshShader->Bind();
            s_Data.MeshShader->SetMat4("u_ObjectToWorldMatrix", transform);

            // 使用默认材质参数（与原 BeginScene 中设置的一致）
            s_Data.MeshShader->SetInt("u_TextureIndex", 1);
            s_Data.TextureSlots[1]->Bind(1);
        }

        RenderCommand::DrawIndexed(mesh->GetVertexArray(), sm.IndexOffset, sm.IndexCount);

        s_Data.Stats.DrawCalls++;
        s_Data.Stats.TriangleCount += sm.IndexCount / 3;
    }
}
```

### 4.2 关于 Apply() 和引擎 Uniform 的设置顺序

#### 方案 A：先 Bind + 设置引擎 Uniform，再 Apply（推荐 ???）

```cpp
material->GetShader()->Bind();
material->GetShader()->SetMat4("u_ObjectToWorldMatrix", transform);
material->Apply();  // Apply 内部不再调用 Bind()
```

这要求 `Material::Apply()` 内部**不调用** `m_Shader->Bind()`，只上传属性值。

**优点**：
- 引擎 uniform 和材质 uniform 的设置逻辑分离清晰
- `Apply()` 职责单一：只上传材质属性

**缺点**：
- 调用者需要手动 Bind Shader

#### 方案 B：Apply 内部完成所有操作

```cpp
material->Apply(transform);  // Apply 内部 Bind + 设置引擎 uniform + 上传材质属性
```

**优点**：
- 调用简洁

**缺点**：
- `Material` 需要知道引擎内部 uniform 的设置逻辑，职责不清
- `Apply` 的参数会越来越多（transform, lightData, ...）

#### 方案 C：Apply 只负责 Bind + 上传材质属性，引擎 uniform 在 Apply 之后设置

```cpp
material->Apply();  // Bind + 上传材质属性
material->GetShader()->SetMat4("u_ObjectToWorldMatrix", transform);  // 引擎 uniform
```

**优点**：
- `Apply()` 封装完整（Bind + 上传）
- 引擎 uniform 由渲染器负责

**缺点**：
- 引擎 uniform 会覆盖材质中同名属性（如果有的话），但由于我们已经过滤了引擎 uniform，不会有冲突

**推荐方案 A**，职责最清晰。需要修改 Phase 1 中 `Material::Apply()` 的实现，去掉内部的 `m_Shader->Bind()` 调用。

> **重要**：如果选择方案 A，需要回到 Phase 1 修改 `Material::Apply()` 的实现，将 `m_Shader->Bind()` 移除，或者将 Apply 拆分为 `Bind()` + `UploadProperties()`。

### 4.3 修改后的 Material::Apply()

根据方案 A，`Apply()` 只上传材质属性，不 Bind Shader：

```cpp
void Material::Apply() const
{
    if (!m_Shader) return;

    // 注意：不在这里调用 m_Shader->Bind()
    // Shader 的绑定由渲染器（Renderer3D）负责

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
```

---

## 5. Scene::OnUpdate 修改

### 5.1 渲染循环改造

#### 方案 A：同时查询 MeshFilterComponent 和 MeshRendererComponent（推荐 ???）

```cpp
void Scene::OnUpdate(DeltaTime dt, EditorCamera& camera)
{
    Renderer3D::BeginScene(camera);
    {
        // 查询同时拥有 TransformComponent、MeshFilterComponent、MeshRendererComponent 的实体
        auto meshGroup = m_Registry.group<TransformComponent>(entt::get<MeshFilterComponent, MeshRendererComponent>);

        for (auto entity : meshGroup)
        {
            auto [transform, meshFilter, meshRenderer] = meshGroup.get<TransformComponent, MeshFilterComponent, MeshRendererComponent>(entity);

            Renderer3D::DrawMesh(transform.GetTransform(), meshFilter.Mesh, meshRenderer.Materials);
        }

        // 兼容：只有 MeshFilter 没有 MeshRenderer 的实体，使用默认渲染
        auto meshOnlyView = m_Registry.view<TransformComponent, MeshFilterComponent>(entt::exclude<MeshRendererComponent>);
        for (auto entity : meshOnlyView)
        {
            auto [transform, meshFilter] = meshOnlyView.get<TransformComponent, MeshFilterComponent>(entity);

            Renderer3D::DrawMesh(transform.GetTransform(), meshFilter.Mesh);
        }
    }
    Renderer3D::EndScene();
}
```

**优点**：
- 向后兼容：没有 `MeshRendererComponent` 的实体仍然可以渲染
- 渐进式迁移：可以逐步为实体添加 `MeshRendererComponent`

**缺点**：
- 两次查询，代码稍多

#### 方案 B：强制要求 MeshRendererComponent

```cpp
void Scene::OnUpdate(DeltaTime dt, EditorCamera& camera)
{
    Renderer3D::BeginScene(camera);
    {
        auto meshGroup = m_Registry.group<TransformComponent>(entt::get<MeshFilterComponent, MeshRendererComponent>);

        for (auto entity : meshGroup)
        {
            auto [transform, meshFilter, meshRenderer] = meshGroup.get<TransformComponent, MeshFilterComponent, MeshRendererComponent>(entity);

            Renderer3D::DrawMesh(transform.GetTransform(), meshFilter.Mesh, meshRenderer.Materials);
        }
    }
    Renderer3D::EndScene();
}
```

**优点**：
- 代码简洁
- 强制统一的组件模型

**缺点**：
- 破坏性修改：现有没有 `MeshRendererComponent` 的实体将不再渲染
- 需要确保所有创建实体的地方都添加了 `MeshRendererComponent`

#### 方案 C：在 MeshFilter 中内嵌默认材质

如果实体没有 `MeshRendererComponent`，在 `DrawMesh` 内部使用全局默认材质。

**推荐方案 A**，兼容性最好，适合渐进式迁移。

---

### 5.2 头文件包含

在 `Scene.cpp` 中需要新增：

```cpp
#include "Components/MeshRendererComponent.h"
```

---

## 6. BeginScene 的光照 Uniform 处理

### 6.1 问题

当前 `BeginScene` 中直接通过 `s_Data.MeshShader` 设置光照 uniform：

```cpp
s_Data.MeshShader->SetFloat3("u_LightDirection", glm::vec3(-0.8f, -1.0f, -0.5f));
s_Data.MeshShader->SetFloat3("u_LightColor", glm::vec3(1.0f, 1.0f, 1.0f));
// ...
```

引入材质系统后，每个材质可能使用不同的 Shader，光照 uniform 需要设置到每个 Shader 上。

### 6.2 处理方案

#### 方案 A：光照参数作为材质属性（推荐 ??? 当前阶段）

光照相关的 uniform（`u_LightDirection`, `u_LightColor`, `u_LightIntensity`）作为材质属性暴露给用户。每个材质独立设置光照参数。

**做法**：
- 从 `BeginScene` 中移除光照 uniform 的设置
- 光照参数由 `Material::Apply()` 上传（因为它们是 Shader 的 active uniform，会被内省到材质属性中）
- 创建材质时，为光照参数设置默认值

**优点**：
- 实现简单，不需要额外的光照系统
- 每个材质可以有不同的光照参数（虽然不太合理，但作为初步方案可以接受）

**缺点**：
- 光照参数不应该属于材质，语义不正确
- 多个物体使用不同材质时，光照参数可能不一致

#### 方案 B：光照参数使用 UBO

将光照参数移到 Uniform Buffer Object 中，类似相机数据的处理方式。

```glsl
layout(std140, binding = 1) uniform LightData
{
    vec3 u_LightDirection;
    vec3 u_LightColor;
    float u_LightIntensity;
};
```

**优点**：
- 语义正确，光照是全局数据
- 所有 Shader 共享同一份光照数据
- 不需要每个材质单独设置

**缺点**：
- 需要修改着色器代码
- 需要创建新的 UBO
- 增加了 Phase 2 的工作量

#### 方案 C：在 DrawMesh 中为每个材质的 Shader 设置光照 Uniform

```cpp
// 在 DrawMesh 中
material->GetShader()->Bind();
material->GetShader()->SetFloat3("u_LightDirection", s_Data.LightDirection);
material->GetShader()->SetFloat3("u_LightColor", s_Data.LightColor);
material->GetShader()->SetFloat("u_LightIntensity", s_Data.LightIntensity);
material->GetShader()->SetMat4("u_ObjectToWorldMatrix", transform);
material->Apply();
```

**优点**：
- 不需要修改着色器
- 光照数据集中管理

**缺点**：
- 每次 DrawCall 都要设置光照 uniform，有性能开销
- 光照参数需要存储在 `Renderer3DData` 中

**推荐方案 A**（当前阶段），最简单。光照参数暂时作为材质属性，后续 Phase 或独立任务中再迁移到 UBO（方案 B）。

如果选择方案 A，需要：
1. 从 `BeginScene` 中移除光照 uniform 的设置代码
2. 从 `BeginScene` 中移除材质默认值的设置代码
3. 确保 `IsEngineInternalUniform()` 中**不包含**光照和材质相关的 uniform（让它们作为材质属性暴露）

---

## 7. 默认材质

### 7.1 创建默认材质

在 `Renderer3D::Init()` 中创建默认材质：

```cpp
void Renderer3D::Init()
{
    // ... 现有初始化代码 ...

    s_Data.MeshShader = CreateRef<Shader>("Assets/Shaders/TextureShader");

    // 创建默认材质
    s_Data.DefaultMaterial = Material::Create(s_Data.MeshShader);
    s_Data.DefaultMaterial->SetName("Default Material");

    // 设置默认材质参数
    s_Data.DefaultMaterial->SetFloat3("u_LightDirection", glm::vec3(-0.8f, -1.0f, -0.5f));
    s_Data.DefaultMaterial->SetFloat3("u_LightColor", glm::vec3(1.0f, 1.0f, 1.0f));
    s_Data.DefaultMaterial->SetFloat("u_LightIntensity", 1.0f);
    s_Data.DefaultMaterial->SetFloat3("u_AmbientCoeff", glm::vec3(0.1f));
    s_Data.DefaultMaterial->SetFloat3("u_DiffuseCoeff", glm::vec3(0.8f));
    s_Data.DefaultMaterial->SetFloat3("u_SpecularCoeff", glm::vec3(0.5f));
    s_Data.DefaultMaterial->SetFloat("u_Shininess", 32.0f);
    s_Data.DefaultMaterial->SetInt("u_TextureIndex", 1);

    // ... 其他初始化 ...
}
```

### 7.2 回退逻辑

在 `DrawMesh` 中，当材质为空时使用默认材质：

```cpp
Ref<Material> material = nullptr;
if (sm.MaterialIndex < materials.size())
{
    material = materials[sm.MaterialIndex];
}

// 回退到默认材质
if (!material || !material->GetShader())
{
    material = s_Data.DefaultMaterial;
}

// 使用材质渲染
material->GetShader()->Bind();
material->GetShader()->SetMat4("u_ObjectToWorldMatrix", transform);
material->Apply();
```

---

## 8. BeginScene 修改

### 8.1 修改后的 BeginScene

```cpp
void Renderer3D::BeginScene(const EditorCamera& camera)
{
    // 设置相机 UBO（不变）
    s_Data.CameraBuffer.ViewProjectionMatrix = camera.GetViewProjectionMatrix();
    s_Data.CameraBuffer.Position = camera.GetPosition();
    s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));

    // 移除：不再在这里设置光照和材质 uniform
    // 这些参数现在由 Material::Apply() 负责上传
}
```

---

## 9. 完整的修改后 DrawMesh

```cpp
void Renderer3D::DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials)
{
    // 准备顶点数据
    s_Data.MeshVertexBufferData.clear();
    for (const Vertex& vertex : mesh->GetVertices())
    {
        Vertex v = vertex;
        s_Data.MeshVertexBufferData.push_back(v);
    }

    uint32_t dataSize = sizeof(Vertex) * static_cast<uint32_t>(s_Data.MeshVertexBufferData.size());
    mesh->SetVertexBufferData(s_Data.MeshVertexBufferData.data(), dataSize);

    // 绘制每个子网格
    for (uint32_t i = 0; i < mesh->GetSubMeshCount(); ++i)
    {
        SubMesh sm = mesh->GetSubMesh(i);

        // 获取该 SubMesh 对应的材质
        Ref<Material> material = nullptr;
        if (sm.MaterialIndex < materials.size())
        {
            material = materials[sm.MaterialIndex];
        }

        // 回退到默认材质
        if (!material || !material->GetShader())
        {
            material = s_Data.DefaultMaterial;
        }

        // 绑定 Shader
        material->GetShader()->Bind();

        // 设置引擎内部 uniform
        material->GetShader()->SetMat4("u_ObjectToWorldMatrix", transform);

        // 绑定纹理（默认白色纹理到 0 号槽位）
        s_Data.WhiteTexture->Bind(0);

        // 应用材质属性
        material->Apply();

        // 绘制
        RenderCommand::DrawIndexed(mesh->GetVertexArray(), sm.IndexOffset, sm.IndexCount);

        s_Data.Stats.DrawCalls++;
        s_Data.Stats.TriangleCount += sm.IndexCount / 3;
    }
}

// 旧接口兼容
void Renderer3D::DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh)
{
    static std::vector<Ref<Material>> emptyMaterials;
    DrawMesh(transform, mesh, emptyMaterials);
}
```

---

## 10. 完整的文件修改清单

### `Lucky/Source/Lucky/Renderer/Renderer3D.h`
- 添加 `#include "Material.h"`
- 新增 `DrawMesh` 重载（带材质列表参数）

### `Lucky/Source/Lucky/Renderer/Renderer3D.cpp`
- 添加 `#include "Material.h"`
- 在 `Renderer3DData` 中添加 `DefaultMaterial` 成员
- 修改 `Init()`：创建默认材质并设置默认参数
- 修改 `BeginScene()`：移除光照和材质 uniform 的设置
- 重写 `DrawMesh()`：集成材质系统
- 添加旧 `DrawMesh` 的兼容实现

### `Lucky/Source/Lucky/Scene/Scene.cpp`
- 添加 `#include "Components/MeshRendererComponent.h"`
- 修改 `OnUpdate()`：查询 `MeshRendererComponent` 并传递材质列表

---

## 11. 验证方法

### 11.1 基本渲染验证

1. 不添加 `MeshRendererComponent` 的实体应该使用默认材质正常渲染（与修改前效果一致）
2. 添加 `MeshRendererComponent` 但材质列表为空的实体应该使用默认材质渲染

### 11.2 材质渲染验证

在创建实体时手动添加材质：

```cpp
// 测试代码（可以放在 EditorLayer 或 Scene 初始化中）
Entity testEntity = m_Scene->CreateEntity("Test");
testEntity.AddComponent<MeshFilterComponent>(someMesh);

auto& meshRenderer = testEntity.AddComponent<MeshRendererComponent>();
auto material = Material::Create(shader);
material->SetFloat3("u_DiffuseCoeff", glm::vec3(1.0f, 0.0f, 0.0f));  // 红色漫反射
material->SetFloat3("u_AmbientCoeff", glm::vec3(0.1f));
material->SetFloat3("u_SpecularCoeff", glm::vec3(0.5f));
material->SetFloat("u_Shininess", 32.0f);
material->SetFloat3("u_LightDirection", glm::vec3(-0.8f, -1.0f, -0.5f));
material->SetFloat3("u_LightColor", glm::vec3(1.0f, 1.0f, 1.0f));
material->SetFloat("u_LightIntensity", 1.0f);
material->SetInt("u_TextureIndex", 0);  // 使用白色纹理
meshRenderer.SetMaterial(0, material);
```

预期效果：该实体应该显示为红色（漫反射系数为红色）。

### 11.3 多材质验证

如果 Mesh 有多个 SubMesh，为不同 SubMesh 设置不同颜色的材质，验证每个 SubMesh 使用正确的材质渲染。

---

## 12. 注意事项

1. **Shader 绑定状态**：OpenGL 是状态机，`glUseProgram` 设置的 Shader 在下次调用前一直有效。确保每个 SubMesh 渲染前都正确绑定了对应材质的 Shader。

2. **纹理槽位管理**：当前 `Apply()` 中纹理从槽位 1 开始分配。如果多个 SubMesh 使用不同材质，每次 `Apply()` 都会重新从 1 开始分配，这是正确的（因为每次 DrawCall 是独立的）。

3. **性能考虑**：每个 SubMesh 都会调用一次 `Apply()`，上传所有 uniform。如果相邻 SubMesh 使用相同材质，可以考虑跳过重复的 `Apply()` 调用（优化，非必须）。

4. **默认材质的纹理**：默认材质使用 `u_TextureIndex = 1`，需要确保 `s_Data.TextureSlots[1]` 已绑定（在 `Init()` 中已设置）。或者改为使用 `u_TextureIndex = 0`（白色纹理）。
