# Phase 0.3：Scene::Copy 深拷贝

## 1. 概述

P0.3 目标：给 `Scene` 引入 **`static Ref<Scene> Copy(const Ref<Scene>& other)`** 深拷贝能力，作为 P0.5 全局 Play/Stop 工具条与 Phase 1 脚本 Runtime 的公共前置。

具体产出：

1. `Scene::Copy` 静态方法：给定源 Scene，返回一份与之**结构等价、组件值语义拷贝、共享资产引用**的新 Scene 实例
2. 私有 helper 模板 `CopyComponentIfExists<...>`：按硬编码的组件列表遍历 registry 逐类型拷贝
3. 明确 Copy 语义边界：拷贝什么、共享什么、跳过什么，并在文档中沉淀"最容易踩的坑"

### 前置依赖

- P0.1 完成（`SceneState`、`OnRuntimeStart/Stop`、`OnUpdateEditor/Runtime`、`OnRenderEditor/Runtime` 已就位）
- P0.2 完成（`CameraComponent` 已在 `ComponentType` 与 `Components.h` 内注册，Copy 时可作为组件列表的一员）

### 本 Phase **不做**的事

- **不做 Play/Stop 按钮接线**（P0.5）?? Copy 出来的新 Scene 由谁调用、如何切换 ActiveScene，全部留给 P0.5
- **不引入 ComponentRegistry**（P0.4 可选前置，本 Phase 走硬编码组件列表，Copy 的唯一消费者是 Play/Stop，值不到抽象成本）
- **不做 Material / Mesh / Texture 的深拷贝**：资产层面的共享是全项目共识，Play 期间通过脚本改写 Material/Texture 属性会残留到编辑态，作为已知短期不完美记录，见【决策点 5】
- **不做 SelectionManager 的额外备份**：SelectionManager 存的是 UUID，Copy 保持 UUID 不变，跨 Copy 天然保留
- **不动 SceneSerializer**：Copy 走内存路径，不经序列化

---

## 2. 涉及的文件

### 需要修改

| 文件 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Scene/Scene.h` | 新增 `static Ref<Scene> Copy(const Ref<Scene>& other);` 声明 |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | 实现 `Copy`，并在匿名命名空间或私有静态位置放 `CopyComponentIfExists<...>` helper |

### 无需新建文件

Copy 的实现规模在 60~80 行内，helper 直接放在 `Scene.cpp` 顶部匿名命名空间，避免过早引入 `SceneUtils.h` / `ComponentCopy.h` 等新头文件。

---

## 3. 现状回顾

### 3.1 Scene 内可变状态的完整清单

对着 [Scene.h](../../Lucky/Source/Lucky/Scene/Scene.h) 通读 `class Scene` 的私有字段：

| 字段 | 类型 | Copy 时如何处理 |
|------|------|-----------------|
| `m_EntityIDMap` | `std::unordered_map<UUID, Entity>` | 由 `CreateEntity(uuid, name)` 逐个重建，不能直接 memcpy（Entity 内含 `Scene*` 指针，指向源 Scene） |
| `m_RootEntityOrder` | `std::vector<UUID>` | 建完实体后**整块覆盖**源的顺序 |
| `m_Registry` | `entt::registry` | 逐组件类型 `emplace_or_replace` 值拷贝到新 registry |
| `m_ViewportWidth / m_ViewportHeight` | `uint32_t` | 直接赋值 |
| `m_State` | `SceneState` | **强制置 `Edit`**（副本是"干净快照"，是否进入 Play 由调用方决定，见【决策点 2】） |
| `m_EnvironmentSettings` | `EnvironmentSettings` | 直接赋值（值类型 struct，其中 `SkyboxMaterial` 是 `Ref<Material>`，共享，见【决策点 5】） |

以及 `Asset` 基类的 Name / Handle：

- `Name`：通过新 Scene 的 ctor 参数 `other->GetName()` 传入，直接赋值
- `Handle`：**副本 Handle 无效**（`CreateRef<Scene>` 默认状态），副本永不落盘、不进 `AssetManager` 缓存，见【决策点 3】

### 3.2 现有组件枚举总数

对照 [Components.h](../../Lucky/Source/Lucky/Scene/Components/Components.h)：

| 组件 | Copy 列表 | 说明 |
|------|:--------:|------|
| `IDComponent` | ? | 由 `CreateEntity(uuid, name)` 直接建，不能重复添加 |
| `NameComponent` | ?（可选） | `CreateEntity(uuid, name)` 已带 Name；为对称性和未来扩展（Tag / Layer 等字段），建议进 Copy 列表覆盖 |
| `TransformComponent` | ? | |
| `RelationshipComponent` | ? | 内部字段全部是 UUID，值语义拷贝后父子关系天然复原 |
| `MeshFilterComponent` | ? | |
| `MeshRendererComponent` | ? | Materials 是 `std::vector<Ref<Material>>`，值拷贝后共享 Material |
| `SpriteRendererComponent` | ? | Texture / Material 同样是 `Ref`，共享 |
| `LightComponent` | ? | |
| `PostProcessVolumeComponent` | ? | |
| `CameraComponent` | ? | 内含 `SceneCamera`（投影参数 + `ViewportSize`），值拷贝后 Aspect / FOV / Near / Far 完整保留 |

Copy 列表最终共 **9 个组件类型**（Name + 8 个业务组件）。

### 3.3 现有 `CreateEntity(UUID uuid, const std::string& name)` 的行为

`Scene::CreateEntity(UUID, const std::string&)` 会：
1. 在 `m_Registry` 中 `create` 一个新 entt::entity
2. 添加 `IDComponent(uuid)`、`NameComponent(name)`、默认 `TransformComponent`、默认 `RelationshipComponent`
3. 写入 `m_EntityIDMap[uuid]`
4. **无脑追加**到 `m_RootEntityOrder` 末尾

这意味着 Copy 使用 `CreateEntity(uuid, name)` 建实体时：
- ? UUID / Name / 4 个默认组件的**存在性**已到位
- ? `m_EntityIDMap` 会被正确重建
- ? `m_RootEntityOrder` 顺序错乱 → Step D 用源的顺序整块覆盖修复
- ? Transform / Relationship 是默认值 → Step C 用源的值覆盖

### 3.4 `OnComponentAdded` 特化对 Copy 的影响

对照 [Scene.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp) 的 `OnComponentAdded<T>` 特化：

| 组件 | `OnComponentAdded` 做了什么 | Copy 是否要跑一遍 |
|------|-----------------------------|:----------------:|
| `IDComponent / NameComponent / TransformComponent / RelationshipComponent / MeshFilterComponent / LightComponent / PostProcessVolumeComponent` | 空实现 | 跑不跑无区别 |
| `MeshRendererComponent` | 根据 MeshFilter 的 SubMesh 数量重置 `Materials` 长度，缺失位置填默认材质 | **不能跑**：源已经稳定，用户可能手动改过 Materials 长度或替换过特定 index 的材质，重跑会破坏 |
| `SpriteRendererComponent` | 若 `Material` 为空则填默认 2D 材质 | 跑不跑视源状态而定；源如果已经填过默认值，跑一遍是幂等的；但为了统一，一律不跑 |
| `CameraComponent` | 若视口宽高有效则 `SetViewportSize` | **不能跑**：会强制把 aspect 拉回视口 aspect，若源的 `FixedAspectRatio=true` 且有自定义 aspect，值会被覆盖 |

**结论**：Copy 路径下**一律不触发 `OnComponentAdded`**，用 `m_Registry.emplace_or_replace<T>()` 直接写值。**这是本 Phase 最容易踩的坑，必须在实现里显式绕开 `Entity::AddComponent` / `Entity::AddOrReplaceComponent`**（这两个方法都会触发特化回调）。

---

## 4. 详细设计

### 4.1 Scene.h 新增声明

在 [Scene.h](../../Lucky/Source/Lucky/Scene/Scene.h) 的 `class Scene` 中，**放在 `CreateEntity` / `DestroyEntity` 声明附近**（同属"实体生命周期"语义）：

```cpp
/// <summary>
/// 深拷贝一份 Scene 副本
/// 
/// 语义边界：
/// - 组件：值语义完整拷贝（新 registry 不与源共享任何组件存储）
/// - 资产引用（Mesh / Material / Texture / SkyboxMaterial 等 Ref&lt;Asset&gt;）：共享，副本与源指向同一份资产
/// - UUID / RootEntityOrder / EnvironmentSettings / ViewportSize / Name：完整拷贝
/// - Asset Handle：副本为无效 Handle（副本不进入 AssetRegistry）
/// - SceneState：副本一律初始为 Edit，是否进入 Play 由调用方决定
/// 
/// 副本不会触发任何 OnComponentAdded 回调
/// </summary>
/// <param name="other">源场景（不为空）</param>
/// <returns>与源等价的新 Scene 实例</returns>
static Ref<Scene> Copy(const Ref<Scene>& other);
```

### 4.2 Scene.cpp 实现

#### 4.2.1 helper：CopyComponentIfExists

放在 `Scene.cpp` 顶部匿名命名空间（紧跟 `#include` 之后）：

```cpp
namespace
{
    template<typename TComponent>
    void CopyComponentIfExists(entt::registry& dst, entt::entity dstEntity,
                               entt::registry& src, entt::entity srcEntity)
    {
        if (src.has<TComponent>(srcEntity))
        {
            const TComponent& srcComp = src.get<TComponent>(srcEntity);
            dst.emplace_or_replace<TComponent>(dstEntity, srcComp);
        }
    }

    template<typename... TComponent>
    void CopyComponents(entt::registry& dst, entt::entity dstEntity,
                        entt::registry& src, entt::entity srcEntity)
    {
        (CopyComponentIfExists<TComponent>(dst, dstEntity, src, srcEntity), ...);
    }
}
```

要点：
- 用 C++17 折叠表达式 `(f<Ts>(...), ...)` 展开变参
- **直接走 `m_Registry.emplace_or_replace`，绕过 `Entity::AddComponent` → 不触发 `OnComponentAdded` 特化**
- 用 `entt::registry::has` 与 `entt::registry::get`，与项目现有 `Entity::HasComponent` / `Entity::GetComponent` 底层一致（当前 entt 版本用的是 `has` 而非 `all_of`，见 [Entity.h](../../Lucky/Source/Lucky/Scene/Entity.h)）

#### 4.2.2 Scene::Copy 主体

```cpp
Ref<Scene> Scene::Copy(const Ref<Scene>& other)
{
    LF_CORE_ASSERT(other, "Scene::Copy - source scene must not be null");

    Ref<Scene> newScene = CreateRef<Scene>(other->GetName());

    // ---- Step A：场景级字段 ----
    newScene->m_ViewportWidth = other->m_ViewportWidth;
    newScene->m_ViewportHeight = other->m_ViewportHeight;
    newScene->m_EnvironmentSettings = other->m_EnvironmentSettings;
    // m_State 保持默认 Edit：副本不继承源的运行状态

    entt::registry& srcRegistry = other->m_Registry;
    entt::registry& dstRegistry = newScene->m_Registry;

    // ---- Step B：先按 UUID 建实体（不拷组件） ----
    // 建完后 m_EntityIDMap 就位，m_RootEntityOrder 顺序会乱，稍后由 Step D 修正
    auto idView = srcRegistry.view<IDComponent>();
    for (auto srcEntity : idView)
    {
        UUID uuid = srcRegistry.get<IDComponent>(srcEntity).ID;
        const std::string& name = srcRegistry.get<NameComponent>(srcEntity).Name;
        newScene->CreateEntity(uuid, name);
    }

    // ---- Step C：逐组件类型值拷贝 ----
    // 不含 IDComponent（Step B 已建）；含 NameComponent（覆盖 CreateEntity 传入的 name，且为未来 Tag/Layer 扩展留位）
    // 不走 Entity::AddComponent，避免触发 OnComponentAdded 特化对副本"再初始化"
    for (auto srcEntity : idView)
    {
        UUID uuid = srcRegistry.get<IDComponent>(srcEntity).ID;
        entt::entity dstEntity = static_cast<entt::entity>(newScene->m_EntityIDMap.at(uuid));

        CopyComponents<
            NameComponent,
            TransformComponent,
            RelationshipComponent,
            MeshFilterComponent,
            MeshRendererComponent,
            SpriteRendererComponent,
            LightComponent,
            PostProcessVolumeComponent,
            CameraComponent
        >(dstRegistry, dstEntity, srcRegistry, srcEntity);
    }

    // ---- Step D：重建根节点顺序 ----
    newScene->m_RootEntityOrder = other->m_RootEntityOrder;

    return newScene;
}
```

关于 `dstEntity` 的获取：`Entity` 有 `operator entt::entity() const`，`m_EntityIDMap.at(uuid)` 返回 `Entity`，隐式转换为 `entt::entity` 即可。上面写成 `static_cast` 是为了显式表明意图，也可以直接用赋值（等价）。

#### 4.2.3 include 补充

`Scene.cpp` 顶部原本已经 `#include "Components/Components.h"`，Components.h 中已聚合了所有组件头，因此 helper 中使用的所有组件类型都已可见，**无需额外 include**。

### 4.3 Copy 之后的等价关系保证

设 `copy = Scene::Copy(src)`，则应满足：

| 属性 | 关系 |
|------|------|
| `copy->GetAllEntitiesWith<IDComponent>()` 数量 | == `src` 的数量 |
| 每个 UUID | 在 `copy->m_EntityIDMap` 与 `src->m_EntityIDMap` 中都存在 |
| `copy->GetEntityWithUUID(u).GetParent().GetUUID()` | == `src->GetEntityWithUUID(u).GetParent().GetUUID()`（父子关系由 RelationshipComponent 的 UUID 值决定） |
| `copy->GetEntityWithUUID(u).GetChildren()` | 内容 == `src->GetEntityWithUUID(u).GetChildren()` |
| `copy->m_RootEntityOrder` | == `src->m_RootEntityOrder` |
| `copy->GetPrimaryCameraEntity().GetUUID()` | == `src->GetPrimaryCameraEntity().GetUUID()` |
| 修改 `copy` 的任意组件字段 | **不影响** `src` 对应组件（值语义） |
| `copy` MeshFilter 的 `Mesh` 指针 | == `src` 的对应指针（`Ref` 引用计数 +1，资产共享） |
| `copy->GetState()` | == `SceneState::Edit`，**与源无关** |
| `copy->GetHandle()` | 无效 Handle |

---

## 5. 关键决策点与方案对比

### 5.1 【决策点 1】组件枚举策略：硬编码模板列表 vs ComponentRegistry

Copy 需要遍历"所有已注册的业务组件"。

#### 方案 A：硬编码模板参数包（**推荐 ★★★**）

```cpp
CopyComponents<
    NameComponent, TransformComponent, RelationshipComponent,
    MeshFilterComponent, MeshRendererComponent, SpriteRendererComponent,
    LightComponent, PostProcessVolumeComponent, CameraComponent
>(dstRegistry, dstEntity, srcRegistry, srcEntity);
```

- **优点**
  - 实现简洁，60~80 行完成全部 Copy
  - 无新增头文件、无新增基础设施
  - 类型安全，编译期展开、零运行时开销
  - 与项目当前 `Components.h` 里"逐类型 ComponentTrait 特化"的模式一致
- **缺点**
  - 每次新增业务组件时需要同步这一行模板参数
  - 与 `SceneSerializer` 中同样"逐组件 if" 的重复问题并存 ?? 但那是 P0.4 或后续 Registry 重构的事

#### 方案 B：先做 P0.4 ComponentRegistry，Copy 走注册表

在 `Scene::Copy` 内 `for (auto& desc : ComponentRegistry::All()) desc.Copy(src, dst, ...);`

- **优点**
  - 新增组件时不用改 Copy 代码
  - Serializer / Inspector / Copy 可共享一套 Registry，一处注册处处生效
- **缺点**
  - **过早抽象**：当前 Copy 只有一个消费者（P0.5），Serializer 里的"逐组件 if" 也没被这个 Phase 归并
  - Registry 的接口设计本身是个大议题（如何统一 Copy / Serialize / Deserialize / Inspector Draw 四种不同签名？），会把 P0.3 的范围膨胀到 P0.4
  - 违反"YAGNI"，也违反 Roadmap "P0.4 可选前置"的定位

#### 方案 C：利用 `ComponentTrait` + entt 类型擦除做运行时列表

维护 `std::vector<std::function<void(entt::registry&, entt::entity, entt::registry&, entt::entity)>>` 存 Copy 回调，Scene 内注册。

- **优点**：新增组件只在一处注册
- **缺点**
  - 引入 `std::function` 的运行时开销
  - 依然需要在某处集中注册（相当于把硬编码列表挪了个位置）
  - Registry 的完整版本才有意义（方案 B）

**结论**：采用**方案 A**。理由：只有一个消费者，Registry 抽象等到 Phase 1 引入 ScriptComponent 时（第二个消费者出现）再一并处理，收益更大。

### 5.2 【决策点 2】副本的 `m_State` 初值：`Edit` vs 保持源值

Copy 出来的新 Scene，`m_State` 应该继承源还是重置？

#### 方案 A：强制置 `Edit`（**推荐 ★★★**）

- **优点**
  - 语义清晰：Copy 是"数据快照"，运行状态是"下一步谁调用谁负责"
  - P0.5 的 Play 流程明确会走 `runtimeScene->OnRuntimeStart()` 显式切到 Play
  - 避免在 "Copy 一个 Play 中的场景做时间倒流" 等未定义场景踩坑
- **缺点**
  - 需要在 `Copy` 内显式写一行（其实新 Scene 构造后默认就是 Edit，"什么都不做"就是 Edit）

#### 方案 B：保持源值（`newScene->m_State = other->m_State;`）

- **优点**
  - "副本 == 源"的对称性更强
- **缺点**
  - 若源正处于 Play，副本无 `OnRuntimeStart` 的初始化过程，直接置为 Play 会在未来引入脚本 / 物理时出现"实例未创建但状态是 Play"的悬空
  - Play/Stop 场景中，副本要么用于 Runtime（P0.5 会主动调 OnRuntimeStart），要么用于备份（P0.5 会保持 Edit），两种情况都不希望 Copy 自己去继承 State

**结论**：采用**方案 A**。

### 5.3 【决策点 3】副本 Asset Handle 的处理

`Scene` 继承自 `Asset`，含 `AssetHandle`。副本要不要保留源 Handle？

#### 方案 A：副本 Handle 无效（`CreateRef<Scene>` 默认状态）（**推荐 ★★★**）

- **优点**
  - Runtime Scene 是"进程内活对象"，不属于资产系统，无需注册到 `AssetManager`
  - 保存快捷键（`Ctrl+S`）在 Play 状态下不会误把 RuntimeScene 写回 EditorScene 的路径
  - 避免"同一个 Handle 在 AssetRegistry 里指向两个不同的 Scene 对象"这类歧义
- **缺点**
  - 若未来出现"想通过 Handle 反查 RuntimeScene 是从哪个 EditorScene 复制来的"的需求，需要新加机制 ?? 目前无此需求

#### 方案 B：保留源 Handle

- **优点**
  - Runtime 中"我从哪来"信息完整
- **缺点**
  - AssetManager 里 handle 唯一性被破坏，或需要为 Runtime Scene 单独绕开注册
  - 保存路径歧义（见方案 A 优点第 2 条）

**结论**：采用**方案 A**。副本永不落盘、不进注册表。

### 5.4 【决策点 4】NameComponent 是否进入 Copy 列表

`CreateEntity(uuid, name)` 已经在 Step B 传入了 name，Step C 再拷一次是否冗余？

#### 方案 A：进 Copy 列表（**推荐 ★★★**）

- **优点**
  - 与其他组件对称，未来给 NameComponent 加字段（如 Tag / Layer / IsActive）时不需要改 Copy 逻辑
  - 组件如果由多字段构成，`CreateEntity` 只传了 name，其他字段会漏拷
- **缺点**
  - Step C 里会对 NameComponent 做一次冗余 `emplace_or_replace`（性能忽略不计）

#### 方案 B：不进 Copy 列表

- **优点**：省一次 emplace
- **缺点**：未来扩展 NameComponent 时容易漏拷

**结论**：采用**方案 A**。

### 5.5 【决策点 5】`EnvironmentSettings.SkyboxMaterial` 的共享 vs 克隆

`EnvironmentSettings` 是值类型 struct，但内部 `SkyboxMaterial` 是 `Ref<Material>`。Copy 后两个 Scene 共享同一份 Material。

#### 方案 A：接受共享语义（**推荐 ★★★**）

- **优点**
  - 与 Mesh / Texture 一致：整个引擎对资产的约定就是"通过 `Ref` 共享"
  - 实现零成本
  - Play 期间修改 Skybox 参数生效在两份 Scene 上，视觉表现完全一致
- **缺点**
  - **已知短期不完美**：Play 中通过脚本改写 SkyboxMaterial 的参数会残留到编辑态。目前 Skybox 编辑主要靠 Inspector，脚本层几乎不会碰到它，风险极低
  - 未来若真出现这类问题，可单独立项做 Material clone（是跨系统议题，不属于 Copy 范畴）

#### 方案 B：Copy 时深克隆 SkyboxMaterial

- **优点**：完美还原
- **缺点**
  - `Material::Clone()` 目前不存在，需要新加接口
  - Material 深拷贝涉及 Shader 引用、Uniform 值、Texture 引用等多层次决策，是独立议题
  - **超出 P0.3 范围**

**结论**：采用**方案 A**。作为已知短期不完美，在文档【7 节 后续接入点预告】中列出。

### 5.6 【决策点 6】helper 放置位置

`CopyComponentIfExists<T>` 应该放哪？

#### 方案 A：`Scene.cpp` 顶部匿名命名空间（**推荐 ★★★**）

- **优点**
  - 生命周期与用途完全绑定 Scene，无外泄需求
  - 无新增头文件
  - 与项目里其他仅供内部使用的辅助函数风格一致
- **缺点**
  - 若未来 SceneSerializer 也想复用，需要迁移 ?? 但那正是 P0.4 Registry 的工作，届时会连同接口一起重构

#### 方案 B：新建 `ComponentCopy.h` 独立头文件

- **优点**：可复用
- **缺点**：给一个仅在一个 .cpp 内使用的模板建独立头文件属于过度组织

#### 方案 C：作为 `Scene` 的私有静态模板方法

- **优点**：形式上"属于 Scene"
- **缺点**：模板必须在头文件展开，把这些实现细节暴露到 `Scene.h`，增加编译依赖

**结论**：采用**方案 A**。

### 5.7 【决策点 7】拷贝时的实体遍历入口：`view<IDComponent>` vs `each`

Step B/C 需要遍历源 registry 的所有实体。

#### 方案 A：`srcRegistry.view<IDComponent>()`（**推荐 ★★★**）

- **优点**
  - 每个实体都有 IDComponent（`CreateEntity` 保证），view 遍历天然覆盖全量
  - 顺便一次拿到 UUID，无需再查
  - 与项目其他地方（`GetAllEntitiesWith<...>`）风格一致
- **缺点**
  - 依赖"每个实体都有 IDComponent"的不变量（当前项目保证此不变量）

#### 方案 B：`srcRegistry.each([&](entt::entity e) { ... })`

- **优点**：不依赖 IDComponent 存在
- **缺点**
  - 需要在回调内再 `has<IDComponent>` 兜底判断
  - entt 不同版本 `each` 签名有差异，不如 view 稳

**结论**：采用**方案 A**。

---

## 6. 验收标准

本 Phase 完成后应满足：

1. **编译通过**：`Scene.h` / `Scene.cpp` 修改后项目正常编译
2. **零回归**：`Scene::Copy` 未被任何生产路径调用（P0.5 才接线），编辑器运行行为与改造前完全一致
3. **手动验证脚本**：在 `EditorLayer::UI_DrawMenuBar` 或某个测试菜单项中临时加入以下代码进行验证，验证完删除：
   ```cpp
   if (ImGui::MenuItem("[Debug] Test Scene::Copy"))
   {
       Ref<Scene> src = SceneManager::GetActiveScene();
       Ref<Scene> copy = Scene::Copy(src);
       SceneManager::SetActiveScene(copy);
   }
   ```
   点击后应满足：
   - **T1 结构一致**：Hierarchy 面板显示的实体列表、层级结构、根节点顺序与原来完全一致
   - **T2 渲染一致**：Scene 面板画面像素级不变（相机、光照、Mesh、Sprite、Sky、后处理全部保持）
   - **T3 Inspector 一致**：选中任意实体，Inspector 显示的所有组件字段值与切换前一致
   - **T4 独立性**：修改副本上某个 Cube 的 Translation，源 Scene 中的对应 Cube 位置不变（需临时保存源 Ref 并切回验证；或在断点中观察）
   - **T5 资产共享**：副本上某个 MeshRenderer 的 `Materials[0]` 指针 == 源上对应指针（`Ref` 引用计数上升，同一份 Material 内存）
   - **T6 状态复位**：`copy->GetState() == SceneState::Edit`
   - **T7 Handle 空**：`copy->GetHandle().IsValid() == false`
4. **主相机可用**：临时把 `SceneViewportPanel::OnUpdate` 中 `m_Scene->OnRenderEditor(m_EditorCamera)` 改为 `m_Scene->OnRenderRuntime()`（在 Copy 后的场景上），Game 视角画面正常
5. **无副作用日志**：Copy 期间控制台不应出现任何 `LF_CORE_ERROR` / `LF_CORE_WARN`

---

## 7. 后续 Phase 的接入点预告

| 位置 | 后续 Phase | 会做什么 |
|------|-----------|---------|
| `Scene::Copy` 的调用方 | P0.5 | Play 按钮：`runtimeScene = Scene::Copy(editorScene); SceneManager::SetActiveScene(runtimeScene); runtimeScene->OnRuntimeStart();`；Stop 按钮：`SceneManager::SetActiveScene(editorScene); runtimeScene 释放`  |
| `EditorLayer` 内部 | P0.5 | 新增 `Ref<Scene> m_EditorScene` 成员用于保存 Play 前的原始场景（"editorScene = ActiveScene ? m_EditorScene = editorScene"） |
| Copy 列表 | Phase 1 | 新增 `ScriptComponent` 后追加到 `CopyComponents<...>` 参数包中 |
| Material clone | 视需求 | 若脚本层出现"修改 SkyboxMaterial 参数在 Stop 后仍残留"的问题，独立立项处理 Material 深拷贝 |
| Registry 化 | P0.4 或 Phase 1 | 当 Serializer / Copy / Inspector 三处逐组件 if 变得难维护时，统一抽象为 `ComponentRegistry` |

### 7.1 已知短期不完美

| 项 | 描述 | 影响 | 何时解决 |
|----|------|------|----------|
| SkyboxMaterial 共享 | 副本与源共享 `Ref<Material>`，Play 中改写会残留 | 目前 Skybox 只在 Inspector 编辑，脚本层不碰，风险极低 | 有实际问题时独立立项 |

---

## 8. 变更清单速览

- **新增**
  - `Scene.h`：`static Ref<Scene> Copy(const Ref<Scene>& other);` 声明
  - `Scene.cpp` 匿名命名空间：`CopyComponentIfExists<TComponent>` + `CopyComponents<TComponent...>` helper
  - `Scene.cpp`：`Scene::Copy` 实现
- **修改**
  - 无
- **删除**
  - 无

---

## 9. 与后续 Phase 的关系

本 Phase 的产出仅有一个静态方法 `Scene::Copy`，本身不改变任何运行时行为（未被生产代码调用）。它的价值完全兑现于：

| Phase | 兑现方式 |
|-------|---------|
| P0.5 全局 Play/Stop 工具条 | Play 时用 `Scene::Copy` 生成 RuntimeScene，Stop 时丢弃 RuntimeScene 恢复 EditorScene |
| Phase 1 MVP | Play → Cube 移动 → Stop 后位置还原，恰恰依赖 P0.5 + P0.3 组合完成"运行状态与编辑数据隔离" |

因此本 Phase 是一个"纯基础设施" Phase，验收标准以"手动切换 ActiveScene 后所有面板表现一致"为核心。
