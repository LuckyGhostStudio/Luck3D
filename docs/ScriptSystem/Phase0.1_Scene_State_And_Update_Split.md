# Phase 0.1：Scene 运行状态机 + 世界推进 / 相机渲染分离

## 1. 概述

本 Phase 是脚本系统的第一块地基。目标：

1. 给 `Scene` 引入明确的运行状态 `SceneState { Edit, Play, Pause }`
2. **将"世界推进"与"相机渲染"彻底分离**，形成 Unity 一致的 Scene / Game 双面板结构基础：
   - **世界推进**（每帧一次，与窗口无关）：`OnUpdateEditor / OnUpdateRuntime`
   - **相机渲染**（每窗口一次，可以调用多次）：`OnRenderEditor / OnRenderRuntime`
3. 把世界推进的调用点**上移到 `EditorLayer::OnUpdate`**，`SceneViewportPanel` 只保留渲染调用
4. 提供 `OnRuntimeStart / OnRuntimeStop` 生命周期钩子（本 Phase 只留空实现，未来 Phase 1 引入脚本、Phase 4 引入物理时挂东西进去）

### 关键语义（对齐 Unity）

Scene 窗口和 Game 窗口是**两个并列存在的窗口**，都渲染同一个 Scene，只是视角与 Overlay 不同：

| 特性 | Scene 窗口 | Game 窗口 |
|------|-----------|-----------|
| 相机来源 | `EditorCamera` | 场景中 `Primary CameraComponent`（P0.2 引入） |
| Gizmo / Grid / Outline | 显示 | 不显示 |
| 拖拽 / 拾取 | 支持 | 不支持 |
| Edit 状态下 | 正常渲染 | 也正常渲染（预览游戏相机构图） |
| Play 状态下 | 正常渲染 | 正常渲染 |

每帧的时间线：

```
EditorLayer::OnUpdate(dt)
    ├── 步骤 0：每帧统计清零（原来在 Scene::OnUpdate 内做，现在上移，保证 Scene/Game 面板都在同一帧的统计里）
    ├── 步骤 1：世界推进（每帧唯一一次，与任何窗口无关）
    │        Scene::OnUpdateEditor(dt)  或  Scene::OnUpdateRuntime(dt)
    │        └── UpdateTransformHierarchy + [脚本 tick / 物理 tick]
    └── 步骤 2：面板渲染（PanelManager 内部按序）
             ├── SceneViewportPanel → Scene::OnRenderEditor(EditorCamera&) + Gizmo/Grid/Outline
             └── GameViewportPanel  → Scene::OnRenderRuntime()   ※ P0.6 才启用
```

### 前置依赖
- 无。这是 P0 的第一步。

### 本 Phase **不做**的事
- 不加 Play/Stop 工具条（P0.5）
- 不加 `CameraComponent`（P0.2）?? 因此 `OnRenderRuntime` 内**先给空壳实现**（P0.1 不真正被调用；等 P0.6 建立 Game 面板、P0.2 建立主相机后再补齐）
- 不加 Game 面板（P0.6）
- 不做 `Scene::Copy`（P0.3）?? Play/Stop 切换本身也留到 P0.5 一起接线，本 Phase 只保证"接口和分支到位"

---

## 2. 涉及的文件

### 需要修改
| 文件 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Scene/Scene.h` | 加 `SceneState` 枚举、`GetState/SetState`；把 `OnUpdate` 拆成 `OnUpdateEditor/Runtime`（无 camera）+ `OnRenderEditor/Runtime`；加 `OnRuntimeStart/Stop` |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | 拆函数实现：世界推进段 vs 渲染段；加空的 Start/Stop |
| `Luck3DApp/Source/EditorLayer.h/.cpp` | `OnUpdate` 内新增"世界推进"调用（按 `Scene::GetState()` 分发） |
| `Luck3DApp/Source/Panels/SceneViewportPanel.cpp` | 原 `m_Scene->OnUpdate(dt, m_EditorCamera)` 改为 `m_Scene->OnRenderEditor(m_EditorCamera)`；不再自己驱动世界推进 |

### 无需新建文件
所有变更都落在现有文件中，避免过早引入新头文件。

---

## 3. 现状分析

### 3.1 当前 `Scene::OnUpdate` 的职责

`Scene::OnUpdate(DeltaTime, EditorCamera&)` 内部做了以下事情（详见 [Scene.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp) 第 148 行起）：

1. `UpdateTransformHierarchy()` ?? 更新世界变换层级
2. 收集所有 `LightComponent` 组成 `SceneLightData`
3. `Renderer3D::BeginScene(camera, sceneLightData)`
4. 收集 `PostProcessVolumeComponent` → `Renderer3D::SetPostProcessSettings`
5. `Renderer3D::SetEnvironmentSettings`
6. 遍历 Mesh 组合 → `Renderer3D::DrawMesh`
7. 遍历 Sprite 组合 → `Renderer3D::DrawSprite`
8. `Renderer3D::EndScene()`

**观察**：这 8 步可以清晰地分成两段：
- **世界推进段**（与相机无关）：步骤 1、2
- **相机渲染段**（依赖具体相机）：步骤 3~8

这个天然的分界线正是 Scene / Game 双面板架构所需要的。

### 3.2 当前唯一调用点

全项目 `Scene::OnUpdate` 只在 [SceneViewportPanel.cpp](../../Luck3DApp/Source/Panels/SceneViewportPanel.cpp) 的 `OnUpdate` 内被调用一次：

```cpp
m_Scene->OnUpdate(dt, m_EditorCamera);   // 更新场景
```

因此拆分的**外部影响面极小**。

### 3.3 `m_IsRunning` 字段

[Scene.h](../../Lucky/Source/Lucky/Scene/Scene.h) 中已有 `bool m_IsRunning = false;` 和 `bool IsRunning() const`，但全项目搜索发现**没有任何地方读取或写入过它**。可以直接删除或以 `m_SceneState` 替代。

### 3.4 `EditorLayer::OnUpdate` 现状

当前 [EditorLayer.cpp](../../Luck3DApp/Source/EditorLayer.cpp) 的 `OnUpdate` 只做了一件事：

```cpp
void EditorLayer::OnUpdate(DeltaTime dt)
{
    m_PanelManager->OnUpdate(dt);
}
```

本 Phase 需要在 `m_PanelManager->OnUpdate(dt)` **之前**插入"世界推进"调用。

---

## 4. 详细设计

### 4.1 SceneState 枚举

放在 `Scene.h` 内、`class Scene` 之前（同一 `namespace Lucky` 下），使用 `enum class`：

```cpp
namespace Lucky
{
    /// <summary>
    /// 场景运行状态
    /// - Edit：编辑器态，世界推进走 OnUpdateEditor（不跑脚本/物理）
    /// - Play：运行态，世界推进走 OnUpdateRuntime（跑脚本/物理）
    /// - Pause：仍走 OnUpdateRuntime，但内部会跳过脚本/物理 tick，仅保留 Transform 层级更新与渲染
    /// </summary>
    enum class SceneState : uint8_t
    {
        Edit = 0,
        Play,
        Pause
    };

    class Scene : public Asset { ... };
}
```

底层类型显式声明 `uint8_t`（对齐 `AssetType`、`ComponentType` 的项目惯例）。

### 4.2 Scene.h 接口变更

在 `class Scene` 中：

**删除**
```cpp
bool IsRunning() const { return m_IsRunning; }        // 删
// ...
bool m_IsRunning = false;                             // 删
void OnUpdate(DeltaTime dt, EditorCamera& camera);    // 删（拆分为下面 4 个接口）
```

**新增（public）**
```cpp
/// <summary>
/// 获取当前运行状态
/// </summary>
SceneState GetState() const { return m_State; }

/// <summary>
/// 直接设置运行状态（不触发任何回调；仅用于内部/序列化场景）
/// 业务层切换 Play/Stop 请使用 OnRuntimeStart / OnRuntimeStop
/// </summary>
void SetState(SceneState state) { m_State = state; }

/// <summary>
/// 进入运行态：由外部（编辑器工具条 / 独立运行时入口）在切换到 Play 前调用
/// 本 Phase 内为空实现；未来 Phase 1 用于实例化脚本对象，Phase 4 用于启动物理世界
/// 内部会把 State 置为 Play
/// </summary>
void OnRuntimeStart();

/// <summary>
/// 退出运行态：由外部在切回 Edit 前调用
/// 本 Phase 内为空实现；未来用于释放脚本对象、销毁物理世界
/// 内部会把 State 置为 Edit
/// </summary>
void OnRuntimeStop();

// ---- 世界推进（每帧唯一一次，由 EditorLayer 驱动） ----

/// <summary>
/// 编辑器态世界推进：Edit 状态下每帧调用
/// 只做 Transform 层级更新等状态数据准备，不做任何相机渲染
/// </summary>
void OnUpdateEditor(DeltaTime dt);

/// <summary>
/// 运行时世界推进：Play / Pause 状态下每帧调用
/// 未来会驱动脚本 OnUpdate、物理 tick；Pause 状态下跳过这些，仅保留 Transform 层级更新
/// 与相机无关，不做渲染
/// </summary>
void OnUpdateRuntime(DeltaTime dt);

// ---- 相机渲染（每窗口一次，可被多个面板重复调用） ----

/// <summary>
/// 编辑器视角渲染：由 Scene 面板调用
/// 使用外部传入的 EditorCamera 提交渲染。Gizmo / Grid / Outline 等 Overlay
/// 由调用方（SceneViewportPanel）在此调用之后自行绘制
/// </summary>
/// <param name="camera">编辑器相机</param>
void OnRenderEditor(EditorCamera& camera);

/// <summary>
/// 游戏视角渲染：由 Game 面板调用（P0.6 引入 GameViewportPanel 后启用）
/// 使用场景内 Primary CameraComponent 作为视图/投影来源
/// P0.2 引入 CameraComponent 之前，本函数保持空实现（不做任何绘制）
/// </summary>
void OnRenderRuntime();
```

**新增（private）**
```cpp
SceneState m_State = SceneState::Edit;

/// <summary>
/// 相机渲染实现：给定 view / projection 矩阵，跑一遍完整的渲染流程
/// OnRenderEditor 和 OnRenderRuntime 都在拿到自己的相机数据后统一走这里
/// 内部执行"收集光源 → BeginScene → 收集后处理 → 提交 Mesh/Sprite → EndScene"
/// 注意：不做 Transform 层级更新（那是世界推进的职责）
/// </summary>
/// <param name="viewMatrix">视图矩阵</param>
/// <param name="projectionMatrix">投影矩阵</param>
/// <param name="cameraPosition">相机世界坐标（用于 PBR 计算）</param>
void RenderSceneImpl(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPosition);
```

> **注**：`RenderSceneImpl` 的参数最终形式取决于 `Renderer3D::BeginScene` 的签名（当前是 `BeginScene(EditorCamera&, SceneLightData&)`）。P0.1 阶段可以先保持 `RenderSceneImpl(EditorCamera& camera)` 签名以最小改动，等 P0.2 引入 `CameraComponent` 后再抽象为矩阵接口。详见【决策点 3】。

### 4.3 Scene.cpp 实现

**核心思路**：把现有 `OnUpdate` 拆成两段：
- 世界推进段（原步骤 1、2） → `OnUpdateEditor` / `OnUpdateRuntime`
- 相机渲染段（原步骤 3~8） → 私有 `RenderSceneImpl`，由 `OnRenderEditor` 调用

伪代码：

```cpp
void Scene::OnUpdateEditor(DeltaTime dt)
{
    // Edit 态世界推进
    UpdateTransformHierarchy();
    // Phase 0.1 只做层级更新；后续可能加"编辑器专属"更新（例如 Gizmo 状态刷新）
}

void Scene::OnUpdateRuntime(DeltaTime dt)
{
    // Play/Pause 世界推进
    // Phase 1 起会加：
    //   if (m_State == SceneState::Play) { ScriptEngine::OnUpdate(this, dt); }
    // Phase 4 起会加：
    //   if (m_State == SceneState::Play) { PhysicsWorld::Tick(dt); }

    UpdateTransformHierarchy();
}

void Scene::OnRenderEditor(EditorCamera& camera)
{
    RenderSceneImpl(camera);   // P0.1 采用与原实现最接近的签名
}

void Scene::OnRenderRuntime()
{
    // P0.1 阶段留空
    // P0.2 引入 CameraComponent 后：
    //   1. 遍历 <TransformComponent, CameraComponent> 找 Primary
    //   2. 构造 view / projection
    //   3. 调用 RenderSceneImpl(...)
    // P0.6 建立 GameViewportPanel 后才会被真正调用
}

void Scene::RenderSceneImpl(EditorCamera& camera)
{
    // 原 OnUpdate 中"步骤 3~8"整块搬过来：
    //   收集光源 → Renderer3D::BeginScene → 后处理设置 → 环境设置
    //   → 遍历 Mesh → 遍历 Sprite → Renderer3D::EndScene
    // 注意：不再包含 UpdateTransformHierarchy（那是世界推进的职责）
    // 注意：不再包含 ResetStats（未来 Game 面板也渲染时会一起清；改到 EditorLayer 里做一次）
}

void Scene::OnRuntimeStart()
{
    m_State = SceneState::Play;
    // Phase 1 会在此实例化脚本
}

void Scene::OnRuntimeStop()
{
    m_State = SceneState::Edit;
    // Phase 1 会在此释放脚本
}
```

> **关于 `Renderer3D::ResetStats` 的搬迁**：原 `OnUpdate` 在渲染开头会 `ResetStats`。**推荐挪到 `EditorLayer::OnUpdate` 的"世界推进之前"位置**，这样即使一帧内 Scene / Game 面板各渲染一次，Stats 也只清零一次、统计的是同一帧的总量。见 4.4。

### 4.4 EditorLayer.cpp 调用改造

当前：

```cpp
void EditorLayer::OnUpdate(DeltaTime dt)
{
    m_PanelManager->OnUpdate(dt);
}
```

改为：

```cpp
void EditorLayer::OnUpdate(DeltaTime dt)
{
    // 步骤 0：每帧统计清零（原来在 Scene::OnUpdate 内做，现在上移，保证 Scene/Game 面板都在同一帧的统计里）
    Renderer3D::ResetStats();
    Renderer2D::ResetStats();

    // 步骤 1：世界推进（每帧唯一一次，与任何窗口无关）
    if (const Ref<Scene>& scene = SceneManager::GetActiveScene())
    {
        switch (scene->GetState())
        {
            case SceneState::Edit:
                scene->OnUpdateEditor(dt);
                break;
            case SceneState::Play:
            case SceneState::Pause:
                scene->OnUpdateRuntime(dt);
                break;
        }
    }

    // 步骤 2：各面板负责渲染各自的相机视角
    m_PanelManager->OnUpdate(dt);
}
```

> P0.1 阶段 `Scene::m_State` 始终是 `Edit`（因为没人调 `OnRuntimeStart`），所以运行时行为**完全不变**。这是本 Phase 的验收基准。

### 4.5 SceneViewportPanel.cpp 调用点改造

原代码（[SceneViewportPanel.cpp](../../Luck3DApp/Source/Panels/SceneViewportPanel.cpp) `OnUpdate` 内）：

```cpp
m_Scene->OnUpdate(dt, m_EditorCamera);   // 更新场景
```

改为：

```cpp
m_Scene->OnRenderEditor(m_EditorCamera);   // Scene 面板：编辑相机渲染
```

改造要点：
- **不再驱动世界推进**（已由 `EditorLayer::OnUpdate` 承担）
- `m_EditorCamera.OnUpdate(dt)` 保留在原位（编辑相机的移动响应输入，属于面板自身的行为）
- Framebuffer 绑定 / 清屏 / Renderer3D::SetTargetFramebuffer / Outline 收集 / GizmoRenderer 调用 / Framebuffer 解绑等**全部保留不动**??它们是"Scene 面板专属"逻辑，正好体现"Overlay 属于面板而非 Scene 类"

### 4.6 兼容策略

**关于旧 `OnUpdate` 是否保留**：**直接删除**。原因：
- 全项目只有一处调用它，改起来无成本
- 保留会让新代码不知道该调哪个（选择困难 + 潜在 Bug）
- Layer 系的 `OnUpdate` 是 `Layer::OnUpdate`，跟 `Scene::OnUpdate` 无关，不构成命名冲突

---

## 5. 关键决策点与方案对比

### 5.1 【决策点 1】状态枚举的组织形式

#### 方案 A：`enum class SceneState`（**推荐 ★★★**）

```cpp
enum class SceneState : uint8_t { Edit, Play, Pause };
```

- **优点**
  - 类型安全，不会被隐式转 int
  - 底层类型固定为 `uint8_t`，未来序列化到 YAML 时字段大小可控
  - 与项目内 `AssetType`、`ComponentType`、`LightType` 完全一致的风格
- **缺点**
  - 使用时需要写 `SceneState::Play`，略?嗦（但项目其它 enum 都是这个风格，一致性优先）

#### 方案 B：`bool m_IsPlaying + bool m_IsPaused` 两个 bool 组合

- **优点**：改动最小，直接沿用现有 `m_IsRunning` 命名习惯
- **缺点**
  - 状态空间松散：`(IsPlaying=false, IsPaused=true)` 是非法组合，需要额外校验
  - 未来扩展 `Simulate`（只跑物理不跑脚本）时要再加 bool，指数级膨胀
  - 与项目内其它系统的 enum 风格不一致

#### 方案 C：`enum SceneState`（无 `class`）

- **优点**：使用时不用写命名空间前缀
- **缺点**：污染 `Lucky` 命名空间（`Edit / Play / Pause` 是非常容易撞名的词），编译器不做类型检查

**结论**：采用**方案 A**。

### 5.2 【决策点 2】"世界推进"与"相机渲染"是否要分离

这是本 Phase 相对上一版最重要的调整点。

#### 方案 A：分离（**推荐 ★★★**）

- 世界推进：`OnUpdateEditor / OnUpdateRuntime`（每帧一次，由 `EditorLayer` 调用）
- 相机渲染：`OnRenderEditor / OnRenderRuntime`（每窗口一次，由各面板调用）

- **优点**
  - 与 Unity 的实际内部结构一致（`Update → LateUpdate → 每个 Camera.Render`）
  - Scene / Game 双面板天然支持：同一 Scene 可以被两个面板各渲染一次
  - **顺序无关**：Scene 面板和 Game 面板谁先渲染都能拿到同一份"当前帧世界状态"
  - 未来插脚本 tick / 物理 tick 的位置清晰、单一（在 `OnUpdateRuntime` 内），不会被面板重复调用
  - 相机渲染是"纯输入函数"（给相机 → 提交绘制），未来还能被"离屏渲染 / 反射探针 / 后处理烘焙"等场景复用
- **缺点**
  - 接口从 2 个 (`OnUpdateEditor/Runtime`) 变为 4 个（多了 `OnRenderEditor/Runtime`），初学者略难理解
  - `EditorLayer::OnUpdate` 需要新增"世界推进"调用（改动面板之外的文件）

#### 方案 B：不分离（原文档旧方案：`OnUpdateEditor/Runtime` 内既跑世界推进又跑渲染）

- **优点**
  - 接口少，只有 2 个 Update
  - 世界推进 + 渲染绑定在一个函数里，调用方省心
- **缺点**
  - **无法支持 Game 面板**：如果 Scene / Game 面板各调用一次 `OnUpdateEditor`/`OnUpdateRuntime`，世界推进会跑两次（脚本 tick 两次 = 灾难）
  - 即使加"是否推进"参数，也是把控制反转丢给调用方，容易出错
  - 与 Unity 结构不一致

#### 方案 C：世界推进保留在 Scene 内自我判断（例如"本帧是否已经推进过"用一个 dirty flag）

- **优点**：调用方不用关心顺序
- **缺点**
  - 隐藏状态多，调试困难
  - 一帧内何时"归零"需要额外的机制
  - 违反"显式优于隐式"原则

**结论**：采用**方案 A**，架构上一步到位；接口数量增加是可接受成本。

### 5.3 【决策点 3】`RenderSceneImpl` 内部相机参数的抽象层次

背景：现在 `Renderer3D::BeginScene` 接受 `EditorCamera&`。P0.2 的 `CameraComponent` 内含 `SceneCamera`，二者签名不同。

#### 方案 A：P0.1 阶段保持 `RenderSceneImpl(EditorCamera&)`，P0.2 时再抽象（**推荐 ★★★**）

- **优点**
  - P0.1 改动最小；原步骤 3~8 几乎原样搬过来
  - 避免"为了未来需求而做的抽象"（现在还看不到 `SceneCamera` 的具体接口）
  - `OnRenderRuntime` 在 P0.1 阶段直接留空（反正也没人调用），P0.2 落地时再补齐
- **缺点**
  - P0.2 时要再改一次 `RenderSceneImpl` 签名（可预期变更，影响面小，只有 `OnRenderEditor` 一个调用者）

#### 方案 B：P0.1 就把 `RenderSceneImpl` 抽象为矩阵接口（`view / projection / cameraPos`）

- **优点**
  - 一次到位，未来 `EditorCamera` 和 `SceneCamera` 都能接
  - 更利于未来"离屏渲染"等场景复用
- **缺点**
  - 需要同步改 `Renderer3D::BeginScene` 签名（涉及 Renderer3D 内部大量代码）??这已经**超出 P0.1 的范围**，风险扩大
  - 或者在 `RenderSceneImpl` 内部构造一个临时的 "假 EditorCamera" 喂给 `Renderer3D::BeginScene`??绕圈子，得不偿失

#### 方案 C：先给 `Renderer3D::BeginScene` 加一个矩阵版本重载

- **优点**：兼容旧接口，同时铺路
- **缺点**：属于 Renderer3D 的改造，工作量应该记到 P0.2 而不是 P0.1

**结论**：采用**方案 A**。P0.1 用 `EditorCamera&`，P0.2 引入 `CameraComponent` 时统一处理 `Renderer3D::BeginScene` 的重构。

### 5.4 【决策点 4】`Renderer3D::ResetStats` 的调用位置

#### 方案 A：上移到 `EditorLayer::OnUpdate`（**推荐 ★★★**）

- **优点**
  - 一帧统计一次，Scene / Game 面板都渲染时能得到"整帧总量"
  - 位置显眼，未来加 `Renderer2D::ResetStats`、其他统计源都往同一处放
- **缺点**：无

#### 方案 B：留在 `RenderSceneImpl` 开头（每次相机渲染都清零）

- **优点**：改动最小
- **缺点**
  - 未来 Scene / Game 各渲染一次时，Game 面板的 Stats 会覆盖 Scene 面板的（或反之），"每帧 Draw Call 数"面板显示的就是最后一个渲染的相机的数，不准

#### 方案 C：完全不管，等未来出问题再处理

- **缺点**：会给未来 P0.6 的 Game 面板留一个坑

**结论**：采用**方案 A**。

### 5.5 【决策点 5】`m_IsRunning` 字段的处理

#### 方案 A：直接删除（**推荐 ★★★**）

- **优点**：全项目零引用，删除无风险，避免"两套状态"歧义
- **缺点**：无

#### 方案 B：保留 `IsRunning()`，改为 `return m_State == SceneState::Play;`

- **优点**：如果未来外部代码想问"是否在跑"，语义直观
- **缺点**：目前没有任何调用者，属于臆想需求；有需要再加

**结论**：采用**方案 A**，直接删除。

### 5.6 【决策点 6】`OnRuntimeStart / OnRuntimeStop` 的位置

#### 方案 A：作为 `Scene` 的成员方法（**推荐 ★★★**）

- **优点**
  - 未来在这里遍历 `ScriptComponent` 时可以直接访问 `m_Registry`
  - 与 `Scene::OnUpdate*` 风格一致
- **缺点**：无

#### 方案 B：放在 `SceneManager` 中作为静态方法

- **优点**：与 Play 按钮的接线更近
- **缺点**：需要通过 `SceneManager::GetActiveScene()->m_Registry` 访问，破坏 Scene 封装

**结论**：采用**方案 A**。

---

## 6. 验收标准

本 Phase 完成后应满足：

1. **编译通过**：`Scene.h/.cpp`、`EditorLayer.h/.cpp`、`SceneViewportPanel.cpp` 修改后项目正常编译
2. **零回归**：编辑器启动后行为**与改造前完全一致**（因为 `m_State` 默认为 `Edit`，`EditorLayer` 走 `OnUpdateEditor`，`SceneViewportPanel` 走 `OnRenderEditor`；两者组合起来的效果 = 原来的 `OnUpdate`）
3. **接口就位**：以下调用可用
   - `scene->GetState()` 返回 `SceneState::Edit`
   - `scene->OnRuntimeStart()` 后 `GetState() == SceneState::Play`
   - `scene->OnRuntimeStop()` 后 `GetState() == SceneState::Edit`
4. **手动验证**：在 `EditorLayer::OnUpdate` 里临时加一行 `SceneManager::GetActiveScene()->OnRuntimeStart();`，画面应当仍然正常渲染。验证完删除该临时代码

---

## 7. 后续 Phase 的接入点预告

本 Phase 留下的扩展点：

| 位置 | 后续 Phase | 会加什么 |
|------|-----------|---------|
| `Scene::OnRuntimeStart` | Phase 1（脚本） | 遍历 `ScriptComponent`，实例化托管对象，调用 `OnCreate` |
| `Scene::OnRuntimeStop` | Phase 1 | 调用脚本 `OnDestroy`，释放托管对象引用 |
| `Scene::OnUpdateRuntime`（`UpdateTransformHierarchy` 之前） | Phase 1 | `ScriptEngine::OnUpdate(dt)`（仅 Play 状态）|
| `Scene::OnUpdateRuntime`（`UpdateTransformHierarchy` 之前） | Phase 4 | `PhysicsWorld::Step(dt)`（仅 Play 状态）|
| `Scene::OnRenderRuntime` | P0.2 + P0.6 | P0.2 补齐主相机查找 + view/projection 计算；P0.6 由 `GameViewportPanel` 调用 |
| `EditorLayer::OnUpdate` 里的 `switch` 分发 | P0.5 | 增加 Play/Stop 按钮触发 `OnRuntimeStart/Stop`；同时接入 `Scene::Copy`（P0.3）做 Edit/Runtime 场景切换 |

---

## 8. 变更清单速览

- **新增**
  - `Scene.h`：`enum class SceneState`
  - `Scene.h/.cpp`：`GetState / SetState / OnRuntimeStart / OnRuntimeStop / OnUpdateEditor / OnUpdateRuntime / OnRenderEditor / OnRenderRuntime / RenderSceneImpl`
  - `EditorLayer::OnUpdate`：`Renderer3D::ResetStats / Renderer2D::ResetStats` + 世界推进 switch 分发
- **修改**
  - `SceneViewportPanel::OnUpdate`：把 `m_Scene->OnUpdate(dt, m_EditorCamera)` 改为 `m_Scene->OnRenderEditor(m_EditorCamera)`；不再负责世界推进
- **删除**
  - `Scene::IsRunning() / m_IsRunning`
  - `Scene::OnUpdate(DeltaTime, EditorCamera&)`（函数体按"世界推进段 / 相机渲染段"两部分迁移到新接口）
  - `Scene::RenderSceneImpl` 内的 `ResetStats` 调用（改由 `EditorLayer::OnUpdate` 每帧一次）
