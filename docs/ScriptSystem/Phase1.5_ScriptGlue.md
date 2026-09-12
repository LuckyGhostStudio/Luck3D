# Phase 1.5：ScriptGlue（打通 C# → C++ 的 InternalCall 桥）

## 1. 概述

P1.5 目标：把 P1.2 在 [InternalCalls.cs](../../Lucky-ScriptCore/Source/Lucky/InternalCalls.cs) 里声明好的 **6 个** `[MethodImpl(MethodImplOptions.InternalCall)]` 桩函数在 C++ 侧实现，并通过 `mono_add_internal_call` 注册到 Mono JIT，使托管代码里的 `Lucky.Debug.Log("...")`、`transform.Position = ...` 能真正落到 native。同时建立 `MonoType* → HasComponent<T>` 的分发表，让 `Entity.HasComponent<T>()` 泛型接口在 native 侧可以按类型分发。

**本 Phase 只做"桥"**：注册跳转表 + 6 个 native 函数体。不驱动脚本 tick（那是 P1.6），不写用户脚本（那是 P1.7）。

### 1.1 关键约束

- **文件位置**：`Lucky/Source/Lucky/Scripting/ScriptGlue.h` / `.cpp`（与 `ScriptEngine` 同目录）
- **命名对齐**：`mono_add_internal_call` 传入的第一个参数字符串**必须**与 [InternalCalls.cs](../../Lucky-ScriptCore/Source/Lucky/InternalCalls.cs) 中的 `Lucky.InternalCalls::Xxx_Yyy` 完全一致，否则 mono 找不到映射会静默失败或运行期抛 `MissingMethodException`
- **调用时机**：`ScriptGlue::RegisterFunctions()` 只依赖 `InitMono()`；`ScriptGlue::RegisterComponents()` 依赖 `LoadCoreAssembly()` 已完成（要拿 CoreAssemblyImage 反射 `Lucky.TransformComponent`）
- **零头污染**：所有 mono 头（`<mono/metadata/object.h>`、`<mono/metadata/reflection.h>`）只在 `ScriptGlue.cpp` 内 include，`ScriptGlue.h` 保持零 mono 依赖

### 1.2 前置条件

- P1.1 已完成：Vendor/mono 依赖就位、`mono_add_internal_call` / `mono_string_to_utf8` / `mono_reflection_type_from_name` / `mono_reflection_type_get_type` 可链接
- P1.2 已完成：[InternalCalls.cs](../../Lucky-ScriptCore/Source/Lucky/InternalCalls.cs) 声明了 6 个 InternalCall 桩；[Debug.cs](../../Lucky-ScriptCore/Source/Lucky/Debug.cs)、[Component.cs](../../Lucky-ScriptCore/Source/Lucky/Component.cs)、[Entity.cs](../../Lucky-ScriptCore/Source/Lucky/Entity.cs)、[Vector3.cs](../../Lucky-ScriptCore/Source/Lucky/Vector3.cs) 已就位
- P1.3 已完成：`ScriptEngine::Init` 已能加载 Core / App 程序集；`ScriptEngine::GetCoreAssemblyImage()` 和 `ScriptEngine::GetSceneContext()` 已在 [ScriptEngine.h](../../Lucky/Source/Lucky/Scripting/ScriptEngine.h) 中声明
- P1.4 已完成：`ScriptComponent` 已作为数据组件存在（本 Phase 不 include 它）
- [Scene.h](../../Lucky/Source/Lucky/Scene/Scene.h) 已提供 `Scene::TryGetEntityWithUUID(UUID)`，供 native 端做 UUID → Entity 查找

### 1.3 本 Phase **不做**的事

- 不实现 `Input_*`、`Time_*`、`Rigidbody_*` 等 InternalCall（Phase 3/4）
- 不打开 [ScriptEngine.cpp](../../Lucky/Source/Lucky/Scripting/ScriptEngine.cpp) `OnCreateEntityScript` 里剩余的注释分支（P1.6）
- 不在 `Scene::OnRuntimeStart/OnUpdateRuntime` 里驱动 `ScriptComponent`（P1.6）
- 不改 [InternalCalls.cs](../../Lucky-ScriptCore/Source/Lucky/InternalCalls.cs)（签名已固化）
- 不为其它组件（`CameraComponent` / `MeshRendererComponent` / `LightComponent` / …）注册 `HasComponent` 分发??它们在托管侧尚无对应 C# 类
- 不做异步日志改造（当前 spdlog 是同步 sink，字符串拷一次防未来切异步足够）

---

## 2. 涉及的文件

### 2.1 新建

| 路径 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Scripting/ScriptGlue.h` | 声明 `class ScriptGlue`，提供 `RegisterFunctions / RegisterComponents` 两个静态方法 |
| `Lucky/Source/Lucky/Scripting/ScriptGlue.cpp` | 6 个 native 函数体 + `MonoType*→HasComponent` 映射 + `RegisterComponent<T>` 模板 + `LF_ADD_INTERNAL_CALL` 局部宏 |

### 2.2 修改

| 文件 | 改动 |
|------|------|
| [ScriptEngine.cpp](../../Lucky/Source/Lucky/Scripting/ScriptEngine.cpp) | 顶部 `#include "ScriptGlue.h"`；`Init()` 末尾追加 `ScriptGlue::RegisterFunctions()` 与 `ScriptGlue::RegisterComponents()` 两行调用 |

### 2.3 不修改

- [InternalCalls.cs](../../Lucky-ScriptCore/Source/Lucky/InternalCalls.cs)：6 个签名 P1.2 已固化
- [Debug.cs](../../Lucky-ScriptCore/Source/Lucky/Debug.cs)、[Component.cs](../../Lucky-ScriptCore/Source/Lucky/Component.cs)、[Entity.cs](../../Lucky-ScriptCore/Source/Lucky/Entity.cs)、[Vector3.cs](../../Lucky-ScriptCore/Source/Lucky/Vector3.cs)
- [ScriptEngine.h](../../Lucky/Source/Lucky/Scripting/ScriptEngine.h)：`GetCoreAssemblyImage / GetSceneContext` 已就位
- [ScriptComponent.h](../../Lucky/Source/Lucky/Scene/Components/ScriptComponent.h)：P1.5 完全不感知
- [Scene.h/.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp)：`TryGetEntityWithUUID` 已可用
- [TransformComponent.h](../../Lucky/Source/Lucky/Scene/Components/TransformComponent.h)：字段名 `Translation` 已就位，P1.5 只读写不改
- premake：新增 `.h/.cpp` 与 `ScriptEngine` 同目录，`Lucky.vcxproj` 重生成即可收录

---

## 3. 现状回顾

### 3.1 托管侧 6 个桩函数（P1.2 产物，签名不可改）

[InternalCalls.cs](../../Lucky-ScriptCore/Source/Lucky/InternalCalls.cs) 现有内容：

```csharp
public static class InternalCalls
{
    // ---- Debug ----
    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern void Debug_Log(string message);

    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern void Debug_Warn(string message);

    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern void Debug_Error(string message);

    // ---- Entity ----
    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern bool Entity_HasComponent(ulong entityID, Type componentType);

    // ---- TransformComponent ----
    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern void TransformComponent_GetPosition(ulong entityID, out Vector3 outPosition);

    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern void TransformComponent_SetPosition(ulong entityID, ref Vector3 inPosition);
}
```

### 3.2 C# 类型到 native 类型的映射规则

Mono 对 InternalCall 的 C# 参数 → native 参数映射是硬约定：

| C# 参数 | native 参数 | 备注 |
|--------|-----------|------|
| `string`（`ref/out` 无） | `MonoString*` | UTF-16 托管字符串；用 `mono_string_to_utf8` 拆包 |
| `Type` | `MonoReflectionType*` | 反射类型；用 `mono_reflection_type_get_type` 取 `MonoType*` |
| `ulong` | `uint64_t` | blittable，直传 |
| `bool` 返回值 | `bool`（Mono 4.0+ 已是 1 字节） | 直传 |
| `out T`（值类型） | `T*` | native 写入，托管拷贝 |
| `ref T`（值类型） | `T*` | native 读取（可写入） |

### 3.3 `TransformComponent` 字段命名事实

[TransformComponent.h](../../Lucky/Source/Lucky/Scene/Components/TransformComponent.h) 的字段名是：

```cpp
struct TransformComponent
{
    glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };
private:
    glm::vec3 RotationEuler;
    glm::quat Rotation;
    ...
};
```

**关键点**：C++ 侧 [TransformComponent.h](../../Lucky/Source/Lucky/Scene/Components/TransformComponent.h) 字段名 `Translation`（public），C# 侧 [Component.cs](../../Lucky-ScriptCore/Source/Lucky/Component.cs) 属性名 `Position`。这个不一致由 [Component.cs](../../Lucky-ScriptCore/Source/Lucky/Component.cs) 决定，P1.5 侧只需在 native `TransformComponent_GetPosition` 内部读写 `tc.Translation` 即可??**是否消除这个不一致**在 §4 决策点 8 里单独讨论。

### 3.4 `Scene::TryGetEntityWithUUID` 语义

```cpp
Entity Scene::TryGetEntityWithUUID(UUID id)
{
    if (const auto it = m_EntityIDMap.find(id); it != m_EntityIDMap.end())
    {
        return it->second;
    }
    return Entity{};
}
```

返回**默认构造 Entity** 时 `m_Scene = nullptr`。而 [Entity.h](../../Lucky/Source/Lucky/Scene/Entity.h) 的 `HasComponent<T>()` 直接 `m_Scene->m_Registry.has<T>(...)`??**对无效 Entity 调 HasComponent 会解引用 nullptr 崩溃**。这决定了 native 端必须先 `if (!entity)` 守卫，再走后续调用（见 §4 决策点 6）。

### 3.5 `Entity` 的 `operator bool`

```cpp
operator bool() const { return m_EntityID != entt::null; }
```

配合 `TryGetEntityWithUUID` 返回值直接 `if (!entity)` 即可判空。

### 3.6 `ScriptEngine::Init` 现状（要插入注册点的地方）

[ScriptEngine.cpp](../../Lucky/Source/Lucky/Scripting/ScriptEngine.cpp) 当前实现：

```cpp
void ScriptEngine::Init()
{
    s_Data = new ScriptEngineData();

    InitMono();

    LoadCoreAssembly("Resources/Scripts/Lucky-ScriptCore.dll");
    LoadAppAssembly("Assets/Scripts/Binaries/App.dll");
    LoadAssemblyClasses();

    s_Data->EntityBaseClass = CreateRef<ScriptClass>("Lucky", "Entity", true);

    LF_CORE_INFO("ScriptEngine: initialized ({} user script classes loaded)", s_Data->EntityClasses.size());
}
```

`ScriptGlue::RegisterFunctions()` 需要 mono JIT 就绪（`InitMono` 后即可）；`ScriptGlue::RegisterComponents()` 需要 CoreAssemblyImage 就绪（`LoadCoreAssembly` 后）。**两者都放在 `LoadAssemblyClasses()` 之后、最后 `LF_CORE_INFO` 之前**是最集中、最不破坏现有节奏的位置（见 §4 决策点 7）。

---

## 4. 关键设计决策（多方案对比）

### 4.1 决策点 1：文件组织形态

#### 方案 A：单一 `ScriptGlue.h/.cpp`，6 个函数塞一起（**推荐 ?**）
- **优点**：P1.5 阶段总共不足 200 行；跳转、grep、diff 都最简单；未来分裂到方案 B 的迁移成本几乎为零（把某组函数剪切到新 .cpp 即可，接口 `RegisterFunctions/RegisterComponents` 不变）
- **缺点**：Phase 3/4 加入 Input、Time、Physics 等 InternalCall 后文件会变大，届时可迁移

#### 方案 B：按主题分文件（`ScriptGlue_Debug.cpp / ScriptGlue_Entity.cpp / ScriptGlue_Transform.cpp`）
- **优点**：未来扩展时天然分文件
- **缺点**：P1.5 阶段只有 6 个函数就分 3 个文件，明显过度设计；同一时刻只有 1 个开发者动这块，冲突问题不存在

#### 方案 C：直接把 native 函数写成 `ScriptEngine.cpp` 内的静态私有函数
- **优点**：不新增文件
- **缺点**：`ScriptEngine.cpp` 已 10KB+，继续塞会失控；native 函数需要 mono `object.h`、`reflection.h` 等一堆头，会污染 `ScriptEngine.cpp` 的头依赖；职责边界破坏

**结论：方案 A**。原因：Luck3D 当前"6 个函数、未来必然增长"的形态下，A 是"当前成本最低 + 未来迁移成本零"的最优解。

---

### 4.2 决策点 2：`ScriptGlue` 是"类"还是"命名空间"

#### 方案 A：`class ScriptGlue { public: static void RegisterFunctions(); static void RegisterComponents(); };`（**推荐 ?**）
- **优点**：调用点写作 `ScriptGlue::RegisterFunctions()`，与既有 `ScriptEngine::Init()` 视觉一致；两个静态方法有明确归属感
- **缺点**：类只有静态方法，本质上是命名空间的伪装

#### 方案 B：`namespace ScriptGlue { void RegisterFunctions(); void RegisterComponents(); }`
- **优点**：语义更准确??就是一组自由函数
- **缺点**：调用形式 `ScriptGlue::RegisterFunctions()` 与"类静态方法调用"完全同形，用户看不出差别；反而与项目内已有的 `class ScriptEngine`、`class Renderer3D`、`class Input`（都是全静态类模式，见 [Coding_Style_Guide.md](../Coding_Style_Guide.md) 13.2 节）风格不一致

**结论：方案 A**。原因：项目已确立"无实例的工具类/管理器用全静态类"的约定，`ScriptGlue` 归属于同一模式。

---

### 4.3 决策点 3：`Entity_HasComponent` 的分发机制

C# 侧 `HasComponent<T>()` 泛型接口 → InternalCall 传入 `Type` 参数 → native 端要根据这个 `MonoType*` 判断实体是否挂了对应 C++ 组件。这是 P1.5 唯一需要"设计"的核心机制。

#### 方案 A：`unordered_map<MonoType*, function<bool(Entity)>>`（**推荐 ?**）
```cpp
static std::unordered_map<MonoType*, std::function<bool(Entity)>> s_EntityHasComponentFuncs;

template<typename TComponent>
static void RegisterComponent()
{
    MonoType* managedType = mono_reflection_type_from_name(...);
    s_EntityHasComponentFuncs[managedType] = [](Entity e) { return e.HasComponent<TComponent>(); };
}
```
- **优点**：扩展新组件只需 `RegisterComponent<T>()` 一行；查询是 `map::find` + `std::function` 调用，撑死每帧几十次，性能完全不敏感
- **缺点**：`std::function` 有一次间接调用开销（相对 direct call）

#### 方案 B：手写 `if-else if` 链
```cpp
if (managedType == s_TransformType) { return e.HasComponent<TransformComponent>(); }
else if (managedType == s_CameraType) { return e.HasComponent<CameraComponent>(); }
...
```
- **优点**：无 `std::function` 开销；无 map 查找
- **缺点**：每加一个组件要改 `Entity_HasComponent` 函数本体；`s_TransformType` 这类静态缓存还是要有，代码总量不少反多；违反"开闭原则"

#### 方案 C：每次调用 `mono_type_get_name(managedType)` 后字符串 switch
- **优点**：不需预注册表
- **缺点**：每次调用都做字符串比较，性能最差；类名重命名时漏改字符串静默出错

**结论：方案 A**。原因：分发开销在此路径完全不敏感（脚本每帧几十次 HasComponent 撑死），而 map 表让"加新组件 = 加一行 `RegisterComponent<T>()`"这一扩展路径最短最平滑。

---

### 4.4 决策点 4：`mono_add_internal_call` 的注册代码形态

#### 方案 A：本地宏 `LF_ADD_INTERNAL_CALL(Name)`（**推荐 ?**）
```cpp
#define LF_ADD_INTERNAL_CALL(Name) \
    mono_add_internal_call("Lucky.InternalCalls::" #Name, (const void*)Name)

void ScriptGlue::RegisterFunctions()
{
    LF_ADD_INTERNAL_CALL(Debug_Log);
    LF_ADD_INTERNAL_CALL(Debug_Warn);
    ...
}

#undef LF_ADD_INTERNAL_CALL
```
- **优点**：函数名 = C# 字符串 = 注册名，三者由预处理器保证同步；写错函数名（如 `Debug_Loog`）立即编译报错，而写错字符串则要运行时才暴露；DRY
- **缺点**：多一个宏（但作用域仅本 `.cpp` 内，用完 `#undef`）

#### 方案 B：手写完整字符串
```cpp
mono_add_internal_call("Lucky.InternalCalls::Debug_Log", (const void*)Debug_Log);
mono_add_internal_call("Lucky.InternalCalls::Debug_Warn", (const void*)Debug_Warn);
...
```
- **优点**：无宏，复制粘贴看得懂
- **缺点**：字符串和函数名同步靠人眼；出错静默（mono 找不到映射，C# 调用时才抛 `MissingMethodException`）

**结论：方案 A**。原因：用编译期约束换运行期约束，是零成本收益。宏是**本地的**（`#define` + `#undef` 只影响本 `.cpp`），不入 `.h`，不污染其它编译单元；宏命名遵循 `LF_` 前缀规范（[Coding_Style_Guide.md](../Coding_Style_Guide.md) §9.1）。

---

### 4.5 决策点 5：`RegisterComponents` 里注册哪些组件

#### 方案 A：只注册 `TransformComponent` 一个（**推荐 ?**）
- **优点**：不做无效注册；`mono_reflection_type_from_name` 对不存在的类返回 nullptr，把 nullptr 作 key 塞进 map 会让后续 `Entity_HasComponent` 收到任意"未在 core 中定义的 MonoType"时都命中同一个 nullptr key，导致语义混乱
- **缺点**：无

#### 方案 B：一次性注册全部 ECS 组件（`Camera / MeshRenderer / Light / SpriteRenderer / PostProcessVolume / Script / Name / Relationship`）
- **优点**：一步到位，Phase 2 再加托管类时 native 侧无需改动
- **缺点**：托管侧根本没有对应 C# 类，`mono_reflection_type_from_name(image, "Lucky.CameraComponent")` 全部返回 nullptr，注册的 key 都是 nullptr，撞在一起
- **缺点**：违反[[memory:du4s18ic]]"不为未来阶段预铺路"

#### 方案 C：只注册 Transform，且对 `mono_reflection_type_from_name` 返回 nullptr 打 `LF_CORE_ERROR`（=A + 防御）
- 与 A 相同 + 一层未来防御性错误提示

**结论：方案 C**（= A 加一层错误日志）。理由：当前实际只注册 Transform 一个，同时 `RegisterComponent<T>` 模板内部对 nullptr 打 `LF_CORE_ERROR("ScriptGlue: managed type '{}' not found in Core assembly", managedTypeName);`，未来加新组件时如果忘了写托管侧 C# 类，日志会立刻提示（不 assert，因为异步开发中允许暂时缺失）。

---

### 4.6 决策点 6：native 函数拿不到 Entity 时的处理策略

C# 传入 `ulong entityID`，`TryGetEntityWithUUID` 可能返回无效 Entity（实体已销毁、或 C# 侧构造了假 ID）。三种处理：

#### 方案 A：`LF_CORE_ASSERT` 直接崩
- **优点**：早暴露 bug
- **缺点**：脚本运行时实体销毁是**合法场景**（脚本 A 销毁实体 B，B 上的脚本下一帧仍在被调度）；引擎不该崩

#### 方案 B：打 warn 后 return 无害值（`GetPosition` 返回零向量、`SetPosition` 忽略、`HasComponent` 返回 false）（**推荐 ?**）
- **优点**：符合 Roadmap Phase 3 明确的方向"脚本异常不导致引擎崩溃"；日志保留可观测性
- **缺点**：可能掩盖脚本作者"引用了已死实体"的 bug；但这个折衷是必要的

#### 方案 C：`mono_raise_exception` 抛托管异常
- **优点**：标准的失败传递机制，C# 侧可 catch
- **缺点**：P1.5 阶段托管侧没有对应 try/catch 基础设施；实体销毁属于常见路径，用异常传达是滥用；Phase 3 补 Console 与异常捕获后可再评估

**结论：方案 B**，并配套**必须**在每个 native 函数入口写 `if (!entity) { LF_CORE_WARN(...); return; }` 前置守卫。原因见 §3.4??`Entity::HasComponent` 对无效 Entity 会解引用 nullptr 崩溃，守卫是硬需求，不是可选优化。

---

### 4.7 决策点 7：`RegisterFunctions / RegisterComponents` 在 `Init` 里的位置

#### 方案 A：都放在 `Init` 末尾，`LF_CORE_INFO` 之前（**推荐 ?**）
```cpp
void ScriptEngine::Init()
{
    s_Data = new ScriptEngineData();
    InitMono();
    LoadCoreAssembly(...);
    LoadAppAssembly(...);
    LoadAssemblyClasses();
    s_Data->EntityBaseClass = CreateRef<ScriptClass>("Lucky", "Entity", true);

    ScriptGlue::RegisterFunctions();
    ScriptGlue::RegisterComponents();

    LF_CORE_INFO("ScriptEngine: initialized ({} user script classes loaded)", s_Data->EntityClasses.size());
}
```
- **优点**：注册集中在一处、`Init` 顶层调用序列一眼看懂；未来加 hot reload 时只需再调一次这两个方法；`RegisterFunctions` 和 `RegisterComponents` 的依赖前置条件（分别是 `InitMono` 和 `LoadCoreAssembly`）都已满足
- **缺点**：无

#### 方案 B：分散在流程中（`RegisterFunctions` 紧跟 `InitMono`；`RegisterComponents` 紧跟 `LoadCoreAssembly`）
- **优点**：贴近"能注册就注册"的最早时机
- **缺点**：Init 序列变碎；两处 include `ScriptGlue.h`；未来 hot reload 时"重注册"要跳到两个地方；对"越早越好"的收益纯粹是安慰性的（mono 允许在类型被使用前的任意时机注册）

**结论：方案 A**。集中调用点在长期维护、Phase 3 hot reload、日志汇总等场景全面更优。

---

### 4.8 决策点 8：C++ 字段 `Translation` 与 C# 属性 `Position` 的映射

事实：
- C++ 侧 [TransformComponent.h](../../Lucky/Source/Lucky/Scene/Components/TransformComponent.h) 字段名 `Translation`
- C# 侧 [Component.cs](../../Lucky-ScriptCore/Source/Lucky/Component.cs) 属性名 `Position`
- InternalCall 桩名 `TransformComponent_GetPosition / SetPosition`

#### 方案 A：native 端内部读写 `tc.Translation`，托管侧继续叫 `Position`（**推荐 ?**）
- **优点**：P1.5 完全不改托管侧；每一层的命名都符合各自的惯用叫法（引擎侧 Translation 是变换矩阵组成语义，脚本侧 Position 是"游戏对象位置"的直觉说法，与 Unity 一致）
- **缺点**：跨层命名不完全对齐??但这是**合理的**跨层翻译，就像 UI "Add Component" 按钮里显示 "Script"，但 C++ 结构体叫 `ScriptComponent`

#### 方案 B：把托管侧 `Position` 改名 `Translation`
- **优点**：C++/C# 命名一致
- **缺点**：破坏"脚本作者用 `Position` 属于常识"的用户期待；Unity 用户来到 Luck3D 会觉得"这个引擎为什么用了个奇怪名字"；改动 P1.2 已完成的托管 API

#### 方案 C：把 C++ 字段改名 `Position`
- **缺点**：改动量巨大??`Translation` 在 [TransformComponent.h](../../Lucky/Source/Lucky/Scene/Components/TransformComponent.h) 是数学语义（变换矩阵的 T 分量），全 codebase grep 会有几十上百处引用；破坏面完全不成比例

**结论：方案 A**。原因：跨层命名翻译是引擎设计常态。native 侧写 `tc.Translation`，注释里说明"对应托管 `Position` 属性"即可。

---

### 4.9 决策点 9：`glm::vec3` 与 C# `Vector3` 的内存布局对齐

C# 侧 `Vector3` 是 `struct { public float x, y, z; }`（无 `[StructLayout]` 显式声明），C++ 侧 `glm::vec3` 是 3 个 float。托管 → native 传递时能否直接 `glm::vec3*` 强转？

#### 方案 A：直接强转 + `static_assert(sizeof(glm::vec3) == 12)` 兜底（**推荐 ?**）
```cpp
static_assert(sizeof(glm::vec3) == 3 * sizeof(float), "glm::vec3 layout mismatch");

static void TransformComponent_GetPosition(UUID entityID, glm::vec3* outPosition)
{
    ...
    *outPosition = tc.Translation;
}
```
- **优点**：0 拷贝；C# 的 struct 默认 `LayoutKind.Sequential`，且 `Vector3` 字段全是 blittable float，实际布局与 C 语言一致，是 Mono 官方文档保证的
- **缺点**：如果未来 `Vector3` 加了 `bool` 或引用字段就不再 blittable??但那是 P2 的问题，届时统一给 `Vector3` 加 `[StructLayout(LayoutKind.Sequential)]` 显式声明即可
- **兜底**：`static_assert` 编译期挡住 glm 侧尺寸变化（如某些 SIMD 编译选项让 glm::vec3 变成 16 字节）

#### 方案 B：托管侧 `Vector3` 现在就加 `[StructLayout(LayoutKind.Sequential)]`
- **优点**：显式声明，长期防御
- **缺点**：P1.5 意外扩展到托管层修改；`bool/引用` 字段的引入是 Phase 2 才可能发生的事，届时一并加更集中

#### 方案 C：native 端接收 `float outX, outY, outZ` 三个 out 参数
- **缺点**：违反 [InternalCalls.cs](../../Lucky-ScriptCore/Source/Lucky/InternalCalls.cs) 已固化的签名 `out Vector3`；改不了

**结论：方案 A**。加一行 `static_assert` 编译期兜底，托管侧的 `[StructLayout]` 加固留给 Phase 2。

---

### 4.10 决策点 10：`MonoString*` → `std::string` 的字符串处理

`mono_string_to_utf8(MonoString*)` 返回 `char*`，**必须**用 `mono_free` 释放（mono API 硬要求）。释放时机有两种：

#### 方案 A：拷贝到 `std::string` 后立即 `mono_free`，再打日志（**推荐 ?**）
```cpp
static void Debug_Log(MonoString* message)
{
    char* cstr = mono_string_to_utf8(message);
    std::string s(cstr);
    mono_free(cstr);
    LF_CORE_INFO("{}", s);
}
```
- **优点**：`mono_free` 立即调用，不依赖 spdlog 内部时序；如果未来 spdlog 切异步 sink（`LF_CORE_INFO` 只是把参数塞进队列、后台线程延迟输出），这段代码依然正确
- **缺点**：多一次 `std::string` 拷贝（32 字节 SSO 之内是栈拷、超出是一次堆分配）??脚本日志频次很低，可忽略

#### 方案 B：直接把 cstr 传给 `LF_CORE_INFO`，然后 `mono_free`
```cpp
LF_CORE_INFO("{}", cstr);
mono_free(cstr);
```
- **优点**：无多余拷贝
- **缺点**：正确性依赖 spdlog 同步 sink??如果未来改异步，`LF_CORE_INFO` 会把 `const char*` 存进队列然后马上 `mono_free`，后台线程 flush 时读的是已释放内存，未定义行为

**结论：方案 A**。当前 spdlog 是同步 sink（已 grep 确认无 async 关键字），方案 B 也能跑；但 A 是 O(1) 拷贝换未来 0 修改，收益远大于成本。

---

## 5. 实现步骤（按依赖顺序）

每步完成后 `Lucky` 与 `Luck3DApp` 都应能编译通过。

### Step 1：新建 `ScriptGlue.h`

**文件**：`Lucky/Source/Lucky/Scripting/ScriptGlue.h`

```cpp
#pragma once

namespace Lucky
{
    /// <summary>
    /// 脚本桥：把 C# 侧 [MethodImpl(InternalCall)] 桩函数与 C++ native 实现挂接
    /// 由 ScriptEngine::Init 在加载完程序集后调用一次
    /// </summary>
    class ScriptGlue
    {
    public:
        /// <summary>
        /// 注册所有 InternalCall 函数到 Mono JIT
        /// 前置条件：InitMono 已完成
        /// </summary>
        static void RegisterFunctions();

        /// <summary>
        /// 注册托管组件类型 → C++ HasComponent 分发的映射表
        /// 前置条件：LoadCoreAssembly 已完成（否则 mono_reflection_type_from_name 找不到类）
        /// </summary>
        static void RegisterComponents();
    };
}
```

**要点**：
- 零 mono 依赖：本头文件不 include 任何 mono header，`.cpp` 里再 include
- 类只做全静态方法（[Coding_Style_Guide.md](../Coding_Style_Guide.md) §13.2 静态类模式）
- XML 文档注释按 [Coding_Style_Guide.md](../Coding_Style_Guide.md) §4.1

### Step 2：新建 `ScriptGlue.cpp` 骨架 + 全局映射表

**文件**：`Lucky/Source/Lucky/Scripting/ScriptGlue.cpp`

```cpp
#include "lcpch.h"
#include "ScriptGlue.h"
#include "ScriptEngine.h"

#include "Lucky/Scene/Scene.h"
#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/TransformComponent.h"

#include <glm/glm.hpp>

#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

#include <format>
#include <functional>
#include <unordered_map>

namespace Lucky
{
    // glm::vec3 与 C# Lucky.Vector3 的内存布局必须一致（都是 3 个连续 float）
    static_assert(sizeof(glm::vec3) == 3 * sizeof(float), "glm::vec3 layout mismatch with C# Lucky.Vector3");

    // 托管 Type → HasComponent<T> 的分发表
    static std::unordered_map<MonoType*, std::function<bool(Entity)>> s_EntityHasComponentFuncs;
}
```

**要点**：
- include 顺序遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md) §3.3：PCH → 对应头 → 项目内部头 → 第三方 → 标准库
- `static_assert` 放在 namespace 顶端、任何函数外
- 映射表用 `static`（文件作用域，前缀 `s_`，符合 [Coding_Style_Guide.md](../Coding_Style_Guide.md) §2.2）

### Step 3：实现 `Debug_*` 三个 native 函数

在 Step 2 的 namespace 内追加：

```cpp
    // ======== Debug ========

    static void Debug_Log(MonoString* message)
    {
        char* cstr = mono_string_to_utf8(message);
        std::string s(cstr);
        mono_free(cstr);
        LF_CORE_INFO("{}", s);
    }

    static void Debug_Warn(MonoString* message)
    {
        char* cstr = mono_string_to_utf8(message);
        std::string s(cstr);
        mono_free(cstr);
        LF_CORE_WARN("{}", s);
    }

    static void Debug_Error(MonoString* message)
    {
        char* cstr = mono_string_to_utf8(message);
        std::string s(cstr);
        mono_free(cstr);
        LF_CORE_ERROR("{}", s);
    }
```

**要点**：
- 三个函数结构完全对称（决策 4.10-A：拷 std::string → mono_free → log）
- 不用宏合并三个函数??三行代码而已，宏反而降低可读性

### Step 4：实现 `Entity_HasComponent`

```cpp
    // ======== Entity ========

    static bool Entity_HasComponent(UUID entityID, MonoReflectionType* componentType)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        if (!scene)
        {
            LF_CORE_WARN("ScriptGlue::Entity_HasComponent: no active scene context");
            return false;
        }

        Entity entity = scene->TryGetEntityWithUUID(entityID);
        if (!entity)
        {
            LF_CORE_WARN("ScriptGlue::Entity_HasComponent: entity {} not found", static_cast<uint64_t>(entityID));
            return false;
        }

        MonoType* managedType = mono_reflection_type_get_type(componentType);
        auto it = s_EntityHasComponentFuncs.find(managedType);
        if (it == s_EntityHasComponentFuncs.end())
        {
            LF_CORE_WARN("ScriptGlue::Entity_HasComponent: managed type not registered");
            return false;
        }

        return it->second(entity);
    }
```

**要点**：
- 三层守卫：scene / entity / managedType，任何一层失败都 warn+false，不 assert（决策 4.6-B）
- `static_cast<uint64_t>(entityID)`：`UUID` 通常内部封 `uint64_t`，日志格式化用显式转换（[Coding_Style_Guide.md](../Coding_Style_Guide.md) §13.8）
- 花括号一律强制（[Coding_Style_Guide.md](../Coding_Style_Guide.md) §5.2）

### Step 5：实现 `TransformComponent_GetPosition / SetPosition`

```cpp
    // ======== TransformComponent ========

    static void TransformComponent_GetPosition(UUID entityID, glm::vec3* outPosition)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        if (!scene)
        {
            LF_CORE_WARN("ScriptGlue::TransformComponent_GetPosition: no active scene context");
            *outPosition = glm::vec3(0.0f);
            return;
        }

        Entity entity = scene->TryGetEntityWithUUID(entityID);
        if (!entity || !entity.HasComponent<TransformComponent>())
        {
            LF_CORE_WARN("ScriptGlue::TransformComponent_GetPosition: entity {} invalid or no TransformComponent", static_cast<uint64_t>(entityID));
            *outPosition = glm::vec3(0.0f);
            return;
        }

        *outPosition = entity.GetComponent<TransformComponent>().Translation;
    }

    static void TransformComponent_SetPosition(UUID entityID, glm::vec3* inPosition)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        if (!scene)
        {
            LF_CORE_WARN("ScriptGlue::TransformComponent_SetPosition: no active scene context");
            return;
        }

        Entity entity = scene->TryGetEntityWithUUID(entityID);
        if (!entity || !entity.HasComponent<TransformComponent>())
        {
            LF_CORE_WARN("ScriptGlue::TransformComponent_SetPosition: entity {} invalid or no TransformComponent", static_cast<uint64_t>(entityID));
            return;
        }

        entity.GetComponent<TransformComponent>().Translation = *inPosition;
    }
```

**要点**：
- native 侧读写 `tc.Translation`（对应托管 `Position` 属性，决策 4.8-A）
- `GetPosition` 失败时 `*outPosition = glm::vec3(0.0f)` 保证托管侧 `out` 参数一定被写入（C# 语义要求 `out` 必须赋值，虽然 InternalCall 允许"native 不写"，但显式零值最安全）
- 两个函数守卫链一致：scene → entity → HasComponent

### Step 6：实现 `RegisterComponent<T>` 模板

在 §Step 4 前面（namespace 内、Debug_Log 之后即可，位置不敏感）追加：

```cpp
    template<typename TComponent>
    static void RegisterComponent()
    {
        // typeid(TComponent).name() 在 MSVC 下形如 "struct Lucky::TransformComponent"
        // 需要抽出 "TransformComponent" 并拼成 C# 全名 "Lucky.TransformComponent"
        std::string_view typeName = typeid(TComponent).name();
        size_t pos = typeName.find_last_of(':');
        std::string_view structName = (pos == std::string_view::npos) ? typeName : typeName.substr(pos + 1);
        std::string managedTypeName = std::format("Lucky.{}", structName);

        MonoType* managedType = mono_reflection_type_from_name(managedTypeName.data(), ScriptEngine::GetCoreAssemblyImage());
        if (!managedType)
        {
            LF_CORE_ERROR("ScriptGlue::RegisterComponent: managed type '{}' not found in Core assembly", managedTypeName);
            return;
        }

        s_EntityHasComponentFuncs[managedType] = [](Entity e) { return e.HasComponent<TComponent>(); };
    }
```

**要点**：
- **不剥 `Component` 后缀**：Luck3D 托管侧类名保留 `Component` 后缀（[Component.cs](../../Lucky-ScriptCore/Source/Lucky/Component.cs) 里定义的是 `TransformComponent`，不是 `Transform`），所以 `Lucky::TransformComponent` → `Lucky.TransformComponent` 直接一一对应，无需处理
- MSVC 下 `typeid(T).name()` 前缀 `struct`/`class`：`find_last_of(':')` 之后得到的就是纯类名，不受前缀影响
- `mono_reflection_type_from_name` 的第一个参数是 `char*`（非 const），需要 `.data()` 而非 `.c_str()`??但 C++17 起 `std::string::data()` 返回 `char*` 版本已存在
- nullptr 时 `LF_CORE_ERROR` 而不 assert，决策 4.5-C

### Step 7：实现 `RegisterFunctions` 与 `RegisterComponents`

在 `.cpp` 底部：

```cpp
    // ======== Registry ========

    void ScriptGlue::RegisterFunctions()
    {
#define LF_ADD_INTERNAL_CALL(Name) \
        mono_add_internal_call("Lucky.InternalCalls::" #Name, (const void*)Name)

        LF_ADD_INTERNAL_CALL(Debug_Log);
        LF_ADD_INTERNAL_CALL(Debug_Warn);
        LF_ADD_INTERNAL_CALL(Debug_Error);

        LF_ADD_INTERNAL_CALL(Entity_HasComponent);

        LF_ADD_INTERNAL_CALL(TransformComponent_GetPosition);
        LF_ADD_INTERNAL_CALL(TransformComponent_SetPosition);

#undef LF_ADD_INTERNAL_CALL
    }

    void ScriptGlue::RegisterComponents()
    {
        s_EntityHasComponentFuncs.clear();
        RegisterComponent<TransformComponent>();
    }
}   // namespace Lucky
```

**要点**：
- 宏 `LF_ADD_INTERNAL_CALL` 是**本地宏**：`#define` 与 `#undef` 都在 `RegisterFunctions` 内部，不入头
- `RegisterComponents` 一开始 `clear()`：为未来 hot reload 做准备（多次调用不残留旧 MonoType* key）
- 目前只注册 `TransformComponent` 一个（决策 4.5-A）

### Step 8：修改 `ScriptEngine.cpp` 挂接调用

**文件**：[ScriptEngine.cpp](../../Lucky/Source/Lucky/Scripting/ScriptEngine.cpp)

顶部 include 区追加：

```cpp
#include "ScriptGlue.h"
```

`Init()` 函数体修改为：

```cpp
void ScriptEngine::Init()
{
    s_Data = new ScriptEngineData();

    InitMono();

    LoadCoreAssembly("Resources/Scripts/Lucky-ScriptCore.dll");
    LoadAppAssembly("Assets/Scripts/Binaries/App.dll");
    LoadAssemblyClasses();

    s_Data->EntityBaseClass = CreateRef<ScriptClass>("Lucky", "Entity", true);

    ScriptGlue::RegisterFunctions();
    ScriptGlue::RegisterComponents();

    LF_CORE_INFO("ScriptEngine: initialized ({} user script classes loaded)", s_Data->EntityClasses.size());
}
```

**要点**：
- 两行调用集中放在 `EntityBaseClass` 构造之后、`LF_CORE_INFO` 之前（决策 4.7-A）
- 顺序：`RegisterFunctions` 先、`RegisterComponents` 后??没有硬依赖，但按"函数注册基础设施→组件类型注册"的语义排列更自然

### Step 9：premake 重新生成 + 编译验证

- 运行 `Scripts/Setup-Windows.bat`（或直接 `premake5 --file=Build.lua vs2022`），让 `ScriptGlue.h/.cpp` 被 `Lucky.vcxproj` 收录
- Debug 编译 `Lucky` → 通过
- Debug 编译 `Luck3DApp` → 通过
- 启动 `Luck3DApp.exe`：
  - 日志中出现 `ScriptEngine: initialized (N user script classes loaded)`
  - **没有** `ScriptGlue::RegisterComponent: managed type 'Lucky.TransformComponent' not found` 之类的 error
  - 没有 mono 相关新 warn/error

---

## 6. 疑点问答

### 6.1 `MonoType*` 作为 map key 稳定吗？

答：**稳定**。同一个 AppDomain 内、同一个 image 加载后，`mono_reflection_type_from_name` 对同一个类名返回的 `MonoType*` 是同一个指针（mono 内部有类型池）。**但**跨 AppDomain reload 后 `MonoType*` 会失效??这也是 §Step 7 的 `RegisterComponents` 一开始 `clear()` 的原因（Phase 3 hot reload 时会重新 `RegisterComponents`）。

### 6.2 为什么不在 `ScriptGlue.h` 里暴露 `s_EntityHasComponentFuncs`？

答：这个 map 只被 `Entity_HasComponent` 与 `RegisterComponent<T>` 两个 `.cpp` 内的函数访问，暴露到头会：1) 泄漏 `MonoType*` 到头依赖；2) 允许其它 TU 修改这张表，破坏封装。所以保持文件作用域 `static`。

### 6.3 `Debug_Log` 的 `MonoString*` 可能为 nullptr 吗？

答：C# 侧 `string message` 传 `null` 是合法调用。native 端 `mono_string_to_utf8(nullptr)` 会返回 nullptr 或者 crash（mono 版本相关）。**加固方案**：在 `Debug_Log/Warn/Error` 入口加 `if (!message) { LF_CORE_INFO(""); return; }`。这个细节可以本 Phase 加，也可以留给 Phase 3 补??建议现在就加，成本一行。

**推荐实现（Step 3 的加固版）**：

```cpp
static void Debug_Log(MonoString* message)
{
    if (!message)
    {
        LF_CORE_INFO("");
        return;
    }
    char* cstr = mono_string_to_utf8(message);
    std::string s(cstr);
    mono_free(cstr);
    LF_CORE_INFO("{}", s);
}
```

### 6.4 为什么 P1.5 不注册 `ScriptComponent` 到 `s_EntityHasComponentFuncs`？

答：托管侧没有 `Lucky.ScriptComponent` 类（[Component.cs](../../Lucky-ScriptCore/Source/Lucky/Component.cs) 里只定义了 `Component` 基类和 `TransformComponent`）。而且脚本作者不需要 `entity.HasComponent<ScriptComponent>()`??脚本本身就是 `ScriptComponent` 的运行时表现，判断没有意义。这条留给未来做"脚本互相查找"时再评估。

### 6.5 `mono_add_internal_call` 的函数指针需要 `__stdcall` 之类的调用约定吗？

答：**不需要**，mono 在 Windows x64 下默认走 x64 calling convention（`__fastcall` 的变体），与 native `static` 函数一致。历史上 x86 时代 mono 有 `MONO_API` 修饰，x64 之后已统一。

### 6.6 `RegisterFunctions` 里如果某个 `mono_add_internal_call` 失败会怎样？

答：`mono_add_internal_call` 本身返回 `void`，且不会失败??它只是往 mono 内部 hash 表里塞一条记录。**真正的失败会在 C# 首次调用该 InternalCall 时暴露**：如果 C# 侧字符串与 `mono_add_internal_call` 注册的字符串不一致，会抛 `MissingMethodException`。这也是决策 4.4-A 用宏保证字符串同步的核心价值。

### 6.7 `TransformComponent_SetPosition` 会不会导致父子层级下的 WorldTransform 不同步？

答：会??`Translation` 是局部坐标，直接改后世界矩阵要等 `Scene::UpdateWorldTransformRecursive` 下一帧才更新（见 [TransformComponent.h](../../Lucky/Source/Lucky/Scene/Components/TransformComponent.h) 的 `WorldTransform` 注释）。这是当前引擎既有约定，不是 P1.5 引入的问题。脚本作者若要立即读到新的世界坐标，需要在下一帧访问（P1.6 后脚本 `OnUpdate` 天然是在每帧 `UpdateWorldTransformRecursive` 之前调用，等下一帧访问即可）。

### 6.8 `RegisterComponents` 里的 `typeid(T).name()` 在 GCC/Clang 下会给出 mangled 名（如 `N5Lucky18TransformComponentE`），怎么办？

答：Luck3D 当前只支持 MSVC（premake 生成 vs2022 工程），MSVC 下 `typeid(T).name()` 就是可读的 `"struct Lucky::TransformComponent"`。**若未来跨平台**，需要用 `__PRETTY_FUNCTION__` 技巧或者显式传字符串：

```cpp
RegisterComponent<TransformComponent>("Lucky.TransformComponent");
```

**结论**：当前 MSVC-only 直接依赖 `typeid` 即可；跨平台是 Phase 4+ 才需要考虑的事，届时统一改造，不预留。

### 6.9 为什么 `RegisterComponent` 是文件作用域 static 而不是 `ScriptGlue` 的私有静态成员方法？

答：模板方法作为类的私有静态成员要在头里定义（模板不能只声明不定义），头就得 include 一大堆 mono / typeid 相关支持。而作为 `.cpp` 内的自由函数模板，可以完全埋在编译单元里，头保持干净。这是 §4.1-A 决策的直接推论。

---

## 7. 验收标准

MVP 级验收（本 Phase 独立完成后应满足全部）：

1. **编译通过**：`Lucky` 与 `Luck3DApp` 的 Debug / Release / Dist 三个 configuration 全通过
2. **启动无 mono 报错**：启动 `Luck3DApp.exe`，日志中出现 `ScriptEngine: initialized (N user script classes loaded)`；**没有** `ScriptGlue::RegisterComponent` 相关的 error
3. **静态自证**：Debug 附加进程，在 `ScriptGlue::RegisterComponents` 出口断点，观察 `s_EntityHasComponentFuncs` size == 1，且 key 非 nullptr
4. **端到端可选验证**（建议做但非必须）：在 `ScriptEngine::Init` 尾部临时插入以下测试代码（验收后**删除**，遵循[[memory:du4s18ic]]）：
   ```cpp
   ScriptClass debugClass("Lucky", "Debug", true);
   MonoMethod* logMethod = debugClass.GetMethod("Log", 1);
   MonoString* msg = mono_string_new(s_Data->AppDomain, "[P1.5 test] Hello from managed side");
   void* args[] = { msg };
   mono_runtime_invoke(logMethod, nullptr, args, nullptr);
   ```
   预期：Console 输出一行 `[info] [P1.5 test] Hello from managed side`。若通过说明 `Debug_Log` 的 InternalCall 已被 mono 正确解析并调用。**验收后删除该测试代码**
5. **代码规范**：`ScriptGlue.h/.cpp` 通过以下人工 checklist：
   - 所有控制语句都带花括号（§5.2）
   - 无"为对齐而对齐"的空格（§5.4，[[memory:ebdvlsub]]）
   - 无引用外部文档的注释、无解释设计原则的说明性长注释（[[memory:ngdbzlni]]）
   - 无"P1.6 会补齐"这类阶段性注释（[[memory:du4s18ic]]）
   - 公有接口有 `/// <summary>` XML 文档注释（§4.1）
   - 智能指针使用 `Ref/Scope`（本 Phase 无智能指针使用，但仍需检查）
   - include 顺序符合 §3.3

---

## 8. 对下一 Phase 的接线点

本 Phase 完成后，这些钩子已就绪，等 P1.6 打开：

| 位置 | 后续 Phase | 打开方式 |
|------|-----------|---------|
| `ScriptEngine::OnCreateEntityScript` 内实例化分支 | **P1.6** | 取消注释，托管对象实例化后调 `InvokeAwake`，其内部执行 C# 侧 `Awake()`，此时若脚本用了 `Debug.Log` 或 `Transform.Position` 就会真正经过本 Phase 注册的 InternalCall 落到 native |
| `Scene::OnRuntimeStart / OnUpdateRuntime / OnRuntimeStop` | **P1.6** | 遍历 `view<ScriptComponent>` 分别调 `ScriptEngine::OnCreateEntityScript / OnUpdateEntityScript / OnRuntimeStop` |
| `s_EntityHasComponentFuncs` 追加新组件 | **Phase 2/3** | 每加一个托管 C# 组件类（如 `Lucky.CameraComponent`），在 `RegisterComponents` 内多写一行 `RegisterComponent<CameraComponent>();` |
| `Input_IsKeyDown` 等 InternalCall | **Phase 3** | 托管侧 [InternalCalls.cs](../../Lucky-ScriptCore/Source/Lucky/InternalCalls.cs) 追加桩；`ScriptGlue.cpp` 内追加 `Input_*` native 函数；`RegisterFunctions` 内 `LF_ADD_INTERNAL_CALL(Input_IsKeyDown);` |
| Hot reload 时重注册 | **Phase 3** | AppDomain unload 后重新 `LoadCoreAssembly / RegisterFunctions / RegisterComponents`??由于 `RegisterComponents` 开头 `clear()`，天然幂等 |

---

## 9. 变更清单速览

- **新增文件（2 个）**
  - `Lucky/Source/Lucky/Scripting/ScriptGlue.h`
  - `Lucky/Source/Lucky/Scripting/ScriptGlue.cpp`
- **修改文件（1 个）**
  - [ScriptEngine.cpp](../../Lucky/Source/Lucky/Scripting/ScriptEngine.cpp)：顶部 include `ScriptGlue.h`；`Init()` 末尾追加 `ScriptGlue::RegisterFunctions() / RegisterComponents()` 两行
- **删除**：无
- **不改动**：`ScriptEngine.h`、`ScriptComponent.h`、`Scene.h/.cpp`、`TransformComponent.h`、`InternalCalls.cs`、`Debug.cs`、`Component.cs`、`Entity.cs`、`Vector3.cs`、premake（新文件在同目录自动收录）

---

## 10. 参考

- Roadmap [ScriptSystem_Roadmap.md](ScriptSystem_Roadmap.md) 第 3 节 (5) "Internal Calls（最小集）"
- 前置详设 [Phase1.2_ScriptCore_Assembly.md](Phase1.2_ScriptCore_Assembly.md)（托管侧 6 个桩的最终形态）
- 前置详设 [Phase1.3_ScriptEngine_Runtime.md](Phase1.3_ScriptEngine_Runtime.md)（`GetCoreAssemblyImage / GetSceneContext / OnCreateEntityScript` 等接口约定）
- 前置详设 [Phase1.4_ScriptComponent.md](Phase1.4_ScriptComponent.md)（`ScriptComponent` 数据组件，本 Phase 不感知）
- 代码规范 [Coding_Style_Guide.md](../Coding_Style_Guide.md) 第 3 节（文件组织）、第 4 节（注释）、第 5 节（格式）、第 9 节（宏）、第 13.2 节（静态类模式）
- Mono Embedding 官方文档：<https://www.mono-project.com/docs/advanced/embedding/>
- Mono API `mono_add_internal_call`：<https://www.mono-project.com/docs/advanced/embedding/#internal-calls>
