# Phase 1.6：Scene Runtime 接入脚本钩子（Cube 动起来）

## 1. 概述

P1.6 目标：把 P1.3~P1.5 铺好的脚本基础设施真正**接入 Scene 世界推进**，让挂了 `ScriptComponent` 的实体在 Play 状态下真正被实例化、被 tick、被清理，Stop 后完整还原。**这一 Phase 完成后即达成脚本系统 MVP**（Console 打印 + Cube 自动向右移动）。

具体涉及三处 `Scene::` 方法的实现补齐：

1. `Scene::OnRuntimeStart`：在切到 Play 时通知 `ScriptEngine`，扫描 `view<ScriptComponent>`，对每个实体逐个实例化托管对象并调用 `Awake`
2. `Scene::OnUpdateRuntime`：在 Play 状态下遍历 `view<ScriptComponent>`，对每个实体调用 `Update(dt)`；Pause 状态跳过脚本 tick，仅保留 Transform 层级更新
3. `Scene::OnRuntimeStop`：通知 `ScriptEngine` 清理所有 `ScriptInstance` 与 `SceneContext`

**本 Phase 明确不做**：

- **不改 `ScriptEngine.h/.cpp`**：P1.3 已经提供 `OnRuntimeStart(Scene*)` / `OnRuntimeStop()` / `OnCreateEntityScript` / `OnUpdateEntityScript` 全套 API，本 Phase 只是消费方
- **不改 `SceneManager.cpp` / `EditorToolbar.cpp` / `EditorLayer.cpp`**：P0.5 已把 Play/Stop 工具条正确接线到 `SceneManager::OnScenePlay/OnSceneStop/SetScenePaused`；`EditorLayer::OnUpdate` 已按 `SceneState` 分派到 `OnUpdateEditor/OnUpdateRuntime`；本 Phase 只需在 `Scene::` 内部补齐即可自动生效
- **不做异常兜底**：脚本抛托管异常时不 try/catch（Roadmap 明确 Phase 3 才做异常处理）
- **不做 Play 中间新增 `ScriptComponent` 的即时实例化**：`Scene::OnComponentAdded<ScriptComponent>` 保持空特化（Roadmap 与 P1.4 §6.3 都已确认此语义）
- **不引入 `Time.DeltaTime` 等托管全局**：`dt` 通过 `Update(float dt)` 参数直接传入，与 P1.3 `ScriptInstance::InvokeUpdate` 已有签名一致

### 1.1 关键约束

- **只改 `Lucky/Source/Lucky/Scene/Scene.cpp` 一个文件**（外加追加两个 include）
- **Scene.h 完全不动**：`OnRuntimeStart / OnUpdateRuntime / OnRuntimeStop` 三个声明已就位（P0.1 落地时已完整声明）；不 include `ScriptEngine.h`，避免 mono 前向声明扩散进整个引擎汇总头
- **脚本 tick 先于 Transform 层级更新**：让脚本本帧修改的 `Translation` 立即在本帧渲染前反映到 `WorldTransform`（详见 4.1 决策 1）
- **Pause 语义与 Scene.h 现有文档一致**：Pause 状态下 `OnUpdateRuntime` 仍被调用但内部跳过脚本 tick（Scene.h 顶部 `SceneState` 注释已明确此语义）
- **托管方法名统一使用 `Awake` / `Update`**：P1.3 落地的 `ScriptInstance` 已用这两个名字（`GetMethod("Awake", 0) / GetMethod("Update", 1)`）；Roadmap 里写的 `OnCreate / OnUpdate` 是历史文本，Sandbox 用户脚本按 `Awake / Update` 命名即可

### 1.2 前置条件

- P0.1 已完成：`SceneState { Edit, Play, Pause }`、`m_State` 字段、`GetState() / SetState()`、`OnRuntimeStart / OnUpdateEditor / OnUpdateRuntime / OnRuntimeStop` 声明就位（[Scene.h](../../Lucky/Source/Lucky/Scene/Scene.h)）
- P0.3 已完成：`Scene::Copy` 深拷贝；`SceneManager::OnScenePlay` 里 `runtimeScene = Scene::Copy(editorScene) → runtimeScene->OnRuntimeStart()` 全链路正确（[SceneManager.cpp](../../Lucky/Source/Lucky/Scene/SceneManager.cpp)）
- P0.5 已完成：全局 Play/Stop/Pause 工具条通过 `SceneManager::OnScenePlay/OnSceneStop/SetScenePaused` 驱动（[EditorToolbar.cpp](../../Luck3DApp/Source/EditorToolbar.cpp)）
- P0.1 + 编辑器接线已完成：`EditorLayer::OnUpdate` 已按 `SceneState` 分派 `OnUpdateEditor / OnUpdateRuntime`（[EditorLayer.cpp](../../Luck3DApp/Source/EditorLayer.cpp) `OnUpdate`）
- P1.3 已完成：`ScriptEngine::OnRuntimeStart(Scene*) / OnRuntimeStop() / OnCreateEntityScript(Entity, const std::string&) / OnUpdateEntityScript(Entity, DeltaTime)` 全部实现，`ScriptInstance::InvokeAwake / InvokeUpdate` 就位（[ScriptEngine.h](../../Lucky/Source/Lucky/Scripting/ScriptEngine.h)）
- P1.4 已完成：`ScriptComponent { std::string ClassName; }` 组件已存在并接入 Scene::Copy / 序列化 / Inspector（[ScriptComponent.h](../../Lucky/Source/Lucky/Scene/Components/ScriptComponent.h)）
- P1.5 已完成：`ScriptGlue::RegisterFunctions / RegisterComponents` 已在 `ScriptEngine::Init` 挂接；`Debug_Log/Warn/Error` 与 `TransformComponent_Get/SetPosition` internal call 就绪（[ScriptGlue.cpp](../../Lucky/Source/Lucky/Scripting/ScriptGlue.cpp)）
- 用户测试脚本：`Assets/Scripts/Binaries/App.dll` 中存在一个继承自 `Lucky.Entity` 的类（例如 `Sandbox.PlayerController`），实体已挂 `ScriptComponent` 且 `ClassName == "Sandbox.PlayerController"`（P1.7 会写这个脚本；本 Phase 落地时可临时手工构造，用于验收）

### 1.3 本 Phase **不做**的事

- 不给 `Scene::OnComponentAdded<ScriptComponent>` 空特化里补代码（Play 状态下动态挂载脚本是罕见路径，本 Phase 不处理）
- 不动 `SceneSerializer`（`ScriptComponent` 的 `ClassName` 早已序列化）
- 不加 `try / catch` 包裹 `ScriptEngine::OnCreateEntityScript / OnUpdateEntityScript`（Phase 3 的目标）
- 不做"Play 中间销毁挂脚本的实体"专用清理钩子（`OnRuntimeStop` 会一次性清空所有 `EntityInstances`，Play 期间的销毁在 MVP 阶段先放任 `ScriptInstance` 悬空??Update 侧的 `ScriptEngine::OnUpdateEntityScript` 会按 UUID 找不到而静默跳过，`ScriptInstance` 到 `OnRuntimeStop` 时统一析构）
- 不加 Time.DeltaTime 全局（P3 目标）
- 不做 `Debug.Log` 的 Console 面板重定向（P3 目标）；本 Phase MVP 验收看 spdlog 输出即可

---

## 2. 涉及的文件

### 2.1 新建

无。

### 2.2 修改

| 文件 | 改动 |
|------|------|
| [Scene.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp) | 顶部追加一个 include（`ScriptEngine.h`；`ScriptComponent.h` 已通过 `Components/Components.h` 间接可用）；补齐 `OnRuntimeStart / OnUpdateRuntime / OnRuntimeStop` 三个方法实现 |

### 2.3 不修改

- [Scene.h](../../Lucky/Source/Lucky/Scene/Scene.h)：三个方法声明与 `SceneState` 定义都已就位
- [ScriptEngine.h/.cpp](../../Lucky/Source/Lucky/Scripting/ScriptEngine.h)：API 完整、`OnCreateEntityScript` 实例化分支已开
- [ScriptGlue.h/.cpp](../../Lucky/Source/Lucky/Scripting/ScriptGlue.cpp)：Debug / Transform internal call 已挂
- [SceneManager.cpp](../../Lucky/Source/Lucky/Scene/SceneManager.cpp)：Play/Stop 驱动链已完整
- [EditorToolbar.cpp](../../Luck3DApp/Source/EditorToolbar.cpp) / [EditorLayer.cpp](../../Luck3DApp/Source/EditorLayer.cpp)：UI 与世界推进分派已完整

---

## 3. 现状回顾

### 3.1 Scene 侧现状：三个 Runtime 方法几乎为空

[Scene.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp) 现状：

```cpp
void Scene::OnUpdateRuntime(DeltaTime dt)
{
    UpdateTransformHierarchy();
}

void Scene::OnRuntimeStart()
{
    m_State = SceneState::Play;
}

void Scene::OnRuntimeStop()
{
    m_State = SceneState::Edit;
}
```

`OnRuntimeStart / OnRuntimeStop` 只切状态；`OnUpdateRuntime` 与 `OnUpdateEditor` 完全一样（都只跑 Transform 层级更新）。**这是 P1.6 唯一需要动的地方。**

### 3.2 ScriptEngine 侧现状：API 完备，等待被调用

[ScriptEngine.cpp](../../Lucky/Source/Lucky/Scripting/ScriptEngine.cpp) 已提供的关键接口：

```cpp
static void OnRuntimeStart(Scene* scene);   // 设 SceneContext
static void OnRuntimeStop();                // 清 SceneContext + clear EntityInstances
static void OnCreateEntityScript(Entity, const std::string& fullClassName);   // 已实例化并 InvokeAwake
static void OnUpdateEntityScript(Entity, DeltaTime);                          // 按 UUID 查 EntityInstances -> InvokeUpdate
static bool EntityScriptClassExists(const std::string& fullClassName);        // 跳过未登记类
```

`OnCreateEntityScript` 内部已经：
1. 从 `EntityClasses` 查 `fullClassName`，找不到时 warn 并 return；
2. `CreateRef<ScriptInstance>(scriptClass, entity)`（构造函数内已经调用 base `.ctor(UUID)`）；
3. 缓存到 `EntityInstances[UUID]`；
4. `InvokeAwake()`。

`OnUpdateEntityScript` 内部已经按 UUID 查表；找不到时静默跳过，找到就 `InvokeUpdate(dt)`。

**P1.6 不需要再包装、不需要再做任何合法性检查??直接调即可。**

### 3.3 Play/Stop 驱动链现状：已完整

[SceneManager.cpp](../../Lucky/Source/Lucky/Scene/SceneManager.cpp) 的 `OnScenePlay`：

```cpp
s_EditorScene = s_ActiveScene;
Ref<Scene> runtimeScene = Scene::Copy(s_ActiveScene);
runtimeScene->OnRuntimeStart();   // ← 本 Phase 要补充脚本相关逻辑的地方
SetActiveScene(runtimeScene);
```

`OnSceneStop`：

```cpp
s_ActiveScene->OnRuntimeStop();   // ← 本 Phase 要补充脚本相关逻辑的地方
SetActiveScene(s_EditorScene);
```

`SetScenePaused` 直接改 `m_State`，不经过 `OnRuntimeStart/Stop`。

[EditorLayer.cpp](../../Luck3DApp/Source/EditorLayer.cpp) 的 `OnUpdate`（已就位）：

```cpp
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
```

**含义**：只要 Scene::OnRuntimeStart/OnUpdateRuntime/OnRuntimeStop 内部补齐脚本逻辑，整条 MVP 链路自动跑通。

### 3.4 Scene.h 顶部对 SceneState 的语义约定

```cpp
enum class SceneState : uint8_t
{
    Edit = 0,
    Play,
    Pause
};
```

其上方注释明确写：
> Pause：仍走 OnUpdateRuntime，但内部跳过脚本/物理 tick，仅保留 Transform 层级更新与渲染

**本 Phase 的 `OnUpdateRuntime` 必须严格实现这一 Pause 语义。**

### 3.5 entt view 遍历返回 `entt::entity`，需要显式包装为 `Entity`

`m_Registry.view<ScriptComponent>()` 每次迭代返回的是 `entt::entity`（原生 handle），需要 `Entity(entityHandle, this)` 才能构造出 Luck3D 的 `Entity` 传给 `ScriptEngine::OnCreateEntityScript / OnUpdateEntityScript`。项目里其它遍历（例如 `Scene::GetPrimaryCameraEntity`）也是这个模式。

---

## 4. 关键设计决策（多方案对比）

### 4.1 决策 1：脚本 tick 与 Transform 层级更新的先后顺序

Play 状态下 `OnUpdateRuntime` 每帧需要做两件事：跑所有 `Update(dt)`、更新 Transform 世界矩阵。这两步的顺序直接影响脚本读到的世界矩阵、以及本帧渲染看到的位置。

#### 方案 A：先脚本 tick → 再 UpdateTransformHierarchy（**推荐 ?**，优先级最高）

```cpp
void Scene::OnUpdateRuntime(DeltaTime dt)
{
    if (m_State == SceneState::Play)
    {
        auto view = m_Registry.view<ScriptComponent>();
        for (entt::entity entityHandle : view)
        {
            Entity entity{ entityHandle, this };
            ScriptEngine::OnUpdateEntityScript(entity, dt);
        }
    }

    UpdateTransformHierarchy();
}
```

- **优点**
  - 脚本本帧修改的 `Translation` 立即通过 `UpdateTransformHierarchy` 反映到 `WorldTransform`，紧接着的 `OnRenderRuntime` 就能画出新位置??用户点击 Play → Cube 立刻可见地移动，符合直觉
  - Unity / Unreal 主流引擎的默认顺序都是"脚本 tick 之后再刷新 world matrix"，用户心智对齐
  - 满足 MVP 出口标准："Play 后 Cube 自动向右移动"??若顺序反了，用户第一帧看不到移动，需要额外等一帧
- **缺点**
  - 脚本第一帧（`Awake` 之后的第一个 `Update`）里读到的 `WorldTransform` 是 `Awake` 前的层级结算结果；对 MVP 而言脚本 `Awake` 本身没跑过 Transform 层级更新，读到的是 Scene::Copy 拷来的旧值（也是正确的编辑态值）??不构成实际问题

#### 方案 B：先 UpdateTransformHierarchy → 再脚本 tick

- **优点**：脚本读到的 `WorldTransform` 是"当前帧最新的层级结算结果"（对复杂父子层级依赖 WorldTransform 的场景有意义）
- **缺点**
  - 脚本改动的 `Translation` 要下一帧才能反映到 `WorldTransform`，用户 Play 后第一帧看不到 Cube 移动
  - MVP 的 `Transform.Position` 读写走的是 `Translation`（Local Space）而不是 `WorldTransform`，本方案的所谓优势对 MVP 完全不成立
  - 与主流引擎心智不一致

#### 方案 C：先 Transform → 脚本 tick → 再 Transform（跑两遍）

- **优点**：脚本既读到最新 WorldTransform，本帧也能立即渲染新位置
- **缺点**
  - 每帧多跑一次层级更新，浪费 CPU
  - 复杂度上升，MVP 阶段没有对应收益

**结论：方案 A**。理由：MVP 出口标准 "Play 后 Cube 移动" 直接要求"脚本改 Translation 后本帧就能看见"，方案 A 唯一满足；且与主流引擎心智一致。

### 4.2 决策 2：`OnRuntimeStart` 的脚本扫描时机

`Scene::OnRuntimeStart` 里除了切状态，还要通知 `ScriptEngine` 并扫描 `view<ScriptComponent>` 实例化托管对象。这两步的顺序、以及 `m_State` 切换的时机都要明确。

#### 方案 A：`ScriptEngine::OnRuntimeStart(this) → 扫描实例化 → m_State = Play`（**推荐 ?**）

```cpp
void Scene::OnRuntimeStart()
{
    ScriptEngine::OnRuntimeStart(this);

    auto view = m_Registry.view<ScriptComponent>();
    for (entt::entity entityHandle : view)
    {
        Entity entity{ entityHandle, this };
        const ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

        if (sc.ClassName.empty())
        {
            continue;
        }
        if (!ScriptEngine::EntityScriptClassExists(sc.ClassName))
        {
            LF_CORE_WARN("Scene::OnRuntimeStart - script class '{0}' not found for entity '{1}'", sc.ClassName, entity.GetName());
            continue;
        }

        ScriptEngine::OnCreateEntityScript(entity, sc.ClassName);
    }

    m_State = SceneState::Play;
}
```

- **优点**
  - `ScriptEngine::OnRuntimeStart(this)` 先设 `SceneContext`，之后 `ScriptGlue` 里 native 函数（如 `TransformComponent_GetPosition`）就能在 `Awake` 里立即使用（用户脚本的 `Awake` 中直接访问 `Transform.Position` 是常见需求）
  - `m_State = Play` 放最后：如果扫描/实例化过程有任何异常，Scene 状态仍是 Edit，语义清晰（虽然本 Phase 不做异常处理，但保留这个安全边界是零成本的）
  - 显式的合法性检查：空 ClassName 静默跳过（P1.4 §6.2 已确认"空 ClassName 是正常中间态"）；非法 ClassName 打 warn 但不 abort
- **缺点**：无

#### 方案 B：`m_State = Play → ScriptEngine::OnRuntimeStart → 扫描实例化`

- **优点**：状态切换在最前，语义"进入 Play 后再实例化"
- **缺点**：如果扫描期间发生异常（未来 Phase 3 加异常处理时），Scene 会停留在 Play 但没实例化任何脚本，状态不自洽

#### 方案 C：不加合法性检查，直接调 `OnCreateEntityScript`（让 ScriptEngine 内部自己处理）

- **优点**：Scene 代码更短
- **缺点**
  - `ScriptEngine::OnCreateEntityScript` 内部只在"类找不到"时打 warn，**没有处理"空 ClassName"**??传空串会当成一个不存在的类名打 warn 刷屏（假设用户挂了 ScriptComponent 但还没填 ClassName，是正常中间态，不该 warn）
  - Scene 是调用方，判 `empty()` 只加两行、语义更清晰

**结论：方案 A**。

### 4.3 决策 3：`OnUpdateRuntime` 的 Pause 语义实现

Scene.h 顶部注释明确：`Pause` 状态仍走 `OnUpdateRuntime`，但内部跳过脚本 tick，仅保留 Transform 层级更新。

#### 方案 A：`if (m_State == SceneState::Play)` 内跑脚本 tick，`UpdateTransformHierarchy` 无条件跑（**推荐 ?**）

```cpp
void Scene::OnUpdateRuntime(DeltaTime dt)
{
    if (m_State == SceneState::Play)
    {
        auto view = m_Registry.view<ScriptComponent>();
        for (entt::entity entityHandle : view)
        {
            Entity entity{ entityHandle, this };
            ScriptEngine::OnUpdateEntityScript(entity, dt);
        }
    }

    UpdateTransformHierarchy();
}
```

- **优点**
  - 直接对应 Scene.h 里的 `SceneState` 注释语义
  - Pause 状态下 Transform 层级仍更新，用户手动拖动实体（虽然 Pause 中用户交互场景本 Phase 未考虑，但保留能力）仍能刷新 WorldTransform
- **缺点**：无

#### 方案 B：`if (m_State != SceneState::Pause)` 内跑脚本 tick

- **优点**：语义等价（Pause 也跳过），语义"非 Pause 就跑"
- **缺点**
  - 与 `SceneState` 未来可能新增枚举值（例如 `PauseWhilePaused`、`SlowMotion` 等）耦合过紧
  - 显式列出 `Play` 更表达意图

#### 方案 C：Pause 与 Play 完全走两个不同的 Scene 方法

- **缺点**：Scene.h 已经定死 `OnUpdateRuntime` 一个入口，SceneManager 不做区分；变更这一契约不属于 P1.6 范围

**结论：方案 A**。

### 4.4 决策 4：`OnRuntimeStop` 里 `m_State` 与 `ScriptEngine::OnRuntimeStop` 的顺序

#### 方案 A：`ScriptEngine::OnRuntimeStop() → m_State = Edit`（**推荐 ?**）

```cpp
void Scene::OnRuntimeStop()
{
    ScriptEngine::OnRuntimeStop();
    m_State = SceneState::Edit;
}
```

- **优点**
  - 与 `OnRuntimeStart` 的顺序对称：Start 时先启脚本子系统再切 Play；Stop 时先停脚本子系统再切 Edit
  - `ScriptEngine::OnRuntimeStop` 内部会 `EntityInstances.clear()`??这一步会析构所有 `Ref<ScriptInstance>`，`ScriptInstance` 持有 mono 对象引用，此时 `SceneContext` 仍指向当前 Scene，理论上未来 `~ScriptInstance` 若要访问场景（例如触发 `OnDestroy` 托管钩子??Phase 3 目标）不会撞到空指针
- **缺点**：无

#### 方案 B：`m_State = Edit → ScriptEngine::OnRuntimeStop()`

- **缺点**
  - 与 Start 顺序不对称
  - 未来加 `OnDestroy` 时，`ScriptEngine::OnRuntimeStop` 需要"仍在 Play 状态"以便让 `OnDestroy` 里的托管代码正常访问场景

**结论：方案 A**。

### 4.5 决策 5：Scene.cpp 的 include 边界

`Scene.cpp` 现在没有 include `ScriptEngine.h`；`ScriptComponent.h` 则已经通过 [Components/Components.h](../../Lucky/Source/Lucky/Scene/Components/Components.h)（P1.4 已把它加入这个汇总头）**间接可用**??Scene.cpp 顶部第 10 行已 `#include "Components/Components.h"`。

#### 方案 A：只在 Scene.cpp 追加 `ScriptEngine.h`，`ScriptComponent` 通过既有的 `Components.h` 间接引入，Scene.h 完全不动（**推荐 ?**）

```cpp
// Scene.cpp 顶部新增一行 include
#include "Lucky/Scripting/ScriptEngine.h"
```

- **优点**
  - `Components/Components.h` 就是为"需要使用全部组件的地方"设计的汇总头，Scene.cpp 已经 include 它，`ScriptComponent` 已经可用，不需要单独重复 include
  - `Scene.h` 是被 `Entity.h` 间接拉进整个引擎的汇总头，mono 前向声明留在 `Scene.cpp` 内部即可
  - 与 P1.4 决策 4.2-A（`ScriptComponent.h` 不 include `ScriptEngine.h`）保持一致
  - 仅影响 `Scene.cpp` 一个 TU 的编译时间
- **缺点**：无

#### 方案 B：Scene.cpp 单独 include `ScriptComponent.h`

- **缺点**：与 `Components.h` 汇总头的设计意图相悖；重复 include 无收益

#### 方案 C：Scene.h 前向声明 `class ScriptEngine`

- **缺点**：Scene.h 里根本不出现 `ScriptEngine` 名字，前向声明无意义

**结论：方案 A**。

### 4.6 决策 6：脚本 tick 遍历的 view 类型

`Scene::OnUpdateRuntime` 每帧要遍历所有挂了 `ScriptComponent` 的实体。有多种 view 选择：

#### 方案 A：`m_Registry.view<ScriptComponent>()`，内部构造 `Entity` 后传给 `ScriptEngine::OnUpdateEntityScript`（**推荐 ?**）

```cpp
auto view = m_Registry.view<ScriptComponent>();
for (entt::entity entityHandle : view)
{
    Entity entity{ entityHandle, this };
    ScriptEngine::OnUpdateEntityScript(entity, dt);
}
```

- **优点**
  - `ScriptEngine::OnUpdateEntityScript(Entity, DeltaTime)` 签名接受 Entity，方案 A 天然贴合
  - 与 `OnRuntimeStart` 扫描用同一模式，两处代码结构一致
  - Entity 构造是零开销的（`Entity` 就是 `{entt::entity, Scene*}` 两个指针大小的值类型）
- **缺点**：`OnUpdateEntityScript` 内部会再调 `entity.GetUUID()`??每次 view 迭代多做一次 `Registry.get<IDComponent>`

#### 方案 B：`m_Registry.view<IDComponent, ScriptComponent>()`，一次 view 拿全 UUID + ClassName

```cpp
auto view = m_Registry.view<IDComponent, ScriptComponent>();
for (entt::entity entityHandle : view)
{
    auto [id, sc] = view.get<IDComponent, ScriptComponent>(entityHandle);
    // ...
}
```

- **优点**：entt 内部会做一次交集，遍历只涉及"同时挂了 IDComponent 与 ScriptComponent"的实体（IDComponent 每个实体都有，交集即全部 ScriptComponent 实体，等价于方案 A）；理论上 view 内部索引一次拿到两个组件，比方案 A 少一次 `Registry.get<IDComponent>` 查询
- **缺点**
  - `ScriptEngine::OnUpdateEntityScript(Entity, dt)` 签名接受 Entity，本方案拿到的 UUID 反而多余
  - 遍历次数 = 挂 `ScriptComponent` 的实体数，MVP 阶段绝对量极小（<=10），微弱性能差异完全不敏感
  - 代码更长

#### 方案 C：`ScriptEngine` 侧新增一个"遍历版" API：`ScriptEngine::UpdateAllEntities(Scene*, dt)`

- **优点**：Scene.cpp 更简洁（一行调用）
- **缺点**
  - `ScriptEngine::OnUpdateEntityScript(Entity, dt)` 已经存在且专业化，再加一层批量 API 是重复设计
  - Roadmap / P1.3 已定"脚本 tick 由 Scene 驱动，ScriptEngine 只提供单实体级 API"

**结论：方案 A**。

### 4.7 决策 7：`OnRuntimeStart` 里对未挂 `IDComponent` 实体的容错

`ScriptEngine::OnCreateEntityScript` 内部会调 `entity.GetUUID()`，而 `GetUUID` 依赖 `IDComponent`。理论上 `Scene::CreateEntity` 会给所有实体挂 `IDComponent`（P0.3 决策）；但 `entt::registry` 也允许通过底层 API 绕过 `Scene::CreateEntity` 直接创建实体（虽然本项目不这样用）。

#### 方案 A：不做防御性检查，相信"所有 Scene 里的实体都有 IDComponent"这一项目 invariant（**推荐 ?**）

- **优点**
  - 与项目其它遍历（`Scene::GetPrimaryCameraEntity` 等）保持一致的信任模型
  - 代码短
- **缺点**：无（invariant 违反是项目 bug，不该在此处兜底掩盖）

#### 方案 B：加 `if (!entity.HasComponent<IDComponent>()) continue`

- **缺点**：掩盖 invariant 违反的 bug，反而让问题晚暴露

**结论：方案 A**。

---

## 5. 实现步骤（按依赖顺序）

只改一个文件 [Scene.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp)，两个步骤：

### Step 1：`Scene.cpp` 顶部追加一个 include

Scene.cpp 顶部已经 include 了 `Components/Components.h`（第 10 行）??这是项目里"需要使用全部组件的地方"约定使用的汇总头，`ScriptComponent` 已经通过它间接可用。P1.6 只需要额外追加 `ScriptEngine.h`：

```cpp
#include "Lucky/Scripting/ScriptEngine.h"
```

**要点**：

- 按 `Coding_Style_Guide.md` §3.3 顺序：`#include "lcpch.h"` → 对应头 → 项目内部头 → 第三方 → 标准库；`ScriptEngine.h` 属项目内部头，追加到 Scene.cpp 既有内部头段落末尾即可
- 只在 `.cpp` 追加，`Scene.h` 完全不动（决策 4.5-A）
- **不要**单独 include `ScriptComponent.h`，与 `Components.h` 汇总头的设计意图相悖

### Step 2：重写 `OnRuntimeStart / OnUpdateRuntime / OnRuntimeStop` 三个方法

用以下**完整定稿代码**替换 [Scene.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp) 中同名的三个方法：

```cpp
    void Scene::OnUpdateRuntime(DeltaTime dt)
    {
        if (m_State == SceneState::Play)
        {
            auto view = m_Registry.view<ScriptComponent>();
            for (entt::entity entityHandle : view)
            {
                Entity entity{ entityHandle, this };
                ScriptEngine::OnUpdateEntityScript(entity, dt);
            }
        }

        UpdateTransformHierarchy();
    }

    void Scene::OnRuntimeStart()
    {
        ScriptEngine::OnRuntimeStart(this);

        auto view = m_Registry.view<ScriptComponent>();
        for (entt::entity entityHandle : view)
        {
            Entity entity{ entityHandle, this };
            const ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

            if (sc.ClassName.empty())
            {
                continue;
            }
            if (!ScriptEngine::EntityScriptClassExists(sc.ClassName))
            {
                LF_CORE_WARN("Scene::OnRuntimeStart - script class '{0}' not found for entity '{1}'", sc.ClassName, entity.GetName());
                continue;
            }

            ScriptEngine::OnCreateEntityScript(entity, sc.ClassName);
        }

        m_State = SceneState::Play;
    }

    void Scene::OnRuntimeStop()
    {
        ScriptEngine::OnRuntimeStop();
        m_State = SceneState::Edit;
    }
```

**要点**：

- `OnUpdateRuntime` 顺序：先脚本 tick（且仅 Play），再 Transform 层级更新（决策 4.1-A / 决策 4.3-A）
- `OnRuntimeStart` 顺序：`ScriptEngine::OnRuntimeStart` → 扫描实例化 → 切状态（决策 4.2-A）
- `OnRuntimeStop` 顺序：`ScriptEngine::OnRuntimeStop` → 切状态（决策 4.4-A）
- 所有循环、判断均使用花括号（`Coding_Style_Guide.md` §5.2）
- 使用 `entt::entity` 显式类型（局部变量类型简短，不用 `auto`，`Coding_Style_Guide.md` §13.9）
- `Entity entity{ entityHandle, this };` 花括号构造，避免 vexing parse
- `const ScriptComponent& sc = entity.GetComponent<ScriptComponent>();` 显式类型，符合项目"引用赋值明示类型"惯例
- 日志格式化 `{0} {1}` 编号占位符，与项目里既有 `LF_CORE_WARN / ERROR` 用法一致
- 不在代码里加任何"P1.6 补充"这类阶段性注释 [[memory:du4s18ic]]
- 不加"分层原则 / 未来会补充 OnDestroy"这类文档式注释 [[memory:ngdbzlni]]

### Step 3：编译验证

- Debug 编译 `Lucky` 静态库 → 通过
- Debug 编译 `Luck3DApp` → 通过
- 三个 configuration（Debug / Release / Dist）均通过

---

## 6. 疑点问答

### 6.1 为什么 `OnRuntimeStop` 里不显式清 `EntityInstances`？

答：`ScriptEngine::OnRuntimeStop()` 内部已经 `EntityInstances.clear()`。Scene 只是调用方，不重复清理。

### 6.2 Play 中间用户通过 Hierarchy 挂了新的 `ScriptComponent`，会怎样？

答：本 Phase 不处理这个路径。`Scene::OnComponentAdded<ScriptComponent>` 是空特化（P1.4 决策），新挂的组件不会被实例化；`OnUpdateRuntime` 遍历时会遇到这个实体，`ScriptEngine::OnUpdateEntityScript` 在 `EntityInstances` 里找不到 UUID → 静默跳过。语义上等价于"Play 期间新挂的脚本本次 Play 不生效"，与 Unity 早期版本行为一致。若要支持"即时生效"，Phase 3 再补。

### 6.3 Play 中间用户销毁了挂脚本的实体，`ScriptInstance` 会悬空吗？

答：会。`ScriptEngine::EntityInstances` 里的 `Ref<ScriptInstance>` 仍持有已销毁实体的 UUID key，但 `ScriptInstance` 内部只持有 `MonoObject*` 和方法句柄，不持有 `Entity`；下一帧 `OnUpdateEntityScript` 仍会调到这个悬空实例的 `Update`，`Update` 内部走 native 时会通过 `TryGetEntityWithUUID` 找不到实体→ScriptGlue 打 warn 并静默 return（P1.5 决策 6-B 的处理链路）。MVP 阶段可接受。Phase 3 再补精细化的实体销毁钩子。

### 6.4 `OnRuntimeStart` 里为什么不 `LF_CORE_INFO` 打一行 "started N scripts"？

答：`ScriptEngine::Init` 已经打过 `"initialized ({} user script classes loaded)"`；`OnCreateEntityScript` 内部若失败会 warn；每次 Play 都刷一行 INFO 是噪声。若确实需要，可在 Sandbox 脚本 `Awake` 内 `Debug.Log("Hello Luck3D")` 表达"我起来了"，正好也是 MVP 出口标准之一。

### 6.5 为什么 `OnUpdateRuntime` 里 view 遍历不需要显式 include `ScriptComponent.h`？entt view 只用类型作为 tag，能不能只用前向声明？

答：`m_Registry.view<ScriptComponent>()` 内部要求 `ScriptComponent` 是完整类型（entt 需要 `sizeof(T)` 与析构逻辑），必须有完整定义。**Scene.cpp 顶部已经 include 了 `Components/Components.h` 汇总头**，`ScriptComponent` 完整定义已经可见??不需要再单独 include。前向声明只能满足指针/引用场景，不满足 view 模板实例化。

### 6.6 `OnRuntimeStart` 里 `LF_CORE_WARN` 打日志时用 `entity.GetName()`，如果实体没挂 `NameComponent` 会怎样？

答：`Entity::GetName()` 内部 `GetComponent<NameComponent>().Name`，未挂时会触发 `LF_CORE_ASSERT`。但项目 invariant 是"`Scene::CreateEntity` 会同时挂 `IDComponent` 和 `NameComponent`"，与 IDComponent 同理（决策 4.7-A），不做防御。

### 6.7 `ScriptEngine::OnCreateEntityScript` 传的是 `entity` 值拷贝，会不会有生命周期问题？

答：`Entity` 是值类型（内部只有 `entt::entity` + `Scene*`），拷贝是零开销。`ScriptInstance` 构造函数只在构造期间用一次 `entity.GetUUID()`，之后不再持有 `Entity`，无生命周期问题。

### 6.8 Pause 状态下 `UpdateTransformHierarchy` 还跑吗？

答：跑。Scene.h `SceneState::Pause` 注释就是这么写的："仅保留 Transform 层级更新与渲染"。方案 4.3-A 的 `UpdateTransformHierarchy()` 无条件调用即是此语义。

### 6.9 Sandbox 脚本的方法名是 `Awake / Update` 还是 `OnCreate / OnUpdate`？

答：**`Awake / Update`**。P1.3 落地时 `ScriptInstance` 已经用了这两个名字（`GetMethod("Awake", 0)` / `GetMethod("Update", 1)`），Roadmap 里的 `OnCreate / OnUpdate` 是历史文本。P1.7 写用户脚本时必须用 `Awake / Update` 才能被脚本引擎正确 invoke。

### 6.10 `OnRuntimeStart` 扫描时如果场景里有 100 个 `ScriptComponent`，是否会卡顿？

答：`OnCreateEntityScript` 内部会调 `mono_object_new + mono_runtime_object_init + InvokeAwake`，单个实体 <1ms 量级。100 个实体 <100ms，Play 按钮点下后的一次性延迟，用户可接受。MVP 阶段不做优化。

---

## 7. 验收标准

**MVP 完全达成条件**（P1.6 完成即达成 Phase 1 出口）：

1. **编译通过**：`Lucky` 和 `Luck3DApp` 的 Debug / Release / Dist 三个 configuration 全通过

2. **Play 无脚本时的正确性**：场景中所有实体都没挂 `ScriptComponent` → 点击 Play → 无任何 mono 相关 warn / error；点击 Stop → 场景还原

3. **Play 有空 `ClassName` 时的正确性**：场景实体挂了 `ScriptComponent` 但 `ClassName == ""` → 点击 Play → 无 warn（空 ClassName 静默跳过）；Stop 后场景还原

4. **Play 有非法 `ClassName` 时的正确性**：`ClassName = "NotExisting.Foo"` → 点击 Play → 日志出现 `Scene::OnRuntimeStart - script class 'NotExisting.Foo' not found for entity '<实体名>'`；实体的其它组件正常，Stop 后场景还原

5. **Debug.Log 验收（MVP 出口之一）**：Sandbox 脚本 `Awake` 内 `Lucky.Debug.Log("Hello Luck3D")`；实体挂 `ScriptComponent` 且 `ClassName = "Sandbox.PlayerController"`；点击 Play → 日志出现 `[info] Hello Luck3D`（走 spdlog 输出即可，Console 面板重定向是 P3 目标）

6. **Cube 移动验收（MVP 出口之一）**：Sandbox 脚本 `Update(float dt)` 内 `Transform.Position += Vector3.Right * dt`（具体见 Roadmap 第 3.7 节）；Cube 挂脚本；点击 Play → Scene 面板与 Game 面板都看到 Cube 沿 +X 方向匀速移动；点击 Stop → Cube 瞬间回到原位（Scene::Copy 天然回滚能力）

7. **Pause 验收**：Play 中 Cube 匀速移动 → 点击 Pause → Cube 停在原地；再次点击 Pause 恢复 Play → Cube 从停下的位置继续移动；点击 Stop → Cube 瞬间回到最初位置

8. **Play 反复切换**：Play → Stop → Play → Stop 反复 10 次，无 mono 泄漏 warning（表现为日志里无渐增的告警数），Cube 每次 Play 都从原位开始

9. **Play 期间修改 Inspector**：Play 中通过 Inspector 修改 Cube 的 `Translation` 值，会立即在下一帧被脚本覆盖（`p.X += dt` 每帧覆盖）??这是符合预期的行为

10. **无 mono 报错日志**：整个 Play → Stop 过程日志中不出现 `mono_reflection_type_get_type` / `mono_string_to_utf8` 相关 assert；不出现 `ScriptGlue::` 系列 warn（除非用户故意在脚本里读写已销毁实体）

---

## 8. 对下一 Phase 的接线点

本 Phase 完成后，MVP 已达成。以下钩子已"数据+运行时双侧就绪、待增强"：

| 位置 | 后续 Phase | 打开方式 |
|------|-----------|---------|
| `Scene::OnComponentAdded<ScriptComponent>` | **Phase 3** | Play 状态下动态挂载脚本时即时实例化：判 `m_State == Play` 且 `EntityScriptClassExists` 后调 `ScriptEngine::OnCreateEntityScript` |
| Play 期间销毁挂脚本实体 | **Phase 3** | 在 `Scene::DestroyEntity` 里若 `m_State == Play` 且实体挂 `ScriptComponent`，调 `ScriptEngine::OnDestroyEntityScript(uuid)` 释放对应 `ScriptInstance` |
| 脚本 `OnDestroy` 托管钩子 | **Phase 3** | `ScriptInstance` 增加 `m_OnDestroyMethod`；`ScriptEngine::OnRuntimeStop` 遍历 `EntityInstances` 逐个 `InvokeOnDestroy` 后再 `.clear()` |
| 脚本抛异常的 try/catch | **Phase 3** | 包裹 `ScriptEngine::OnCreateEntityScript / OnUpdateEntityScript` 的 `mono_runtime_invoke`，`mono_runtime_invoke` 已经通过 `MonoObject** exc` 参数返回异常，只需读取并打日志 |
| Time.DeltaTime 全局 | **Phase 3** | 加 `Lucky.Time` 托管类；`ScriptEngine::OnUpdateEntityScript` 前设置 `Time` 静态字段（或走 InternalCall） |
| `Debug.Log` 重定向到 Console 面板 | **Phase 3** | `ScriptGlue::Debug_Log` 改成把消息写入 `Console::Push`；spdlog 与 Console 都收 |
| 脚本字段的动态 Inspector 编辑 | **Phase 2** | `ScriptComponent` 加 `FieldMap`；`Draw_Script` 按字段类型循环绘制 |

---

## 9. 变更清单速览

- **新增文件**：无
- **修改文件（1 个）**：
  - [Scene.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp)：顶部追加 `#include "Lucky/Scripting/ScriptEngine.h"`（`ScriptComponent.h` 已通过既有的 `Components/Components.h` 汇总头间接可用）；重写 `OnRuntimeStart / OnUpdateRuntime / OnRuntimeStop` 三个方法
- **删除**：无
- **不改动**：`Scene.h`、`ScriptEngine.h/.cpp`、`ScriptGlue.h/.cpp`、`ScriptComponent.h`、`SceneManager.cpp`、`EditorToolbar.cpp`、`EditorLayer.cpp`、premake 脚本

---

## 10. 参考

- Roadmap [ScriptSystem_Roadmap.md](ScriptSystem_Roadmap.md) 第 3 节 (6) "生命周期钩子接入 Scene::OnUpdateRuntime" 与 (7) Sandbox 脚本示例
- 前置详设 [Phase1.3_ScriptEngine_Runtime.md](Phase1.3_ScriptEngine_Runtime.md) §4.10（`ScriptInstance` 归属决策）与 §8（接线点表）
- 前置详设 [Phase1.4_ScriptComponent.md](Phase1.4_ScriptComponent.md) §6.2 / §6.3（空 ClassName 是正常中间态；`OnComponentAdded<ScriptComponent>` 保持空特化）
- 前置详设 [Phase1.5_ScriptGlue.md](Phase1.5_ScriptGlue.md) 决策 6（无效 Entity / 未注册类型的守卫策略）
- 代码规范 [Coding_Style_Guide.md](../Coding_Style_Guide.md) 第 3.3 节（include 顺序）、第 5.2 节（花括号）、第 13.9 节（auto 使用规范）
