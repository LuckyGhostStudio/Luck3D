# Luck3D 引擎功能发展路线图

> **文档版本**：v1.0  
> **创建日期**：2026-04-01  
> **文档说明**：本文档整理了引擎在完成方向光组件（Phase 6）之后的功能发展方向，按优先级排序。每个功能模块包含必要性分析、改动范围、前置依赖和推荐方案。可根据本文档的优先级序列逐步实施。

---

## 目录

- [一、当前引擎架构概览](#一当前引擎架构概览)
- [二、已完成的功能阶段](#二已完成的功能阶段)
- [三、功能发展路线总览](#三功能发展路线总览)
- [四、阶段 1：基础完善](#四阶段-1基础完善)
  - [P1-1 材质属性 Map 改造（Phase 5）](#p1-1-材质属性-map-改造phase-5)
  - [P1-2 场景序列化（SceneSerializer）](#p1-2-场景序列化sceneserializer)
- [五、阶段 2：渲染改进](#五阶段-2渲染改进)
  - [P2-1 渲染排序 + 延迟提交（DrawCommand）](#p2-1-渲染排序--延迟提交drawcommand)
  - [P2-2 Gizmo 渲染系统](#p2-2-gizmo-渲染系统)
- [六、阶段 3：内容扩展](#六阶段-3内容扩展)
  - [P3-1 模型导入（AssetImporter）](#p3-1-模型导入assetimporter)
  - [P3-2 多光源支持](#p3-2-多光源支持)
- [七、远期展望（暂不实施）](#七远期展望暂不实施)
- [八、实施路线图时间线](#八实施路线图时间线)

---

## 一、当前引擎架构概览

### 渲染流程

```
SceneViewportPanel::OnUpdate
  → Framebuffer.Bind()
  → RenderCommand::Clear()
  → Scene::OnUpdate()
      → Renderer3D::BeginScene()     // 设置 Camera UBO (binding=0) + Light UBO (binding=1)
      → Renderer3D::DrawMesh() × N   // 遍历 SubMesh → 绑定 Shader → Apply 材质 → DrawIndexedRange
      → Renderer3D::EndScene()       // 当前为空实现
  → Framebuffer.Unbind()
```

### 架构特征

| 特征 | 当前状态 |
|------|---------|
| 渲染路径 | 单 Pass Forward 渲染 |
| 渲染器设计 | `Renderer3D` 全 static 方法 + 文件级 static 数据 |
| 物体排序 | 无排序，按 entt 注册顺序绘制 |
| 批处理 | 无，每个 SubMesh 一次 DrawCall |
| 离屏渲染 | 已有 Framebuffer（SceneViewportPanel 使用） |
| 光照 | 单方向光，通过 UBO 传递（Phase 6 将改为组件驱动） |
| 材质系统 | Shader 内省 + MaterialProperty 列表 + Inspector UI |
| 场景管理 | entt ECS，支持父子层级关系 |

### 已有的渲染模块

| 模块 | 文件 | 说明 |
|------|------|------|
| Renderer | `Renderer.h/cpp` | 顶层初始化入口，调用 RenderCommand::Init + Renderer3D::Init |
| Renderer3D | `Renderer3D.h/cpp` | 3D 渲染器，BeginScene/DrawMesh/EndScene |
| RenderCommand | `RenderCommand.h/cpp` | 最底层 OpenGL 调用封装 |
| Shader | `Shader.h/cpp` | 着色器加载、编译、Uniform 内省与上传 |
| Material | `Material.h/cpp` | 材质属性管理与 Apply |
| Framebuffer | `Framebuffer.h/cpp` | 帧缓冲区（颜色附件 + 深度附件） |
| UniformBuffer | `UniformBuffer.h/cpp` | UBO 封装 |
| Mesh | `Mesh.h/cpp` | 网格数据（顶点 + SubMesh） |
| MeshFactory | `MeshFactory.h/cpp` | 内置几何体工厂（目前仅 Cube） |
| Texture | `Texture.h/cpp` | 2D 纹理 |

---

## 二、已完成的功能阶段

| 阶段 | 名称 | 文档 | 状态 |
|------|------|------|------|
| ECS Phase 1 | 世界矩阵与层级更新 | `docs/ECS/Phase1_WorldMatrix_And_HierarchyUpdate.md` | ? 已完成 |
| ECS Phase 2 | 重新设置父节点与世界 Transform 访问 | `docs/ECS/Phase2_Reparent_And_WorldTransformAccess.md` | ? 已完成 |
| ECS Phase 3 | DirtyFlag 与 Transform 通知 | `docs/ECS/Phase3_DirtyFlag_And_TransformNotification.md` | ? 已完成 |
| Material Phase 1 | 材质与 Shader 内省 | `docs/MaterialSystem/Phase1_Material_And_ShaderIntrospection.md` | ? 已完成 |
| Material Phase 2 | 渲染管线集成 | `docs/MaterialSystem/Phase2_RenderPipeline_Integration.md` | ? 已完成 |
| Material Phase 3 | Inspector UI | `docs/MaterialSystem/Phase3_Inspector_UI.md` | ? 已完成 |
| Material Phase 4 | Shader 切换与默认材质 | `docs/MaterialSystem/Phase4_ShaderSwitch_And_DefaultMaterial.md` | ? 已完成 |
| Material Phase 5 | 材质属性 Map 重构 | `docs/MaterialSystem/Phase5_MaterialProperty_Map_Refactor.md` | ?? 已设计 |
| Material Phase 6 | 方向光组件 | `docs/MaterialSystem/Phase6_DirectionalLight_Component.md` | ?? 已设计 |

---

## 三、功能发展路线总览

```
阶段 1：基础完善                    阶段 2：渲染改进                阶段 3：内容扩展
┌──────────────────────┐      ┌──────────────────────┐      ┌──────────────────────┐
│ P1-1 材质属性 Map 改造 │      │ P2-1 渲染排序+延迟提交 │      │ P3-1 模型导入 Assimp  │
│     ?? 优先级：高      │      │    ?? 优先级：中高     │      │    ?? 优先级：中       │
│                      │      │                      │      │                      │
│ P1-2 场景序列化       │      │ P2-2 Gizmo 渲染系统   │      │ P3-2 多光源支持       │
│     ?? 优先级：高      │      │    ?? 优先级：中       │      │    ?? 优先级：低       │
└──────────────────────┘      └──────────────────────┘      └──────────────────────┘
         ↓                             ↓                            ↓
    无渲染管线改动                 轻量级渲染改造                 Shader 架构改造
```

| 编号 | 功能 | 优先级 | 涉及渲染管线改造 | 前置依赖 |
|------|------|--------|-----------------|---------|
| P1-1 | 材质属性 Map 改造 | ?? 高 | 否 | Phase 5 设计文档 |
| P1-2 | 场景序列化 | ?? 高 | 否 | P1-1（材质序列化依赖 Map 结构） |
| P2-1 | 渲染排序 + 延迟提交 | ?? 中高 | **是（轻量级）** | 无 |
| P2-2 | Gizmo 渲染系统 | ?? 中 | 否（独立绘制路径） | 无 |
| P3-1 | 模型导入 | ?? 中 | 否 | 无 |
| P3-2 | 多光源支持 | ?? 低 | **是（Shader 改造）** | P2-1（排序系统） |

---

## 四、阶段 1：基础完善

### P1-1 材质属性 Map 改造（Phase 5）

> **详细设计文档**：`docs/MaterialSystem/Phase5_MaterialProperty_Map_Refactor.md`

#### 必要性分析

| 维度 | 说明 |
|------|------|
| **当前问题** | 材质属性使用 `std::vector<MaterialProperty>` 存储，每次读写属性需要遍历查找，时间复杂度 O(n) |
| **改造收益** | 改为 `std::unordered_map` 后读写 O(1)，代码更清晰，为序列化做准备 |
| **改动范围** | `Material.h/cpp`、`InspectorPanel.cpp`（UI 遍历方式调整） |
| **风险** | 低。纯数据结构替换，不影响外部 API |

#### 核心改动

- `Material` 类内部存储从 `std::vector<MaterialProperty>` 改为 `std::unordered_map<std::string, MaterialProperty>`
- `FindProperty()` 方法简化为 map 查找
- `RebuildProperties()` 重建逻辑适配 map
- `Apply()` 遍历 map 上传 uniform
- Inspector UI 遍历方式从 vector 改为 map iterator

#### 为什么排在第一

- 改动量小，风险低
- 为后续场景序列化（P1-2）中的材质序列化提供更清晰的数据结构
- 消除当前代码中的遍历查找模式

---

### P1-2 场景序列化（SceneSerializer）

#### 必要性分析

| 维度 | 说明 |
|------|------|
| **当前问题** | 场景数据完全在代码中硬编码（`EditorLayer::OnAttach` 里手动创建 Cube），关闭程序后一切丢失 |
| **为什么紧迫** | 没有序列化，所有编辑器功能都是"一次性"的。用户在 Inspector 中调整的光照参数、材质属性、Transform 等全部无法保存 |
| **改动范围** | 新增 `SceneSerializer` 类 + 材质序列化 `.mat` 文件 |
| **风险** | 中。需要为每种组件定义序列化格式，但不影响现有渲染逻辑 |

#### 核心改动

##### 1. 新增文件

| 文件 | 说明 |
|------|------|
| `SceneSerializer.h/cpp` | 场景序列化/反序列化核心类 |
| `MaterialSerializer.h/cpp`（可选） | 材质序列化为 `.mat` 文件 |

##### 2. 序列化格式（推荐 YAML）

**选择 YAML 的原因**：
- 人类可读，方便调试和手动编辑
- 主流引擎（Unity `.unity`/`.prefab`、Godot `.tscn`）均使用类似的文本格式
- C++ 有成熟的 yaml-cpp 库

**场景文件结构示例**（`.scene`）：

```yaml
Scene: New Scene
Entities:
  - Entity: 12345678901234
    NameComponent:
      Name: Cube
    TransformComponent:
      Translation: [0.0, 0.0, 0.0]
      RotationEuler: [0.0, 0.0, 0.0]
      Scale: [1.0, 1.0, 1.0]
    RelationshipComponent:
      Parent: 0
      Children: []
    MeshFilterComponent:
      MeshType: Cube  # 内置几何体类型（后续支持外部模型路径）
    MeshRendererComponent:
      Materials:
        - Path: "Assets/Materials/Default.mat"
  - Entity: 12345678901235
    NameComponent:
      Name: Directional Light
    TransformComponent:
      Translation: [0.0, 3.0, 0.0]
      RotationEuler: [-0.7854, -0.4636, 0.0]
      Scale: [1.0, 1.0, 1.0]
    DirectionalLightComponent:
      Color: [1.0, 1.0, 1.0]
      Intensity: 1.0
```

**材质文件结构示例**（`.mat`）：

```yaml
Material: Default Material
Shader: Standard
Properties:
  u_AmbientCoeff:
    Type: Float3
    Value: [0.2, 0.2, 0.2]
  u_DiffuseCoeff:
    Type: Float3
    Value: [0.8, 0.8, 0.8]
  u_SpecularCoeff:
    Type: Float3
    Value: [0.5, 0.5, 0.5]
  u_Shininess:
    Type: Float
    Value: 32.0
  u_MainTexture:
    Type: Sampler2D
    Value: "Assets/Textures/default.png"
```

##### 3. 需要序列化的组件

| 组件 | 序列化字段 | 说明 |
|------|-----------|------|
| IDComponent | UUID | 实体唯一标识 |
| NameComponent | Name | 实体名称 |
| TransformComponent | Translation, RotationEuler, Scale | 变换数据（序列化欧拉角，反序列化时同步四元数） |
| RelationshipComponent | Parent UUID, Children UUIDs | 父子关系 |
| MeshFilterComponent | MeshType / MeshPath | 网格来源（内置类型或外部文件路径） |
| MeshRendererComponent | Material 路径列表 | 引用 `.mat` 文件 |
| DirectionalLightComponent | Color, Intensity | 光照参数（方向从 Transform 推导，不单独序列化） |

##### 4. 编辑器集成

- **File → Save Scene**（Ctrl+S）：序列化当前场景到 `.scene` 文件
- **File → Open Scene**（Ctrl+O）：反序列化 `.scene` 文件，重建场景
- **File → New Scene**（Ctrl+N）：创建空场景

##### 5. 第三方依赖

| 库 | 用途 | 集成方式 |
|----|------|---------|
| yaml-cpp | YAML 解析与生成 | 作为子模块或通过包管理器引入 |

#### 前置依赖

- **P1-1 材质属性 Map 改造**：材质序列化时，Map 结构比 vector 更容易按 key-value 格式输出

---

## 五、阶段 2：渲染改进

### P2-1 渲染排序 + 延迟提交（DrawCommand）

#### 必要性分析

| 维度 | 说明 |
|------|------|
| **当前问题** | 物体按 entt 注册顺序绘制，没有任何排序逻辑 |
| **影响** | 1. 透明物体渲染错误（需要从远到近排序）<br/>2. 相同 Shader/材质的物体没有聚合，频繁切换 Shader 状态导致性能浪费 |
| **改动范围** | 仅修改 `Renderer3D.cpp` 内部实现，外部 API 不变 |
| **风险** | 低。内部重构，不影响 Scene、Material 等外部模块 |

#### 当前问题详解

```
当前 DrawMesh 流程（立即绘制）：
  物体 A（Shader=Standard, Material=M1）→ 绑定 Standard → Apply M1 → Draw
  物体 B（Shader=Custom,   Material=M2）→ 绑定 Custom   → Apply M2 → Draw
  物体 C（Shader=Standard, Material=M1）→ 绑定 Standard → Apply M1 → Draw  ← 重复绑定！
  物体 D（Shader=Standard, Material=M3）→ 绑定 Standard → Apply M3 → Draw

问题：Shader Standard 被绑定了 3 次，其中 2 次是不必要的
```

#### 核心改动

##### 1. 新增 DrawCommand 结构

```cpp
/// 绘制命令：描述一次 DrawCall 所需的全部信息
struct DrawCommand
{
    glm::mat4 Transform;        // 模型变换矩阵
    Ref<Mesh> Mesh;             // 网格引用
    const SubMesh* SubMeshPtr;  // SubMesh 指针
    Ref<Material> Material;     // 材质
    uint64_t SortKey;           // 排序键
    float DistanceToCamera;     // 到相机的距离（用于透明排序）
};
```

##### 2. 排序键设计

```
SortKey（64 位）：
┌──────────┬──────────┬──────────────────┐
│ 1 bit    │ 16 bit   │ 16 bit           │
│ 不透明=0 │ ShaderID │ MaterialID       │
│ 透明=1   │          │                  │
└──────────┴──────────┴──────────────────┘

不透明物体：按 SortKey 升序（聚合相同 Shader/Material，减少状态切换）
透明物体：  按 DistanceToCamera 降序（从远到近绘制）
```

##### 3. 改造后的流程

```
BeginScene()
  → 设置 Camera/Light UBO
  → 清空 DrawCommand 列表

DrawMesh() × N
  → 不立即绘制，而是为每个 SubMesh 生成 DrawCommand 并加入列表

EndScene()
  → 分离不透明和透明命令
  → 不透明命令按 SortKey 升序排序
  → 透明命令按 DistanceToCamera 降序排序
  → 遍历排序后的列表，执行实际绘制
  → 清空 DrawCommand 列表
```

##### 4. 涉及文件

| 文件 | 改动 |
|------|------|
| `Renderer3D.cpp` | `Renderer3DData` 新增 `std::vector<DrawCommand>`；`DrawMesh` 改为收集命令；`EndScene` 实现排序 + 绘制 |
| `Renderer3D.h` | 外部 API 不变 |

#### 为什么是"轻量级"渲染改造

这个改动**不引入任何新的架构概念**（不需要 RenderPass、RenderPipeline、RenderGraph 等类），只是把 `DrawMesh` 内部从"立即执行"改为"收集 + 排序 + 批量执行"。外部调用方式完全不变。

#### 为未来扩展打下的基础

- **阴影映射**：后续只需在 `EndScene` 的排序绘制之前，插入一个 Shadow Pass（用深度 Shader 绘制一遍不透明物体到 Shadow Map）
- **后处理**：在 `EndScene` 的绘制完成后，可以添加后处理步骤
- **多 Pass**：DrawCommand 列表可以被多个 Pass 复用

---

### P2-2 Gizmo 渲染系统

#### 必要性分析

| 维度 | 说明 |
|------|------|
| **当前问题** | 编辑器中没有任何辅助可视化（网格线、灯光图标、坐标轴、选中高亮等） |
| **为什么需要** | 添加灯光组件后，用户在场景中看不到灯光的位置和方向，编辑体验差 |
| **改动范围** | 新增 Gizmo 渲染模块，在 `Scene::OnUpdate` 之后额外绘制 |
| **风险** | 低。独立的绘制路径，不影响主渲染流程 |

#### 核心功能

| 功能 | 优先级 | 说明 |
|------|--------|------|
| 方向光方向线/箭头 | ?? 高 | 显示灯光方向，让用户直观看到光照方向 |
| 选中实体包围盒 | ?? 高 | 线框高亮选中的物体 |
| 场景网格线（Grid） | ?? 中 | 地面参考网格，帮助判断空间位置 |
| Transform 操控手柄 | ?? 中 | 平移/旋转/缩放 Gizmo（可集成 ImGuizmo） |
| 相机视锥体线框 | ?? 低 | 可视化相机的视锥体范围 |

#### 实现方案

##### 方案 A：独立的 GizmoRenderer（推荐）

新增 `GizmoRenderer` 类，提供绘制线段、圆、箭头等基础图元的 static 方法：

```cpp
class GizmoRenderer
{
public:
    static void Init();
    static void Shutdown();
    static void BeginScene(const EditorCamera& camera);
    static void EndScene();

    static void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);
    static void DrawWireBox(const glm::vec3& center, const glm::vec3& size, const glm::vec4& color);
    static void DrawArrow(const glm::vec3& origin, const glm::vec3& direction, float length, const glm::vec4& color);
    static void DrawGrid(float size, int divisions);
};
```

**优点**：与主渲染流程完全解耦，独立的 Shader 和绘制逻辑  
**缺点**：需要额外的 Shader（线段着色器）

##### 方案 B：集成 ImGuizmo

使用 ImGuizmo 库实现 Transform 操控手柄：

**优点**：成熟的库，功能完善  
**缺点**：只解决 Transform 操控，不解决灯光方向线、网格线等需求

##### 推荐方案

**A + B 结合**：用 `GizmoRenderer` 实现基础图元绘制（线段、箭头、网格线），用 ImGuizmo 实现 Transform 操控手柄。

#### 涉及文件

| 文件 | 说明 |
|------|------|
| `GizmoRenderer.h/cpp`（新增） | Gizmo 渲染器 |
| `Gizmo.vert/frag`（新增） | 线段/图元着色器 |
| `SceneViewportPanel.cpp` | 在场景渲染后调用 GizmoRenderer |

---

## 六、阶段 3：内容扩展

### P3-1 模型导入（AssetImporter）

#### 必要性分析

| 维度 | 说明 |
|------|------|
| **当前问题** | 只有 `MeshFactory::CreateCube()` 一个硬编码的立方体，无法导入外部模型 |
| **为什么需要** | 没有模型导入，引擎只能渲染立方体，无法验证光照/材质在复杂模型上的效果 |
| **改动范围** | 集成 Assimp 库，新增 `MeshImporter` 类 |
| **风险** | 中。Assimp 是大型第三方库，需要处理编译配置 |

#### 核心功能

| 功能 | 说明 |
|------|------|
| 支持 `.obj` 格式 | 最基础的 3D 模型格式，广泛支持 |
| 支持 `.fbx` 格式 | 工业标准格式，支持动画（后续扩展） |
| 支持 `.gltf`/`.glb` 格式 | 现代标准格式，Web 友好 |
| 自动创建 SubMesh | 根据模型中的 mesh 分组创建 SubMesh |
| 自动创建材质 | 根据模型中的材质信息创建 Material 并关联 |
| 法线/切线计算 | 如果模型缺少法线或切线，自动计算 |

#### 实现方案

```cpp
class MeshImporter
{
public:
    /// 从文件导入网格
    /// @param filepath 模型文件路径
    /// @return 导入的 Mesh（包含所有 SubMesh）
    static Ref<Mesh> Import(const std::string& filepath);

    /// 从文件导入网格和材质
    /// @param filepath 模型文件路径
    /// @param outMaterials 输出的材质列表
    /// @return 导入的 Mesh
    static Ref<Mesh> ImportWithMaterials(const std::string& filepath, 
                                          std::vector<Ref<Material>>& outMaterials);
};
```

#### 第三方依赖

| 库 | 用途 | 集成方式 |
|----|------|---------|
| Assimp | 模型文件解析 | 作为子模块或预编译库引入 |

#### 编辑器集成

- 拖拽模型文件到场景视口 → 自动创建实体 + MeshFilter + MeshRenderer
- Inspector 中 MeshFilter 组件显示模型文件路径
- 支持重新导入（模型文件修改后刷新）

---

### P3-2 多光源支持

#### 必要性分析

| 维度 | 说明 |
|------|------|
| **当前问题** | 仅支持单个方向光，通过 UBO (binding=1) 传递固定结构 |
| **为什么暂缓** | 需要 Shader 架构级改动，改动量大，当前单光源已满足基本需求 |
| **前置条件** | P2-1 渲染排序完成后再考虑 |
| **改动范围** | Shader 重写 + UBO/SSBO 改造 + 新增光源组件 |

#### 需要支持的光源类型

| 光源类型 | 组件名 | 参数 | 说明 |
|---------|--------|------|------|
| 方向光 | DirectionalLightComponent | Color, Intensity | 已有（Phase 6） |
| 点光源 | PointLightComponent | Color, Intensity, Range, Attenuation | 从一个点向所有方向发光 |
| 聚光灯 | SpotLightComponent | Color, Intensity, Range, InnerAngle, OuterAngle | 锥形光束 |

#### Shader 改造方案

##### 方案 A：Uniform 数组（简单，推荐初期使用）

```glsl
#define MAX_DIRECTIONAL_LIGHTS 4
#define MAX_POINT_LIGHTS 16
#define MAX_SPOT_LIGHTS 8

layout(std140, binding = 1) uniform Lights
{
    int DirectionalLightCount;
    int PointLightCount;
    int SpotLightCount;
    
    DirectionalLight DirectionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLight PointLights[MAX_POINT_LIGHTS];
    SpotLight SpotLights[MAX_SPOT_LIGHTS];
};
```

**优点**：实现简单，兼容性好（OpenGL 3.3+）  
**缺点**：光源数量有上限，UBO 大小有限制（通常 16KB）

##### 方案 B：SSBO（灵活，推荐后期使用）

```glsl
layout(std430, binding = 2) buffer LightBuffer
{
    int LightCount;
    Light Lights[];  // 动态大小数组
};
```

**优点**：光源数量无硬编码上限，大小限制宽松（通常 128MB+）  
**缺点**：需要 OpenGL 4.3+，实现稍复杂

##### 推荐策略

初期使用**方案 A**（Uniform 数组），满足大多数场景需求。当光源数量需求超过上限时，再迁移到**方案 B**（SSBO）。

#### 渲染策略

| 策略 | 说明 | 适用场景 |
|------|------|---------|
| 单 Pass Forward | 所有光源在一个 Pass 中计算（当前方案的自然扩展） | 光源数量 < 20 |
| Multi-Pass Forward | 每个光源一个 Pass，结果叠加 | 光源数量中等，但需要更多灵活性 |
| Forward+ | 基于 Tile 的光源剔除 | 大量光源（> 100） |
| Deferred Rendering | G-Buffer + 延迟光照计算 | 大量光源 + 复杂材质 |

**推荐**：当前阶段使用**单 Pass Forward**，在 Shader 中循环遍历所有光源。这是最简单的扩展方式，与现有架构完全兼容。

---

## 七、远期展望（暂不实施）

以下功能在当前阶段不建议实施，但作为远期发展方向记录：

| 功能 | 说明 | 前置条件 |
|------|------|---------|
| **阴影映射（Shadow Mapping）** | 方向光阴影需要额外的 Shadow Pass（深度渲染到 Shadow Map） | P2-1 延迟提交（复用 DrawCommand 列表） |
| **后处理管线（Post-Processing）** | Bloom、Tone Mapping、FXAA 等 | P2-1 延迟提交 + Framebuffer 链 |
| **PBR 材质** | 基于物理的渲染（Metallic-Roughness 工作流） | 多光源支持 + IBL 环境光 |
| **RenderPass 系统** | 抽象的多 Pass 渲染框架 | 阴影映射 + 后处理需求明确后 |
| **骨骼动画** | Skinned Mesh 渲染 | 模型导入（Assimp 支持骨骼数据） |
| **粒子系统** | GPU 粒子或 CPU 粒子 | 渲染排序（透明排序） |
| **场景八叉树/BVH** | 空间加速结构，用于视锥体剔除 | 场景物体数量 > 100 时 |
| **多视口渲染** | 支持多个 Scene View | 渲染器从 static 改为实例化 |

---

## 八、实施路线图时间线

```
Phase 6 (已设计)          P1-1              P1-2              P2-1              P2-2              P3-1              P3-2
DirectionalLight    材质属性Map改造     场景序列化       渲染排序+延迟提交    Gizmo渲染系统      模型导入          多光源支持
     │                   │                │                 │                 │                │                │
     ▼                   ▼                ▼                 ▼                 ▼                ▼                ▼
┌─────────┐      ┌──────────┐      ┌──────────┐      ┌──────────┐      ┌──────────┐      ┌──────────┐      ┌──────────┐
│ ?? 最高  │ ──→ │  ?? 高   │ ──→ │  ?? 高   │ ──→ │  ?? 中高  │ ──→ │  ?? 中   │ ──→ │  ?? 中   │ ──→ │  ?? 低   │
│ 消除硬编码│      │ 数据结构  │      │ 持久化   │      │ 渲染质量  │      │ 编辑体验  │      │ 内容丰富  │      │ 光照丰富  │
│ 组件化灯光│      │ 优化     │      │ 保存/加载 │      │ 透明排序  │      │ 可视化   │      │ 外部模型  │      │ 多种灯光  │
└─────────┘      └──────────┘      └──────────┘      └──────────┘      └──────────┘      └──────────┘      └──────────┘
                                                                                                                  │
                                                                                                                  ▼
                                                                                                          ┌──────────────┐
                                                                                                          │   远期展望    │
                                                                                                          │ 阴影/后处理   │
                                                                                                          │ PBR/动画     │
                                                                                                          └──────────────┘
```

### 关于渲染管线改造的时机建议

| 时机 | 触发条件 | 改造内容 |
|------|---------|---------|
| **P2-1 阶段** | 引入透明材质 / 场景物体增多 | DrawMesh 延迟提交 + 排序（轻量级，仅改 Renderer3D.cpp 内部） |
| **阴影需求出现时** | 需要方向光阴影 | 引入 Shadow Pass 概念（在 EndScene 中插入深度渲染步骤） |
| **后处理需求出现时** | 需要 Bloom / Tone Mapping | 引入 Framebuffer 链 + 后处理 Pass |
| **多种渲染路径时** | 需要同时支持 Forward / Deferred | 引入 RenderPipeline 抽象类 |

**核心原则**：不要提前引入不需要的抽象。每次改造都应该由具体的功能需求驱动，而不是为了"架构完整性"。
