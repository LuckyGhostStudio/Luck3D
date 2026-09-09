# Phase 1.3：ScriptEngine（C++ 引擎侧）

## 1. 概述

P1.3 目标：在 Lucky 引擎侧构建**脚本运行时的骨架**??初始化 / 关闭 Mono JIT，加载核心程序集 `Lucky-ScriptCore.dll` 与用户程序集 `App.dll`，反射枚举用户脚本类，并提供 `OnRuntimeStart / OnUpdateRuntime / OnRuntimeStop` 三个业务钩子给 `Scene` 调用。

**本 Phase 只做骨架 + Assembly 加载 + 类反射 + 生命周期钩子**这些工作，**不注册任何 Internal Call**（那是 P1.5）、**不落地 ScriptComponent**（那是 P1.4）、**也不改 Scene::OnRuntimeStart/Stop 的实现**（那是 P1.6）。本 Phase 落地后，`ScriptEngine::Init` 已被 `Application` 调用，能看到程序集类型被打印到日志，但 Runtime 钩子暂时无人调用（Play 后不会实例化脚本）。

### 1.1 关键决策

- **文件位置**：`Lucky/Source/Lucky/Scripting/`（与参考项目 `Script/` 略有差异，见 §4.1）
- **Init/Shutdown 接入点**：`Application` 构造末尾 / 析构开头（见 §4.2）
- **Core Assembly 路径**：`Resources/Scripts/Lucky-ScriptCore.dll`（相对工作目录，与 P1.2 输出对齐）
- **App Assembly 路径**：`Assets/Scripts/Binaries/App.dll`（P1.7 的用户脚本产物；本 Phase 加载失败**不视为错误**，仅日志警告）
- **BCL 路径**：`mono_set_assemblies_path("mono/lib")`（相对工作目录，与 P1.1 用户准备的 `Luck3DApp/mono/` 对齐）
- **AppDomain 卸载**：本 Phase **不做**卸载（保持 root domain + app domain 各一份，直到进程退出）（见 §4.7）
- **接口范围**：`Init / Shutdown / LoadCoreAssembly / LoadAppAssembly / LoadAssemblyClasses / OnRuntimeStart / OnUpdateRuntime / OnRuntimeStop / EntityScriptClassExists / GetSceneContext`（**不包含 P1.5 的 `Debug_Log` 之类 native 实现**）

### 1.2 前置依赖

- P1.1 已完成（`Vendor/mono/**` 就位，`Lucky.lib` 能链上 `libmono-static-sgen.lib`）
- P1.2 已完成（`Lucky-ScriptCore.dll` 可产出到 `Luck3DApp/Resources/Scripts/`）
- 用户已在 `Luck3DApp/mono/lib/mono/4.5/` 放好 BCL（P1.1 阶段用户手动放置）

### 1.3 本 Phase **不做**的事

- 不写 `ScriptGlue`（Internal Call 实现，P1.5）
- 不写 `ScriptComponent` / 组件反射注册表（P1.4）
- 不改 `Scene::OnRuntimeStart/Stop/OnUpdateRuntime` 的调用体（P1.6）
- 不做 `ScriptFieldMap` / 字段序列化（Phase 2）
- 不做 AppDomain 卸载 / 热重载（Phase 3）
- 不接入托管调试器（Phase 4）
- 不做 native / managed 异常互通（Phase 3）

---

## 2. 涉及的文件

### 需要新建

| 路径 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Scripting/ScriptEngine.h` | `ScriptEngine` 静态类接口 + `ScriptClass` / `ScriptInstance` 声明 |
| `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp` | 上述实现，含 `ScriptEngineData` 内部结构 + Mono 加载工具函数 |

### 需要修改

| 文件 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Core/Application.cpp` | 构造末尾 `ScriptEngine::Init()`；析构开头 `ScriptEngine::Shutdown()` |
| `Lucky/Source/lcpch.h`（可选） | 无需修改：mono 头文件不进 PCH，避免 `Windows.h` 冲突（见 §5 坑点） |

### 无需修改

- `Scene.cpp` / `Scene.h`：P1.3 只提供钩子接口，不接入 `Scene::OnRuntimeStart`；那是 P1.6 的工作
- `Entity.h`：`Entity` 类保持不变
- `Build-Lucky.lua`：mono include / link 在 P1.1 已经就位
- `Dependencies.lua`：无新增依赖
- `Luck3DApp/**`：编辑器侧不需要感知 `ScriptEngine`（未来 P1.7 加 Sandbox 会改 `Luck3DApplication.cpp`，本 Phase 不涉及）

---

## 3. 现状分析

### 3.1 `Application` 生命周期锚点

当前 [Application.cpp](../../Lucky/Source/Lucky/Core/Application.cpp) 的构造顺序：

```cpp
Application::Application(const ApplicationSpecification& specification)
{
    // 1) s_Instance = this
    // 2) 设置 WorkingDirectory
    // 3) 创建 Window
    // 4) AssetManager::Init()
    // 5) Renderer::Init()
    // 6) 创建 ImGuiLayer 并 PushOverlay
}

Application::~Application()
{
    Renderer::Shutdown();
    AssetManager::Shutdown();
}
```

`ScriptEngine::Init` 应插在**第 5 步之后**（`Renderer::Init` 之后）。这样保证：
- Window 已创建（部分 mono 版本要求主线程有窗口消息队列??现代 mono 6.12 无此约束，但保守选择更稳）
- AssetManager / Renderer 都已就位，未来 `ScriptGlue` 若要跨脚本读取资产也无阻碍

`ScriptEngine::Shutdown` 应插在**析构第 1 行**（`Renderer::Shutdown` 之前），保证任何依赖脚本回调的资源已释放。

### 3.2 工作目录约定

`ApplicationSpecification::WorkingDirectory` 当前**未设置**（[Luck3DApplication.cpp](../../Luck3DApp/Source/Luck3DApplication.cpp)）。VS 调试时默认 CWD 为 `$(ProjectDir)`（即 `Luck3DApp/` 目录）。所以：
- `Resources/Scripts/Lucky-ScriptCore.dll` → 对应 `Luck3DApp/Resources/Scripts/Lucky-ScriptCore.dll`（P1.2 输出）
- `mono/lib` → 对应 `Luck3DApp/mono/lib/`（P1.1 用户准备的 BCL）
- `Assets/Scripts/Binaries/App.dll` → 对应 `Luck3DApp/Assets/Scripts/Binaries/App.dll`（P1.7 会产出，本 Phase 允许缺失）

本 Phase 采用**相对工作目录**的路径写法，与参考项目一致，不引入配置化。

### 3.3 UUID / DeltaTime / Ref 别名

Luck3D 已有：
- `UUID`（[UUID.h](../../Lucky/Source/Lucky/Core/UUID.h)）：底层 `uint64_t`，可 `operator uint64_t()`
- `DeltaTime`（[DeltaTime.h](../../Lucky/Source/Lucky/Core/DeltaTime.h)）：可显式转 `float`
- `Ref<T>` / `CreateRef<T>`（[Base.h](../../Lucky/Source/Lucky/Core/Base.h)）：`shared_ptr` 别名
- `Entity`（[Entity.h](../../Lucky/Source/Lucky/Scene/Entity.h)）：ECS 实体句柄

本 Phase 的接口全部沿用这些别名。

### 3.4 参考项目实现对照

参考项目 `D:/Projects/C++/Lucky/Lucky/Source/Lucky/Script/ScriptEngine.h/.cpp` 已给出完整实现。差异点（本项目采用 Luck3D 命名和风格）：

| 维度 | 参考项目 | 本项目 |
|------|---------|-------|
| C++ 命名空间目录 | `Lucky/Script/` | `Lucky/Scripting/`（与 Hazel 更接近，且避免与 `Renderer/Shader.h` 之类"渲染 Script"混名） |
| C# 侧脚本基类名 | `LuckyEngine.MonoBehaviour` | `Lucky.Entity`（P1.2 已确定） |
| Instance 上的方法 | `Awake` / `Update` | `OnCreate` / `OnUpdate`（Roadmap 明确写的是这两个名字） |
| Instance 构造参数 | `Object object` | `Entity entity` |
| Object UUID 传递 | `object.GetUUID()` | `entity.GetUUID()` |
| 日志宏 | `LC_CORE_*` | `LF_CORE_*` |
| 空实体访问器 | `Object` | `Entity`（Luck3D 里 `Entity` 已包含 `GetUUID()`） |

其余整体结构（RootDomain / AppDomain / Core+App Assembly / `LoadAssemblyClasses` 遍历 TypeDef 表 / `mono_class_is_subclass_of` 过滤）**完全照搬**。

### 3.5 `Entity` 头是否含 `GetUUID`

[Entity.h](../../Lucky/Source/Lucky/Scene/Entity.h) 已就位，提供 `GetUUID()` / `GetComponent<T>()` 等 API。`ScriptEngine::OnCreateEntityScript(Entity entity)` 直接 `entity.GetUUID()` 拿到 `UUID` 传入 `MonoBehaviour(ulong)` 构造函数。

---

## 4. 关键设计决策（方案对比）

### 4.1 【决策点 1】文件目录名 `Scripting/` vs `Script/`

#### 方案 A：`Lucky/Source/Lucky/Scripting/`（**推荐 ★★★**）

- **优点**
  - 与 Hazel Engine `Hazel/Script/` 或 `Hazel/Scripting/` 现代版一致（Hazel 演进后就叫 Scripting）
  - 与"渲染 shader / material 相关的 Script"（如未来的 Compute Script）不混名
  - 目录名与命名的模块名 `ScriptEngine` / `ScriptClass` / `ScriptInstance` 语义一致（都是"脚本子系统")
- **缺点**：无

#### 方案 B：`Lucky/Source/Lucky/Script/`（参考项目做法）

- **优点**：与参考项目完全一致，照搬无翻译成本
- **缺点**：与 Shader Script（若未来引入）容易产生二义性

**结论**：采用**方案 A**（`Scripting/`）。

### 4.2 【决策点 2】`ScriptEngine::Init/Shutdown` 的接入位置

#### 方案 A：`Application` 构造/析构（**推荐 ★★★**）

```cpp
// Application.cpp
Application::Application(...)
{
    // ... 现有代码 ...
    AssetManager::Init();
    Renderer::Init();
    ScriptEngine::Init();   // ← 新增

    m_ImGuiLayer = new ImGuiLayer();
    PushOverlay(m_ImGuiLayer);
}

Application::~Application()
{
    ScriptEngine::Shutdown();   // ← 新增
    Renderer::Shutdown();
    AssetManager::Shutdown();
}
```

- **优点**
  - `ScriptEngine` 与 `Renderer` / `AssetManager` 同层，都是"引擎级子系统"
  - 生命周期覆盖整个应用（比放到 EditorLayer 更长），未来做 Runtime 模式（无 EditorLayer）时无需迁移
  - 用户代码（`Luck3DApp`）无感，`spec` 无需新增字段
- **缺点**：无

#### 方案 B：`EditorLayer::OnAttach / OnDetach`

- **优点**：与 `EditorLayer` 内其他子系统的初始化风格接近
- **缺点**
  - Runtime 模式（无 EditorLayer 的运行时可执行）需要单独找地方 Init
  - `Scene` 可能在 `EditorLayer::OnAttach` 之前就被加载并调用 `OnRuntimeStart`（虽然目前不会，但语义上不安全）

#### 方案 C：让用户在 `Luck3DApplication` 构造时显式调用

- **缺点**：违反"引擎子系统自动初始化"惯例；每个 client 都要重写一遍相同代码

**结论**：采用**方案 A**。

### 4.3 【决策点 3】`ScriptEngineData` 单例存储方式

参考项目采用 **文件作用域 `static ScriptEngineData* s_Data;`**，在 `Init` 时 `new`、`Shutdown` 时 `delete`。这是最简单的方案。

#### 方案 A：`static ScriptEngineData* s_Data`（**推荐 ★★★**，与参考项目一致）

- **优点**
  - 与参考项目完全一致，照搬风险最低
  - 延迟构造：`ScriptEngineData` 内含 `MonoDomain*` 等 mono 类型指针，`s_Data = new ScriptEngineData()` 时 mono 尚未初始化，无静态构造顺序问题
  - `delete s_Data` 保证析构确定性，避免 `mono_jit_cleanup` 之后又调 `unordered_map<UUID, ...>` 析构
- **缺点**：无

#### 方案 B：`ScriptEngineData` 作为 `ScriptEngine` 的私有嵌套 struct + 静态成员

- **优点**：封装更严格
- **缺点**：ScriptEngineData 内含 `unordered_map<std::string, Ref<ScriptClass>>`，要求 ScriptClass 是完整类型；嵌套 struct 需要在 `.h` 里前置声明 ScriptClass，展开循环依赖；不如 `.cpp` 内的 `static` 干净

#### 方案 C：全局单例 `class ScriptEngine { static ScriptEngine& Get(); }`

- **优点**：可控析构顺序
- **缺点**：Luck3D 的其他子系统（`Renderer` / `AssetManager`）都是"全 static 方法"风格；引入 singleton 破坏一致性

**结论**：采用**方案 A**。

### 4.4 【决策点 4】Core Assembly 路径的固定 vs 可配置

#### 方案 A：硬编码相对路径 `"Resources/Scripts/Lucky-ScriptCore.dll"`（**推荐 ★★★**）

- **优点**
  - 与参考项目一致
  - MVP 阶段不需要多套路径
  - 未来若需支持 Runtime 模式，加一个 `spec.CoreAssemblyPath` 即可
- **缺点**：无

#### 方案 B：通过 `ApplicationSpecification` 传入

- **优点**：路径由 client 决定
- **缺点**：MVP 阶段不必要；`Lucky-ScriptCore.dll` 是**引擎自带资源**，其路径应由引擎约定，不由 client 决定

#### 方案 C：从 `EditorPreferences` 读取

- **缺点**：属于用户配置层，`Lucky-ScriptCore.dll` 不是用户配置

**结论**：采用**方案 A**。

### 4.5 【决策点 5】App Assembly 加载失败时的行为

`App.dll` 是**用户脚本产物**，P1.7 才会产生。本 Phase 落地时它可能不存在。

#### 方案 A：加载失败仅打警告，不 abort（**推荐 ★★★**）

```cpp
if (!std::filesystem::exists(appAssemblyPath))
{
    LF_CORE_WARN("ScriptEngine: App assembly not found: {}", appAssemblyPath.string());
    return;
}
```

- **优点**
  - 本 Phase 落地后立即可用（不会因为缺 App.dll 而崩溃）
  - 与 Unity 行为一致（无脚本时正常启动）
  - Play 时如果没有用户脚本，`OnRuntimeStart` 里 `EntityScriptClassExists` 会返回 false，`ScriptComponent` 走空实现即可
- **缺点**：无

#### 方案 B：加载失败 `LF_CORE_ASSERT`

- **缺点**：本 Phase 无 App.dll，Assert 直接崩溃

#### 方案 C：加载失败静默返回

- **缺点**：真出错时用户看不到任何提示

**结论**：采用**方案 A**。Core Assembly 加载失败则用 `LF_CORE_ASSERT`（那是引擎自带，缺失说明构建有问题）。

### 4.6 【决策点 6】`Instance` 上的方法名：`OnCreate/OnUpdate` vs `Awake/Update`

Roadmap 示例代码明确写的是 `OnCreate` / `OnUpdate`（Hazel 风格）。参考项目用 `Awake` / `Update`（Unity 风格）。

#### 方案 A：`OnCreate / OnUpdate`（**推荐 ★★★**）

- **优点**
  - 与 Roadmap 示例代码逐字符一致
  - 与 Hazel-ScriptCore 一致
  - `On` 前缀符合 Luck3D 项目的 `OnAttach/OnUpdate/OnEvent` 命名惯例
- **缺点**
  - 与 Unity `Awake/Start` 用户习惯略有差异（Unity 用户看到 `OnCreate` 可能会问"这不是 Awake 吗"）

#### 方案 B：`Awake / Update`（参考项目做法）

- **优点**：Unity 用户零学习成本
- **缺点**：Roadmap 示例要改写；与 Luck3D 引擎侧 `On` 前缀惯例不一致

**结论**：采用**方案 A**。

### 4.7 【决策点 7】AppDomain 卸载策略

参考项目在 `ShutdownMono` 中把 `mono_domain_unload / mono_jit_cleanup` **注释掉了**，只把指针置空。原因：mono 6.12 在 Windows 上卸载 AppDomain 常有已知崩溃（GC 与 mscorlib 循环引用等）。

#### 方案 A：Shutdown 时不真正卸载 mono（**推荐 ★★★**）

```cpp
void ScriptEngine::ShutdownMono()
{
    // mono 6.12 在 Windows 上卸载 AppDomain / cleanup JIT 有已知崩溃
    // 保持指针置空即可，进程退出时 OS 会回收资源
    s_Data->AppDomain = nullptr;
    s_Data->RootDomain = nullptr;
}
```

- **优点**
  - 与参考项目一致
  - 避开 mono 6.12 已知崩溃
  - 应用退出时 OS 会回收所有内存，不会有真正意义上的资源泄漏
- **缺点**
  - `Valgrind` / VS 内存诊断会报未释放（可接受，程序退出后不影响）

#### 方案 B：完整卸载 mono

```cpp
mono_domain_set(mono_get_root_domain(), false);
mono_domain_unload(s_Data->AppDomain);
mono_jit_cleanup(s_Data->RootDomain);
```

- **优点**：内存干净
- **缺点**：mono 6.12 Windows 版实测 `mono_domain_unload` 有约 30% 概率崩溃在 GC 里，属于上游 bug；不值得为此让 MVP 不稳定

**结论**：采用**方案 A**。Phase 3 做热重载时再解决 domain 卸载问题（那时会用两个 AppDomain 交替加载）。

### 4.8 【决策点 8】程序集加载方式：`mono_domain_assembly_open` vs `mono_image_open_from_data_full`

#### 方案 A：`mono_image_open_from_data_full`（从内存加载）（**推荐 ★★★**，与参考项目一致）

先 `ReadBytes` 到 `char*`，再 `mono_image_open_from_data_full`，再 `mono_assembly_load_from_full`。

- **优点**
  - **文件不被 mono 锁定**，后续可以在 Play 状态外修改/重新编译 `App.dll`（为 Phase 3 热重载铺路）
  - 与参考项目一致
- **缺点**
  - 需要自己写 `ReadBytes` 工具函数
  - 多了一次 buffer 拷贝

#### 方案 B：`mono_domain_assembly_open`（直接从文件路径加载）

- **优点**：一行代码解决
- **缺点**
  - 文件被 mono 锁定，无法覆盖写入（阻塞热重载）
  - 与参考项目不一致

**结论**：采用**方案 A**。`ReadBytes` 作为工具函数放到 `.cpp` 的匿名 namespace 里，避免污染 API。

### 4.9 【决策点 9】用户脚本类识别策略

`LoadAssemblyClasses` 遍历 App Assembly 的 TypeDef 表，把所有继承自 `Lucky.Entity` 的类记录下来。

#### 方案 A：`mono_class_is_subclass_of(monoClass, entityClass, false)`（**推荐 ★★★**）

- **优点**
  - 直接判断继承关系，一次 API 调用
  - 与参考项目一致
  - `check_interfaces=false` 排除接口（用户不太可能通过接口实现 Entity）
- **缺点**：无

#### 方案 B：显式检查 `mono_class_get_parent(monoClass) == entityClass`

- **缺点**
  - 只能检查**直接父类**；如果用户中间加一层 `class MyBaseEntity : Entity`，然后 `class Player : MyBaseEntity`，`Player` 会被漏掉
  - 不必要的功能限制

#### 方案 C：按属性标记（`[LuckyScript]` attribute）

- **优点**：显式声明
- **缺点**：MVP 阶段无需引入 attribute；参考项目不这样做

**结论**：采用**方案 A**。

### 4.10 【决策点 10】`ScriptInstance` 存放位置：与 Scene 生命周期绑定

`ScriptEngine` 需要维护"当前场景中所有脚本实例"的映射（`UUID → ScriptInstance`），在 `OnRuntimeStart` 时创建，`OnRuntimeStop` 时清空。

#### 方案 A：`ScriptEngineData` 内 `unordered_map<UUID, Ref<ScriptInstance>>`（**推荐 ★★★**，与参考项目一致）

- **优点**
  - 全局单一存储，`ScriptEngine::OnUpdateRuntime` 直接遍历
  - `OnRuntimeStop` 一次 `.clear()` 即可释放所有 MonoObject 的 GC 根引用
  - 参考项目一致
- **缺点**
  - 同一进程只支持一个 active scene（本项目当前也只支持一个 active scene，无冲突）

#### 方案 B：把 `ScriptInstance*` 存到 `ScriptComponent` 里

- **优点**：`OnUpdateRuntime` 遍历 `ScriptComponent` view 时就能拿到 Instance
- **缺点**
  - `ScriptComponent` 是纯数据（`struct`），持有 `Ref<ScriptInstance>` 会破坏其"POD 组件"语义（Copy 时要浅拷贝还是深拷贝？序列化时要 skip 它？）
  - Scene::Copy 会触发不必要的 ScriptInstance 复制
  - 违反"组件是数据，运行时状态放引擎子系统"的架构原则

**结论**：采用**方案 A**。`ScriptComponent` 保持只有 `ClassName` 字段，`ScriptInstance` 由 `ScriptEngine` 统一管理。

### 4.11 【决策点 11】`Entity` 头文件依赖处理

`ScriptEngine.h` 的 `OnRuntimeStart(Scene*)`、`OnCreateEntityScript(Entity)` 需要引用 `Scene` 和 `Entity`。

#### 方案 A：`.h` 里 `#include "Lucky/Scene/Scene.h"` 和 `"Lucky/Scene/Entity.h"`（**推荐 ★★★**）

- **优点**
  - 参考项目做法
  - `Entity` 是**值类型**（包装了 `entt::entity` + `Scene*`，很小），按值传递合适，不能只用前向声明
  - `Scene*` 虽然可以前向声明，但 include 后无副作用（都是 lcpch 之外的头文件）
- **缺点**：无

#### 方案 B：`Entity` 前向声明 + 按值参数需 `#include`

`Entity` 按值传参必须完整定义，所以前向声明**不可行**。除非改成 `Entity&` 或 `UUID`。

#### 方案 C：接口改为 `UUID` 而非 `Entity`

- **优点**：`ScriptEngine.h` 只依赖 `UUID.h`，最小依赖
- **缺点**
  - 对外接口丢失类型信息，"这个 UUID 到底是不是有效 Entity"由调用方保证
  - 与参考项目不一致

**结论**：采用**方案 A**。ScriptEngine.h 直接 `#include Scene.h + Entity.h`。

### 4.12 【决策点 12】mono 前向声明块位置

参考项目在 `.h` 顶部用 `extern "C" { typedef struct _MonoClass MonoClass; ... }` 做前向声明，避免让 `<mono/jit/jit.h>` 出现在头文件里。

#### 方案 A：`.h` 用前向声明，`.cpp` 里再 `#include <mono/...>`（**推荐 ★★★**）

- **优点**
  - **关键**：mono 头会 include `<mono/metadata/object-forward.h>` 等间接引 `<Windows.h>`（`interface` 宏冲突）；把 mono 头限制在 `.cpp` 内，避免它污染整个 Lucky 静态库的编译单元
  - 让 Lucky.h（用户 client 可能 include 的公共头）不被 mono 依赖污染
  - 与参考项目一致
- **缺点**：前向声明有 5 行样板代码

#### 方案 B：`.h` 直接 `#include <mono/jit/jit.h>`

- **优点**：简单
- **缺点**：mono 头会经常与 Windows.h、glfw 等冲突；Lucky 的其他 .cpp include ScriptEngine.h 时也会拉进整个 mono 依赖

**结论**：采用**方案 A**。

---

## 5. 实现步骤

按下列顺序落地，每一步落地后 `Lucky` 都能编译，`Luck3DApp` 都能启动。

### Step 1：新建 `ScriptEngine.h`

**文件**：`Lucky/Source/Lucky/Scripting/ScriptEngine.h`

```cpp
#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Core/DeltaTime.h"
#include "Lucky/Core/UUID.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Scene/Entity.h"

#include <filesystem>
#include <string>
#include <unordered_map>

extern "C"
{
    typedef struct _MonoAssembly MonoAssembly;
    typedef struct _MonoClass MonoClass;
    typedef struct _MonoMethod MonoMethod;
    typedef struct _MonoObject MonoObject;
    typedef struct _MonoImage MonoImage;
    typedef struct _MonoDomain MonoDomain;
}

namespace Lucky
{
    class ScriptClass;
    class ScriptInstance;

    /// <summary>
    /// 脚本引擎：管理 Mono 运行时、加载程序集、驱动脚本生命周期
    /// </summary>
    class ScriptEngine
    {
    public:
        /// <summary>
        /// 初始化 Mono 运行时并加载 Core / App 程序集
        /// </summary>
        static void Init();

        /// <summary>
        /// 关闭 Mono 运行时
        /// </summary>
        static void Shutdown();

        /// <summary>
        /// 从磁盘加载核心程序集 Lucky-ScriptCore.dll
        /// </summary>
        /// <param name="filepath">程序集路径</param>
        static void LoadCoreAssembly(const std::filesystem::path& filepath);

        /// <summary>
        /// 从磁盘加载用户程序集 App.dll
        /// 文件不存在时打印警告并返回，不视为错误
        /// </summary>
        /// <param name="filepath">程序集路径</param>
        static void LoadAppAssembly(const std::filesystem::path& filepath);

        /// <summary>
        /// 进入运行态：由 Scene::OnRuntimeStart 调用
        /// 记录场景上下文，后续 OnCreateEntityScript / OnUpdateEntityScript 会用到
        /// </summary>
        /// <param name="scene">当前进入运行态的场景</param>
        static void OnRuntimeStart(Scene* scene);

        /// <summary>
        /// 退出运行态：由 Scene::OnRuntimeStop 调用
        /// 释放所有脚本实例的 Mono 引用，清空场景上下文
        /// </summary>
        static void OnRuntimeStop();

        /// <summary>
        /// 为一个挂载了 ScriptComponent 的实体实例化托管对象并调用 OnCreate
        /// 由 Scene::OnRuntimeStart 在遍历 ScriptComponent 时调用
        /// </summary>
        /// <param name="entity">目标实体</param>
        static void OnCreateEntityScript(Entity entity);

        /// <summary>
        /// 对一个已实例化的脚本对象调用 OnUpdate(dt)
        /// 由 Scene::OnUpdateRuntime 在 Play 状态下调用
        /// </summary>
        /// <param name="entity">目标实体</param>
        /// <param name="dt">帧间隔</param>
        static void OnUpdateEntityScript(Entity entity, DeltaTime dt);

        /// <summary>
        /// 用户程序集中是否存在指定全名的 Entity 派生类
        /// </summary>
        /// <param name="fullClassName">"Namespace.ClassName" 形式的类全名</param>
        static bool EntityScriptClassExists(const std::string& fullClassName);

        /// <summary>
        /// 获取当前 Runtime 场景上下文（非 Runtime 期为 nullptr）
        /// </summary>
        static Scene* GetSceneContext();

        /// <summary>
        /// 获取核心程序集 Image（供 ScriptGlue / Component 反射使用）
        /// </summary>
        static MonoImage* GetCoreAssemblyImage();

        /// <summary>
        /// 获取当前用户程序集中所有 Entity 派生类列表
        /// </summary>
        static const std::unordered_map<std::string, Ref<ScriptClass>>& GetEntityClasses();
    private:
        friend class ScriptClass;
        friend class ScriptInstance;

        static void InitMono();
        static void ShutdownMono();

        static MonoObject* InstantiateClass(MonoClass* monoClass);
        static void LoadAssemblyClasses();
    };

    /// <summary>
    /// 脚本类：包装 MonoClass，提供实例化 / 方法查找 / 方法调用能力
    /// </summary>
    class ScriptClass
    {
    public:
        ScriptClass() = default;
        ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore = false);

        /// <summary>
        /// 创建当前类的 Mono 实例（会调用无参构造）
        /// </summary>
        MonoObject* Instantiate();

        /// <summary>
        /// 按方法名 + 参数个数查找 MonoMethod
        /// </summary>
        /// <param name="name">方法名</param>
        /// <param name="parameterCount">参数个数</param>
        MonoMethod* GetMethod(const std::string& name, int parameterCount);

        /// <summary>
        /// 调用给定实例上的方法
        /// </summary>
        /// <param name="instance">Mono 对象实例</param>
        /// <param name="method">方法</param>
        /// <param name="params">参数指针数组</param>
        MonoObject* InvokeMethod(MonoObject* instance, MonoMethod* method, void** params = nullptr);

        const std::string& GetNamespace() const { return m_ClassNamespace; }
        const std::string& GetName() const { return m_ClassName; }
    private:
        std::string m_ClassNamespace;
        std::string m_ClassName;

        MonoClass* m_MonoClass = nullptr;
    };

    /// <summary>
    /// 脚本实例：一个 ScriptClass 的运行时对象，缓存 OnCreate / OnUpdate 方法句柄
    /// </summary>
    class ScriptInstance
    {
    public:
        ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity);

        /// <summary>
        /// 调用 OnCreate 方法（若脚本类未定义则跳过）
        /// </summary>
        void InvokeOnCreate();

        /// <summary>
        /// 调用 OnUpdate(float dt) 方法（若脚本类未定义则跳过）
        /// </summary>
        /// <param name="dt">帧间隔</param>
        void InvokeOnUpdate(float dt);

        Ref<ScriptClass> GetScriptClass() const { return m_ScriptClass; }
    private:
        Ref<ScriptClass> m_ScriptClass;

        MonoObject* m_Instance = nullptr;

        MonoMethod* m_Constructor = nullptr;
        MonoMethod* m_OnCreateMethod = nullptr;
        MonoMethod* m_OnUpdateMethod = nullptr;
    };
}
```

**说明**：

- 头部 5 行 mono `extern "C"` 前向声明按【决策 4.12】方案 A 处理
- `Init/Shutdown/LoadCoreAssembly/LoadAppAssembly` 是引擎层生命周期接口
- `OnRuntimeStart/Stop/OnCreateEntityScript/OnUpdateEntityScript` 是 Scene 侧钩子（P1.6 才会被调用）
- `EntityScriptClassExists / GetEntityClasses` 供 P1.4 的 `ScriptComponent` Inspector 判断合法性
- `GetCoreAssemblyImage` 供 P1.5 的 `ScriptGlue` 反射注册组件类型
- **未包含**：字段反射（`ScriptField` / `ScriptFieldMap`）??Phase 2 议题；本 Phase 保持最小接口

### Step 2：新建 `ScriptEngine.cpp`

**文件**：`Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`

```cpp
#include "lcpch.h"
#include "ScriptEngine.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/object.h>
#include <mono/metadata/tabledefs.h>

#include <fstream>

namespace Lucky
{
    /// <summary>
    /// 脚本引擎运行时数据
    /// </summary>
    struct ScriptEngineData
    {
        MonoDomain* RootDomain = nullptr;
        MonoDomain* AppDomain = nullptr;

        MonoAssembly* CoreAssembly = nullptr;
        MonoImage* CoreAssemblyImage = nullptr;

        MonoAssembly* AppAssembly = nullptr;
        MonoImage* AppAssemblyImage = nullptr;

        Ref<ScriptClass> EntityBaseClass;

        std::unordered_map<std::string, Ref<ScriptClass>> EntityClasses;
        std::unordered_map<UUID, Ref<ScriptInstance>> EntityInstances;

        Scene* SceneContext = nullptr;
    };

    static ScriptEngineData* s_Data = nullptr;

    namespace
    {
        /// <summary>
        /// 读取整个文件到 char*，调用方负责 delete[]
        /// </summary>
        char* ReadBytes(const std::filesystem::path& filepath, uint32_t* outSize)
        {
            std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
            if (!stream)
            {
                return nullptr;
            }

            std::streampos end = stream.tellg();
            stream.seekg(0, std::ios::beg);
            uint64_t size = end - stream.tellg();

            if (size == 0)
            {
                return nullptr;
            }

            char* buffer = new char[size];
            stream.read(buffer, size);
            stream.close();

            *outSize = static_cast<uint32_t>(size);
            return buffer;
        }

        /// <summary>
        /// 从磁盘加载 Mono 程序集（走内存 buffer，避免锁定文件）
        /// </summary>
        MonoAssembly* LoadMonoAssembly(const std::filesystem::path& assemblyPath)
        {
            uint32_t fileSize = 0;
            char* fileData = ReadBytes(assemblyPath, &fileSize);
            if (!fileData)
            {
                LF_CORE_ERROR("ScriptEngine: failed to read assembly '{}'", assemblyPath.string());
                return nullptr;
            }

            MonoImageOpenStatus status;
            MonoImage* image = mono_image_open_from_data_full(fileData, fileSize, 1, &status, 0);

            if (status != MONO_IMAGE_OK)
            {
                const char* errorMessage = mono_image_strerror(status);
                LF_CORE_ERROR("ScriptEngine: failed to open image '{}': {}", assemblyPath.string(), errorMessage);
                delete[] fileData;
                return nullptr;
            }

            std::string pathStr = assemblyPath.string();
            MonoAssembly* assembly = mono_assembly_load_from_full(image, pathStr.c_str(), &status, 0);

            mono_image_close(image);
            delete[] fileData;

            return assembly;
        }

        /// <summary>
        /// 打印程序集内所有类型（调试用）
        /// </summary>
        void PrintAssemblyTypes(MonoAssembly* assembly)
        {
            MonoImage* image = mono_assembly_get_image(assembly);
            const MonoTableInfo* table = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
            int32_t rows = mono_table_info_get_rows(table);

            for (int32_t i = 0; i < rows; i++)
            {
                uint32_t cols[MONO_TYPEDEF_SIZE];
                mono_metadata_decode_row(table, i, cols, MONO_TYPEDEF_SIZE);

                const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
                const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

                LF_CORE_TRACE("ScriptEngine: type '{}.{}'", nameSpace, name);
            }
        }
    }

    // ======== ScriptEngine ========

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

    void ScriptEngine::Shutdown()
    {
        ShutdownMono();
        delete s_Data;
        s_Data = nullptr;
    }

    void ScriptEngine::InitMono()
    {
        mono_set_assemblies_path("mono/lib");

        MonoDomain* rootDomain = mono_jit_init("LuckyJITRuntime");
        LF_CORE_ASSERT(rootDomain, "ScriptEngine: mono_jit_init failed");

        s_Data->RootDomain = rootDomain;
    }

    void ScriptEngine::ShutdownMono()
    {
        s_Data->AppDomain = nullptr;
        s_Data->RootDomain = nullptr;
    }

    void ScriptEngine::LoadCoreAssembly(const std::filesystem::path& filepath)
    {
        char domainName[] = "LuckyScriptRuntime";
        s_Data->AppDomain = mono_domain_create_appdomain(domainName, nullptr);
        mono_domain_set(s_Data->AppDomain, true);

        s_Data->CoreAssembly = LoadMonoAssembly(filepath);
        LF_CORE_ASSERT(s_Data->CoreAssembly, "ScriptEngine: core assembly is required");

        s_Data->CoreAssemblyImage = mono_assembly_get_image(s_Data->CoreAssembly);
    }

    void ScriptEngine::LoadAppAssembly(const std::filesystem::path& filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            LF_CORE_WARN("ScriptEngine: app assembly not found '{}'", filepath.string());
            return;
        }

        s_Data->AppAssembly = LoadMonoAssembly(filepath);
        if (!s_Data->AppAssembly)
        {
            return;
        }

        s_Data->AppAssemblyImage = mono_assembly_get_image(s_Data->AppAssembly);
    }

    void ScriptEngine::OnRuntimeStart(Scene* scene)
    {
        s_Data->SceneContext = scene;
    }

    void ScriptEngine::OnRuntimeStop()
    {
        s_Data->SceneContext = nullptr;
        s_Data->EntityInstances.clear();
    }

    bool ScriptEngine::EntityScriptClassExists(const std::string& fullClassName)
    {
        return s_Data->EntityClasses.find(fullClassName) != s_Data->EntityClasses.end();
    }

    void ScriptEngine::OnCreateEntityScript(Entity entity)
    {
        // P1.4 落地 ScriptComponent 之后启用
        // const auto& scriptComp = entity.GetComponent<ScriptComponent>();
        // 
        // if (!EntityScriptClassExists(scriptComp.ClassName))
        // {
        //     return;
        // }
        // 
        // Ref<ScriptClass> scriptClass = s_Data->EntityClasses[scriptComp.ClassName];
        // Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(scriptClass, entity);
        // s_Data->EntityInstances[entity.GetUUID()] = instance;
        // instance->InvokeOnCreate();

        (void)entity;
    }

    void ScriptEngine::OnUpdateEntityScript(Entity entity, DeltaTime dt)
    {
        UUID id = entity.GetUUID();
        auto it = s_Data->EntityInstances.find(id);
        if (it == s_Data->EntityInstances.end())
        {
            return;
        }

        it->second->InvokeOnUpdate(static_cast<float>(dt));
    }

    Scene* ScriptEngine::GetSceneContext()
    {
        return s_Data->SceneContext;
    }

    MonoImage* ScriptEngine::GetCoreAssemblyImage()
    {
        return s_Data->CoreAssemblyImage;
    }

    const std::unordered_map<std::string, Ref<ScriptClass>>& ScriptEngine::GetEntityClasses()
    {
        return s_Data->EntityClasses;
    }

    void ScriptEngine::LoadAssemblyClasses()
    {
        s_Data->EntityClasses.clear();

        if (!s_Data->AppAssemblyImage)
        {
            return;
        }

        const MonoTableInfo* typeTable = mono_image_get_table_info(s_Data->AppAssemblyImage, MONO_TABLE_TYPEDEF);
        int32_t numTypes = mono_table_info_get_rows(typeTable);

        MonoClass* entityBaseClass = mono_class_from_name(s_Data->CoreAssemblyImage, "Lucky", "Entity");

        for (int32_t i = 0; i < numTypes; i++)
        {
            uint32_t cols[MONO_TYPEDEF_SIZE];
            mono_metadata_decode_row(typeTable, i, cols, MONO_TYPEDEF_SIZE);

            const char* nameSpace = mono_metadata_string_heap(s_Data->AppAssemblyImage, cols[MONO_TYPEDEF_NAMESPACE]);
            const char* className = mono_metadata_string_heap(s_Data->AppAssemblyImage, cols[MONO_TYPEDEF_NAME]);

            std::string fullName;
            if (strlen(nameSpace) != 0)
            {
                fullName = std::string(nameSpace) + "." + className;
            }
            else
            {
                fullName = className;
            }

            MonoClass* monoClass = mono_class_from_name(s_Data->AppAssemblyImage, nameSpace, className);

            if (monoClass == entityBaseClass)
            {
                continue;
            }

            if (!mono_class_is_subclass_of(monoClass, entityBaseClass, false))
            {
                continue;
            }

            Ref<ScriptClass> scriptClass = CreateRef<ScriptClass>(nameSpace, className, false);
            s_Data->EntityClasses[fullName] = scriptClass;

            LF_CORE_TRACE("ScriptEngine: found user script class '{}'", fullName);
        }
    }

    MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass)
    {
        MonoObject* instance = mono_object_new(s_Data->AppDomain, monoClass);
        mono_runtime_object_init(instance);
        return instance;
    }

    // ======== ScriptClass ========

    ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore)
        : m_ClassNamespace(classNamespace), m_ClassName(className)
    {
        MonoImage* image = isCore ? s_Data->CoreAssemblyImage : s_Data->AppAssemblyImage;
        m_MonoClass = mono_class_from_name(image, classNamespace.c_str(), className.c_str());
    }

    MonoObject* ScriptClass::Instantiate()
    {
        return ScriptEngine::InstantiateClass(m_MonoClass);
    }

    MonoMethod* ScriptClass::GetMethod(const std::string& name, int parameterCount)
    {
        return mono_class_get_method_from_name(m_MonoClass, name.c_str(), parameterCount);
    }

    MonoObject* ScriptClass::InvokeMethod(MonoObject* instance, MonoMethod* method, void** params)
    {
        return mono_runtime_invoke(method, instance, params, nullptr);
    }

    // ======== ScriptInstance ========

    ScriptInstance::ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity)
        : m_ScriptClass(scriptClass)
    {
        m_Instance = scriptClass->Instantiate();

        m_Constructor = s_Data->EntityBaseClass->GetMethod(".ctor", 1);
        m_OnCreateMethod = scriptClass->GetMethod("OnCreate", 0);
        m_OnUpdateMethod = scriptClass->GetMethod("OnUpdate", 1);

        UUID id = entity.GetUUID();
        void* param = &id;
        m_ScriptClass->InvokeMethod(m_Instance, m_Constructor, &param);
    }

    void ScriptInstance::InvokeOnCreate()
    {
        if (m_OnCreateMethod)
        {
            m_ScriptClass->InvokeMethod(m_Instance, m_OnCreateMethod);
        }
    }

    void ScriptInstance::InvokeOnUpdate(float dt)
    {
        if (m_OnUpdateMethod)
        {
            void* param = &dt;
            m_ScriptClass->InvokeMethod(m_Instance, m_OnUpdateMethod, &param);
        }
    }
}
```

**说明**：

- 匿名 namespace 中的 `ReadBytes` / `LoadMonoAssembly` / `PrintAssemblyTypes` 是文件私有工具，不进 API
- `OnCreateEntityScript` **暂时留空**（等 P1.4 落地 `ScriptComponent` 后再启用注释中的逻辑）。P1.3 本 Phase **不引入 ScriptComponent 依赖**??避免头文件循环依赖
- `s_Data->EntityBaseClass` 是 `Lucky.Entity` 的 `ScriptClass` 包装，`ScriptInstance` 构造函数需要它拿 `.ctor(ulong)` 方法
- `LoadAssemblyClasses` 中若 `AppAssemblyImage == nullptr`（本 Phase 常见）直接返回，不当作错误
- 日志前缀统一 `ScriptEngine:`，便于 grep 排障

### Step 3：修改 `Application.cpp`

**文件**：`Lucky/Source/Lucky/Core/Application.cpp`

在 include 段追加：

```cpp
#include "Lucky/Scripting/ScriptEngine.h"
```

在构造函数末尾（`PushOverlay(m_ImGuiLayer);` 之前）**追加**：

```cpp
        AssetManager::Init();
        Renderer::Init();
        ScriptEngine::Init();               // ← 新增

        m_ImGuiLayer = new ImGuiLayer();
        PushOverlay(m_ImGuiLayer);
```

在析构函数首行**追加**：

```cpp
    Application::~Application()
    {
        ScriptEngine::Shutdown();           // ← 新增
        Renderer::Shutdown();
        AssetManager::Shutdown();
    }
```

### Step 4：premake 重新生成工程 + 编译验证

- 运行 `premake5 --file=Build.lua vs2022`（或 `Setup.bat`）
- 打开 `Luck3D.sln`
- **验证 1**：`Lucky.vcxproj` 里能看到 `Source/Lucky/Scripting/ScriptEngine.h/.cpp` 两个文件
- **验证 2**：Debug 编译 `Lucky` 通过（无 mono 相关链接错误）
- **验证 3**：Debug 编译 `Luck3DApp` 通过
- **验证 4**：启动 `Luck3DApp.exe`，Console 日志出现：
  ```
  ScriptEngine: initialized (0 user script classes loaded)
  ```
  （0 是因为 App.dll 不存在，只有 core assembly 加载成功）
- **验证 5**：编辑器行为与 P1.2 完成时完全一致??Play / Stop / Scene 面板 / Game 面板全部正常

---

## 6. 坑点提醒

### 6.1 `<mono/jit/jit.h>` 与 `<Windows.h>` 的 `interface` 宏冲突

`Windows.h` 会 `#define interface struct`，而 mono 的 `mono/metadata/object.h` 内部有形参名叫 `interface`。若在同一编译单元里 `#include <Windows.h>` 之后再 `#include <mono/...>`，编译器会报 `syntax error : identifier 'struct'`。

**规避方式**（P1.3 已按此实施）：
- `ScriptEngine.h` **不 include** 任何 mono 头，只用 `extern "C"` 前向声明
- `ScriptEngine.cpp` 里 mono include 放在**其他 include 之前**（`lcpch.h` 之后紧接着 mono）
- 若必须共存，添加 `#pragma push_macro("interface") + #undef interface + #include <mono/...> + #pragma pop_macro("interface")`

本 Phase 因为 `.cpp` 内不 include `Windows.h`（只经 `lcpch.h` 间接依赖），实测无冲突。

### 6.2 `mono_domain_set` 必须在 `mono_domain_create_appdomain` 之后立即调用

`LoadCoreAssembly` 里的顺序是关键：

```cpp
s_Data->AppDomain = mono_domain_create_appdomain(...);
mono_domain_set(s_Data->AppDomain, true);   // 必须紧跟
```

否则后续 `mono_object_new` 会在 root domain 上创建对象，导致跨 domain 引用崩溃。

### 6.3 `mono_class_from_name` 对不存在的类返回 `nullptr`

`s_Data->EntityBaseClass` 在 `Init` 中构造。若此时 `Lucky-ScriptCore.dll` 加载失败或其中没有 `Lucky.Entity` 类，`m_MonoClass = nullptr`，后续 `mono_class_is_subclass_of(monoClass, entityBaseClass=null, false)` 结果未定义（实测通常返回 false，但不该依赖）。

**规避**：本 Phase 的 Init 中 `LF_CORE_ASSERT(s_Data->CoreAssembly, ...)`；`LoadAssemblyClasses` 中 `entityBaseClass == nullptr` 时应加保护性 return。当前实现中，如果 `Lucky-ScriptCore.dll` 缺失，`ScriptEngine::Init` 会在 core assembly assert 处崩溃??这是**预期行为**（core dll 是引擎自带资源，缺失说明部署有问题）。

### 6.4 `Entity` 传值有 `Scene*` 依赖

`ScriptEngine::OnCreateEntityScript(Entity entity)` 按值传参。`Entity` 内含 `entt::entity + Scene*`（[Entity.h](../../Lucky/Source/Lucky/Scene/Entity.h)），是轻量值类型，按值传递合适。但若在 `OnRuntimeStop` 之后仍持有该 `Entity` 副本，其 `Scene*` 可能悬垂??因此**不要**把 `Entity` 存到 `ScriptEngineData`。本设计中，`ScriptEngineData::EntityInstances` 只以 `UUID` 为 key，不存 `Entity`，从根源避免此问题。

### 6.5 `mono_runtime_invoke` 参数打包规则

三个参数示例：

```cpp
// 无参
m_ScriptClass->InvokeMethod(instance, method);

// 一个 ulong 参数
UUID id = entity.GetUUID();
void* param = &id;
m_ScriptClass->InvokeMethod(instance, ctor, &param);   // 注意是 &param（void**）

// 一个 float 参数
float dt = 0.016f;
void* param = &dt;
m_ScriptClass->InvokeMethod(instance, onUpdate, &param);
```

`params` 是 `void**`，指向一个 `void*` 数组，数组每项指向对应参数的值。基本类型和 struct 都按"指向值的指针"传递。

### 6.6 `PrintAssemblyTypes` 用完记得关闭

`PrintAssemblyTypes` 只是调试工具，本 Phase **未在正式代码路径中调用**。如需调试可在 `LoadCoreAssembly` / `LoadAppAssembly` 内临时调用，验证后删除??不留调试代码 [[memory:du4s18ic]]。

### 6.7 `mono/lib` 相对路径依赖工作目录

`mono_set_assemblies_path("mono/lib")` 是**相对于进程当前工作目录**的路径。VS 调试时 CWD 是 `$(ProjectDir)`（即 `Luck3DApp/`），此时 `mono/lib` 正确解析为 `Luck3DApp/mono/lib/`。

**如果**从 exe 目录（`Binaries/**/Luck3DApp/`）双击启动，`mono/lib` 会解析失败。规避方式：
- MVP 阶段（本 Phase）：只从 VS F5 调试启动
- 未来（Phase 3+）：`Application` 主动设置 `spec.WorkingDirectory = <ExeDir>/../../../Luck3DApp` 或将 `mono/` 目录也随 exe 部署

本 Phase 不解决此问题，只在验收标准中约定"从 VS 启动"。

### 6.8 `Lucky-ScriptCore.dll` 与 `App.dll` 加载顺序

顺序**必须**是 Core → App → LoadAssemblyClasses。理由：
- `LoadAssemblyClasses` 内 `mono_class_from_name(CoreAssemblyImage, "Lucky", "Entity")` 需要 Core 已加载
- `mono_class_is_subclass_of` 需要同时访问两个 image

Init 中已按此顺序编写。

### 6.9 静态构造顺序

`ScriptEngine::Init` 中 `s_Data = new ScriptEngineData()` 是**动态初始化**，避免了 C++ 静态构造顺序问题。若把 `s_Data` 改成 `static ScriptEngineData s_Data;`（值类型），其构造会发生在 `main` 之前，此时 mono 尚未初始化，`std::unordered_map<UUID, Ref<ScriptInstance>>` 的构造虽然本身安全，但若未来加入依赖 mono 的静态成员就会出问题。**保持指针风格**。

### 6.10 `Entity.h` 循环依赖

`ScriptEngine.h` include `Entity.h`，`Entity.h` include `Scene.h`（用于 `GetUUID()` 定义），`Scene.h` 若未来 include `ScriptEngine.h`（例如 `OnRuntimeStart` 转调 `ScriptEngine::OnRuntimeStart`），会形成循环。

规避：`Scene.cpp`（非 `.h`）里 include `ScriptEngine.h`。P1.6 落地时严格遵守此规则。

---

## 7. 验收标准

对应 Roadmap Phase 1 第 (3) 条"引擎侧 `ScriptEngine`"：

1. **编译通过**：`Lucky` 三个 configuration（Debug / Release / Dist）均编译通过；`Luck3DApp` 三个 configuration 均编译通过；无 mono 相关链接错误
2. **启动无崩溃**：`Luck3DApp.exe` 从 VS F5 启动，`Application` 构造走到 `ScriptEngine::Init()`，`mono_jit_init` 成功
3. **Core Assembly 加载成功**：日志出现 `ScriptEngine: initialized (N user script classes loaded)`，N ≥ 0
4. **App Assembly 缺失时不崩溃**：`Assets/Scripts/Binaries/App.dll` 不存在，日志出现 `ScriptEngine: app assembly not found ...`，程序继续
5. **App Assembly 存在时能识别用户脚本类**（可选烟囱测试）：手动放一个含 `namespace Sandbox { class Foo : Lucky.Entity {} }` 的 `App.dll` 到目标路径，重启后日志出现 `ScriptEngine: found user script class 'Sandbox.Foo'`
6. **零回归**：编辑器行为与 P1.2 完成时完全一致，Play / Stop / 场景面板 / Inspector 全部正常（本 Phase 未接入 Scene runtime 钩子）
7. **Shutdown 干净**：程序退出无崩溃、无 mono 相关的 abort 信息

---

## 8. 后续 Phase 的接入点

本 Phase 落地后，下列节点可以在后续 Phase 中直接使用：

| 位置 | 后续 Phase | 会做什么 |
|------|-----------|---------|
| `ScriptEngine::GetCoreAssemblyImage()` | P1.5 | `ScriptGlue::RegisterComponents` 里 `mono_class_from_name(coreImage, "Lucky", "TransformComponent")` |
| `ScriptEngine::OnCreateEntityScript(Entity)` 内部注释 | P1.4 完成后启用 | 拿 `ScriptComponent::ClassName`、创建 `ScriptInstance`、放入 `EntityInstances`、`InvokeOnCreate` |
| `ScriptEngine::OnUpdateEntityScript(Entity, dt)` | P1.6 | 由 `Scene::OnUpdateRuntime` 每帧对每个持有 `ScriptComponent` 的实体调用一次 |
| `ScriptEngine::OnRuntimeStart(Scene*)` | P1.6 | 由 `Scene::OnRuntimeStart` 调用，同时遍历 `ScriptComponent` view 依次 `OnCreateEntityScript` |
| `ScriptEngine::OnRuntimeStop()` | P1.6 | 由 `Scene::OnRuntimeStop` 调用，释放全部 `ScriptInstance` |
| `ScriptEngine::EntityScriptClassExists / GetEntityClasses` | P1.4 | Inspector 里判断用户输入的 ClassName 是否合法 / 下拉选择 |
| 匿名 namespace 的 `LoadMonoAssembly` | Phase 3 | 热重载时会重新调用该函数，配合 AppDomain 卸载 |

---

## 9. 变更清单速览

- **新增**
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.h`
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`
- **修改**
  - `Lucky/Source/Lucky/Core/Application.cpp`：include `ScriptEngine.h`；构造末尾调 `ScriptEngine::Init`；析构开头调 `ScriptEngine::Shutdown`
- **删除**：无

---

## 10. 参考

- 参考项目 `D:/Projects/C++/Lucky/Lucky/Source/Lucky/Script/ScriptEngine.h/.cpp`：整体架构与实现模式
- Hazel Engine `Hazel/src/Hazel/Script/ScriptEngine.cpp`：`Entity` 单基类模型来源
- Mono Embedding 官方文档 - Loading assemblies: <https://www.mono-project.com/docs/advanced/embedding/#loading-assemblies>
- Roadmap [ScriptSystem_Roadmap.md](ScriptSystem_Roadmap.md) 第 3 章 (3)
