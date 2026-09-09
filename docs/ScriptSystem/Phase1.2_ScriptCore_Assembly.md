# Phase 1.2：Lucky-ScriptCore 托管层最小工程

## 1. 概述

P1.2 目标：搭建脚本系统的托管（C#）一侧最小工程 `Lucky-ScriptCore`，输出一份可被 mono 运行时加载的类库程序集 `Lucky-ScriptCore.dll`，提供 MVP 需要的最小基础类型（`Entity` / `Component` / `TransformComponent` / `Vector3` / `Debug` / `InternalCalls`）。

**本 Phase 只做托管侧代码 + csproj + premake 集成 + 输出到指定路径**这一件事，不涉及 C++ 引擎侧代码，不注册任何 Internal Call 的 C++ 实现，不接入 `Scene::OnRuntimeStart`。

### 1.1 关键决策

- **命名空间**：`Lucky`（与 Roadmap 中 `Lucky.Entity`、`Lucky.Debug` 保持一致；参考项目的 `LuckyEngine` 命名空间不采纳，见 §4.1）
- **基础模型**：**Hazel 风格**（`Entity` + `Component`，`Entity` 作为脚本基类），非参考项目的 `GameObject` + `MonoBehaviour` 风格（见 §4.2）
- **目标框架**：`.NET Framework 4.7.2`，与参考项目一致（见 §4.3）
- **输出路径**：`Luck3DApp/Resources/Scripts/Lucky-ScriptCore.dll`，与参考项目一致
- **premake 集成方式**：在 `Build.lua` 新增 `group "Scripts"` 分组下 include `Build-Lucky-ScriptCore.lua`
- **Vector3 字段大小写**：使用**小写** `x / y / z`，与参考项目、GLSL、`glm::vec3` 一致（见 §4.4）

### 1.2 前置依赖

- P1.1 已完成（`Vendor/mono/` 就位、`Lucky` 静态库能链上 `libmono-static-sgen.lib`）
- 本项目未强依赖 mono BCL 就位（本 Phase 只生成 dll，不加载 dll；BCL 的部署由 P1.3 落地后一起验证）
- 系统安装 .NET Framework 4.7.2 SDK 或以上（premake 生成 vs2022 工程时 MSBuild 需要 `Microsoft.CSharp.targets`；VS 2022 默认自带）

### 1.3 本 Phase **不做**的事

- 不写任何 C++ 代码（`ScriptEngine`、`ScriptGlue` 都在 P1.3 / P1.5）
- 不引入 `Input` 类和 `KeyCode` 枚举（P1.5 之后按需引入；本 Phase 保持最小集）
- 不做用户脚本 `Sandbox` / `App.dll`（那是 P1.7）
- 不做字段序列化 / 反射（Phase 2 议题）

---

## 2. 涉及的文件

### 需要新建

| 路径 | 说明 |
|------|------|
| `Lucky-ScriptCore/Build-Lucky-ScriptCore.lua` | premake C# 项目脚本 |
| `Lucky-ScriptCore/Lucky-ScriptCore.csproj` | 由 premake 生成（首次生成前不存在；作为附件文档提供参考形态） |
| `Lucky-ScriptCore/Source/Lucky/Entity.cs` | 脚本基类（对应 Roadmap `Lucky.Entity`） |
| `Lucky-ScriptCore/Source/Lucky/Component.cs` | 组件基类 + `TransformComponent` |
| `Lucky-ScriptCore/Source/Lucky/Debug.cs` | 日志接口（`Log / Warn / Error`） |
| `Lucky-ScriptCore/Source/Lucky/Vector3.cs` | 三维向量 POD（与 `glm::vec3` 内存布局对齐） |
| `Lucky-ScriptCore/Source/Lucky/InternalCalls.cs` | Internal Call 声明表（P1.5 会追加，本 Phase 只放 MVP 所需 4 项） |

### 需要修改

| 文件 | 说明 |
|------|------|
| `Build.lua` | 在合适分组下 `include "Lucky-ScriptCore/Build-Lucky-ScriptCore.lua"` |

### 无需修改

- `Lucky/**`、`Luck3DApp/**` 任何 C++ 代码
- `Dependencies.lua`（本 Phase 不引入新依赖）
- `Lucky/Build-Lucky.lua`、`Luck3DApp/Build-Luck3DApp.lua`（本 Phase 不改 C++ 项目构建）

---

## 3. 现状分析

### 3.1 本项目当前无任何 C# 项目

`D:/Projects/C++/Luck3D/` 下没有 `.csproj` 文件，`Build.lua` 也没有 C# 项目引入，`Luck3DApp/Resources/Scripts/` 目录不存在（[Resources](../../Luck3DApp/Resources) 当前只有 `Fonts/` 和 `Icons/`）。P1.2 是本项目引入的第一个托管工程。

### 3.2 参考项目 `D:/Projects/C++/Lucky` 的既有做法

| 维度 | 参考项目做法 | 本项目采用 |
|------|-------------|-----------|
| 命名空间 | `LuckyEngine` | **`Lucky`**（与 Roadmap 对齐） |
| 脚本基类 | `MonoBehaviour`（Unity 风格） | **`Entity`**（Hazel/Roadmap 风格） |
| 实体表达 | `GameObject`（内含 `ulong ID`） | **`Entity` 自身持有 `UUID ID`** |
| 组件基类 | `Component`（`public GameObject gameObject`） | **`Component`（`public Entity Entity`）** |
| Transform 类名 | `Transform` | **`TransformComponent`**（与 C++ 侧一致） |
| Vector3 字段 | `x / y / z`（小写） | **`x / y / z`（小写）** |
| 目标框架 | `.NET Framework 4.7.2` | **`.NET Framework 4.7.2`** |
| 输出路径 | `LuckyEditor/Resources/Scripts/` | **`Luck3DApp/Resources/Scripts/`** |
| 目录结构 | `Source/LuckyEngine/**` | **`Source/Lucky/**`** |
| premake kind | `SharedLib` + `language "C#"` | **同参考项目** |

理由参见 §4 的方案对比。

### 3.3 UUID 类型对齐

Luck3D 的 [UUID.h](../../Lucky/Source/Lucky/Core/UUID.h) 底层是 `uint64_t`，与 C# 的 `ulong`（`System.UInt64`）二进制布局完全一致。C# 侧统一用 `ulong` 传递 UUID，不做任何 marshal。

### 3.4 Log 系统对齐

Luck3D 的 [Log.h](../../Lucky/Source/Lucky/Core/Log.h) 提供两套宏：`LF_CORE_*`（引擎内核）与 `LF_*`（客户端）。`Debug.Log` 走的是**用户脚本**，本质上是"客户端"，因此 P1.5 的 `Debug_Log` C++ 实现最终会落到 `LF_TRACE / LF_WARN / LF_ERROR`；但本 Phase 只声明 C# 端接口，不涉及 C++ 实现。

---

## 4. 关键设计决策（方案对比）

### 4.1 【决策点 1】托管层命名空间

#### 方案 A：`Lucky`（**推荐 ★★★**）

```csharp
namespace Lucky
{
    public class Entity { ... }
    public static class Debug { ... }
}
```

- **优点**
  - 与 Roadmap 中的示例代码逐字符对齐（Roadmap 明确写的是 `class PlayerController : Lucky.Entity`、`Lucky.Debug.Log`）
  - 与 Hazel 生态一致，未来对照 Hazel-ScriptCore 学习/参考文档时无翻译成本
  - 名称简洁，用户 `using Lucky;` 后直接写 `Debug.Log(...)` / `class Foo : Entity` 即可
- **缺点**
  - C++ 侧命名空间也是 `Lucky`，跨语言看代码时同名空间可能产生"这段代码属于哪一侧"的短暂困惑??但两侧文件后缀不同（`.cs` vs `.h/.cpp`），实际不会混淆

#### 方案 B：`LuckyEngine`（参考项目做法）

- **优点**
  - 与 C++ 侧命名空间 `Lucky` 明确区分
  - 直接对齐参考项目，未来抄参考实现无需替换命名空间
- **缺点**
  - Roadmap 示例代码要重写（把 `Lucky.Entity` 换成 `LuckyEngine.Entity` 等）
  - 用户代码要写 `using LuckyEngine;` + `Lucky.Debug`（若与 C++ 混提代码时反而更绕）
  - 与 Hazel 参考实现不一致

#### 方案 C：`Luck3D`

- **优点**：与项目名 `Luck3D` 完全对齐
- **缺点**
  - 引擎的 C# 命名空间历史上一般跟随**引擎名**（`Lucky`）而非**编辑器名**（`Luck3D`）；C++ 侧命名空间也是 `Lucky`，不是 `Luck3D`
  - 项目名带数字，写起来不如 `Lucky` 顺手

**结论**：采用**方案 A**（`Lucky`）。

### 4.2 【决策点 2】脚本基类模型：`Entity` vs `GameObject/MonoBehaviour`

Roadmap 明确写的是"继承自 `Entity`"，参考项目走的是 Unity 风格的 `GameObject` + `MonoBehaviour` 分离。这是本 Phase 最需要早定的决策。

#### 方案 A：Hazel 风格 `Entity` 单基类（**推荐 ★★★**）

```csharp
public class Entity
{
    public readonly ulong ID;

    protected Entity() { ID = 0; }               // 无参构造仅供反序列化/占位
    internal Entity(ulong id) { ID = id; }       // 引擎侧 Activator.CreateInstance 后由 field 注入

    public bool HasComponent<T>() where T : Component, new() { ... }
    public T GetComponent<T>() where T : Component, new() { ... }
}

// 用户代码
public class PlayerController : Lucky.Entity
{
    void OnCreate() { Lucky.Debug.Log("Hello Luck3D"); }
    void OnUpdate(float dt) { ... }
}
```

- **优点**
  - 与 Roadmap MVP 示例代码一字不差
  - 与 Hazel-ScriptCore 一致，学习成本低，参考实现丰富
  - 一个基类搞定，用户不用理解"GameObject 持有组件，脚本本身也是组件"的间接层
  - `Entity` 直接持有 `ID`，`HasComponent`/`GetComponent` 直接 `InternalCalls.Entity_HasComponent(ID, ...)`
- **缺点**
  - 未来若想在同一 Entity 上挂多个脚本类，需要为每个脚本独立 `ScriptComponent` 实例（Hazel 的做法就是这样）；参考项目的 GameObject 模型天然支持"一 GameObject 多脚本"
  - 与 Unity 用户习惯略有差异（Unity 用户看到 `class : Entity` 会稍不适应）

#### 方案 B：Unity 风格 `GameObject` + `MonoBehaviour`（参考项目做法）

```csharp
public class GameObject { public readonly ulong ID; ... }
public abstract class Component { public GameObject gameObject { get; internal set; } }
public class MonoBehaviour : Component { ... }

// 用户代码
public class PlayerController : LuckyEngine.MonoBehaviour
{
    void OnUpdate(float dt) { transform.position += Vector3.right * dt; }
}
```

- **优点**
  - 与 Unity 用户习惯完全一致
  - 天然支持"一实体多脚本"
  - 参考项目已实现，抄起来最快
- **缺点**
  - Roadmap 示例代码要改动（`: Lucky.Entity` → `: Lucky.MonoBehaviour`）
  - 概念层次多一层（GameObject / Component / MonoBehaviour）
  - 对齐 C++ 侧 `entt::entity` 时的模型阻抗不匹配：C++ 里没有 `GameObject`，只有 `Entity`；用户读 C# 代码看到 `gameObject.ID`，读 C++ 代码看到 `Entity`，需要记忆双语命名
  - 与 Roadmap"最小闭环"精神相悖??MVP 阶段引入三层（GameObject / Component / MonoBehaviour）过重

#### 方案 C：`Entity` + `MonoBehaviour` 混合（Entity 是脚本基类，Component 是组件访问代理）

- **优点**：字段 `entity.HasComponent<T>()` 与 `Transform.position` 之类的 API 都能自然写出
- **缺点**：非标准，既不像 Hazel 也不像 Unity；文档负担和使用者理解成本都高

**结论**：采用**方案 A**（Hazel 风格 `Entity`）。理由：
1. **Roadmap 已经明确**（"提供基础类型：`Entity`（持有 UUID）、`Component`（基类，含 Entity 属性）"）；
2. **最小化 MVP**??本 Phase 只做最小闭环，不引入 `MonoBehaviour` 这一层，等 Phase 3 做协程/生命周期时若真的需要再补；
3. **对齐 C++ 侧**??C++ 里就叫 `Entity`（[Entity.h](../../Lucky/Source/Lucky/Scene/Entity.h)），双语一致，跨端阅读零成本。

### 4.3 【决策点 3】目标框架

#### 方案 A：`.NET Framework 4.7.2`（**推荐 ★★★**）

- **优点**
  - 与参考项目完全一致，csproj 直接对照写
  - mono 6.12 的 `mscorlib` 是 net472 兼容，加载零风险
  - Windows 环境下 VS 2022 默认自带工具链，无需额外安装
- **缺点**
  - 属于 Windows-only 的 legacy Framework，跨平台性弱??但本项目当前只做 Windows 编辑器，未来若做 Linux/macOS 时再迁移到 `netstandard2.0`

#### 方案 B：`netstandard2.0`

- **优点**：跨平台，未来接 CoreCLR 时更容易
- **缺点**
  - mono 6.12 加载 netstandard2.0 需要额外的 `netstandard.dll` 引用集，BCL 拷贝需要单独处理
  - Roadmap 明确"Phase 1 先走 Mono"，本 Phase 追求最小可行，无需为未来跨平台买单

#### 方案 C：`net6.0` / `net8.0`

- **缺点**：mono 6.12 无法直接加载 net6+ 程序集（.NET Core 元数据格式不同）；除非切到 CoreCLR，否则不适用

**结论**：采用**方案 A**（net472）。

### 4.4 【决策点 4】Vector3 字段命名：`X/Y/Z` vs `x/y/z`

#### 方案 A：小写 `x / y / z`（**推荐 ★★★**）

```csharp
public struct Vector3
{
    public float x, y, z;
}
```

- **优点**
  - 与 GLSL、`glm::vec3::x/y/z`、参考项目完全一致
  - Unity 用户习惯（`transform.position.x`）
  - 用户脚本代码 `p.x += dt` 顺手
- **缺点**
  - 违反 C# 官方公有字段 PascalCase 命名约定??但游戏引擎的 Vector 类型是行业惯例例外（Unity、Godot 都是小写）

#### 方案 B：PascalCase `X / Y / Z`

- **优点**：符合 C# 官方命名约定
- **缺点**：与项目内 C++ / GLSL 侧的所有 vector 表达式不一致，双语阅读成本高

#### 方案 C：属性 + 字段并存（`X { get; set; }` + `private float x;`）

- **优点**：既符合 C# 约定又跟 glm 内存布局一致
- **缺点**：C# 属性会破坏 POD 布局（`[StructLayout(Sequential)]` 不能有 backing field 自动生成的顺序保证），Internal Call 无法直接 `ref Vector3` 与 `glm::vec3*` 对拷；必须手写 property + private field，代码量翻倍且易错

**结论**：采用**方案 A**（小写 `x / y / z`）。

### 4.5 【决策点 5】Vector3 内存布局标注

C++ 侧 `glm::vec3` 是紧凑 12 字节（3 个 `float`），无 padding。C# 的 `struct` 默认 `[StructLayout(Sequential)]`（Sequential + `Pack = 8`，但由于三个字段都是 4 字节 float，实际排布无 gap，正好 12 字节）。

#### 方案 A：不显式加 `[StructLayout(Sequential)]`（**推荐 ★★★**）

- **优点**
  - C# 结构体默认就是 Sequential，无需显式声明；参考项目也没加
  - Internal Call `ref Vector3` 与 `glm::vec3*` 直接对拷不会出错
- **缺点**
  - 若未来某个新加的字段位置不合适（例如加了 `w`），可能触发 padding；但那属于 API 变更，非布局隐患

#### 方案 B：显式 `[StructLayout(LayoutKind.Sequential)]`

- **优点**：显式表达跨语言约束意图，防御性编码
- **缺点**：本 Phase 无必要；C# runtime 默认对 struct 就是 Sequential，加了纯粹是文档意义

**结论**：采用**方案 A**。若未来加更多向量字段或有 marshal 报错，再显式加 `[StructLayout]` 属性。

### 4.6 【决策点 6】输出路径

#### 方案 A：`Luck3DApp/Resources/Scripts/Lucky-ScriptCore.dll`（**推荐 ★★★**）

- **优点**
  - 与参考项目完全一致
  - Resources 目录已存在（[Luck3DApp/Resources](../../Luck3DApp/Resources)），新增子目录 `Scripts/` 自然
  - P1.3 加载路径固定，无需通过 `EditorPreferences` 等配置暴露
- **缺点**：无

#### 方案 B：输出到 `Binaries/**/Luck3DApp/Resources/Scripts/`

- **优点**：与 C++ 项目的输出目录风格一致
- **缺点**
  - `Lucky-ScriptCore.dll` 是**引擎自带**的 core assembly，不是用户产物；放 Binaries 会让人误以为是编译产物 (临时可清空)
  - 与参考项目不一致

#### 方案 C：`Luck3DApp/Assets/Scripts/Core/Lucky-ScriptCore.dll`

- **优点**：所有脚本相关文件集中在 `Assets/Scripts/`
- **缺点**
  - `Assets/` 是用户资产目录，把引擎自带的 core dll 混进去容易被误删/误提交
  - Roadmap 明确用户脚本 `App.dll` 才放 `Assets/Scripts/Binaries/`，`Lucky-ScriptCore.dll` 是引擎侧资源应放 `Resources/`

**结论**：采用**方案 A**。

### 4.7 【决策点 7】premake `Build.lua` 中的分组归属

#### 方案 A：新增 `group "Scripts"`（**推荐 ★★★**）

```lua
group "Core"
    include "Lucky/Build-Lucky.lua"
group ""

group "Scripts"
    include "Lucky-ScriptCore/Build-Lucky-ScriptCore.lua"
group ""

group "Tools"
    include "Luck3DApp/Build-Luck3DApp.lua"
group ""
```

- **优点**
  - VS Solution Explorer 里 `Scripts` 分组独立，未来 Phase 1.7 加 `Sandbox`（用户 App.dll）自然放在同一分组
  - 与"Core = C++ 引擎、Scripts = C# 托管、Tools = 编辑器"三分法清晰
- **缺点**：无

#### 方案 B：放在 `group "Core"` 下（参考项目做法）

```lua
group "Core"
    include "Lucky/Build-Lucky.lua"
    include "Lucky-ScriptCore/Build-Lucky-ScriptCore.lua"
group ""
```

- **优点**：与参考项目对齐
- **缺点**
  - "Core" 语义上是引擎核心，把 C# 托管层放进去混淆了 C++ / C# 边界
  - 未来加 Sandbox 时若放 Core 更奇怪，若放 Tools 又与 Lucky-ScriptCore 分离

#### 方案 C：不加 group，直接顶层 include

- **优点**：premake 语法最短
- **缺点**：VS 里所有项目平铺，与现有 Core/Tools/Dependencies 三分法不一致

**结论**：采用**方案 A**（新增 `group "Scripts"`）。

### 4.8 【决策点 8】Debug 类的实现方式

#### 方案 A：`static class Debug` + 直接调用 InternalCalls（**推荐 ★★★**）

```csharp
public static class Debug
{
    public static void Log(string message)   => InternalCalls.Debug_Log(message);
    public static void Warn(string message)  => InternalCalls.Debug_Warn(message);
    public static void Error(string message) => InternalCalls.Debug_Error(message);
}
```

- **优点**
  - 与 Unity 风格一致（`Debug.Log`）
  - 三个方法，覆盖 MVP 需求
- **缺点**：无

#### 方案 B：只提供 `Debug.Log`，Warn/Error 由后续 Phase 追加

- **优点**：更严格的最小 MVP
- **缺点**
  - `Log/Warn/Error` 是同一族方法，一次做完比分批做更省事
  - C++ 侧 `Debug_Log/Debug_Warn/Debug_Error` 的 Internal Call 一次注册三个也几乎没有额外成本

#### 方案 C：`Log(object)` 参数改为 `object` 而非 `string`

- **优点**：Unity 的 `Debug.Log(object)` 支持任意类型，用户可以直接 `Debug.Log(vec3)` 而无需手动 `ToString`
- **缺点**
  - Internal Call 传递 `object` 要 boxing + `MonoObject*`，实现比 `MonoString*` 复杂得多
  - MVP 阶段不必要；C# 端 `.ToString()` 让用户自己写即可

**结论**：采用**方案 A**（`static class Debug`，Log/Warn/Error 三方法，`string` 参数）。

### 4.9 【决策点 9】`Entity.HasComponent<T>` 的类型参数约束

#### 方案 A：`where T : Component, new()`（**推荐 ★★★**，与参考项目一致）

```csharp
public bool HasComponent<T>() where T : Component, new()
{
    return InternalCalls.Entity_HasComponent(ID, typeof(T));
}

public T GetComponent<T>() where T : Component, new()
{
    if (!HasComponent<T>()) return null;
    T component = new T { Entity = this };
    return component;
}
```

- **优点**
  - 编译期约束：只有 `Component` 派生类才能查询，避免误传 `Entity` 或 `int` 等类型
  - `new()` 约束让 `GetComponent` 能 `new T()` 实例化组件代理对象
  - `typeof(T)` 直接传给 Internal Call，C++ 侧用 `MonoReflectionType*` 匹配（P1.5 会实现）
- **缺点**：`new()` 约束要求组件必须有无参构造函数??本 Phase 的 `Component` / `TransformComponent` 都是无参构造，天然满足

#### 方案 B：无约束 `where T : Component`（去掉 `new()`）

- **优点**：更少约束
- **缺点**：`GetComponent<T>` 无法 `new T()` 实例化代理；需要另一套工厂机制，成本高

**结论**：采用**方案 A**。

### 4.10 【决策点 10】`InternalCalls` 类的可见性

#### 方案 A：`public static class InternalCalls`，方法为 `internal extern static`（**推荐 ★★★**，与参考项目一致）

- **优点**
  - `public class` 保证 C++ 侧 `mono_add_internal_call("Lucky.InternalCalls::Debug_Log", ...)` 能通过完整类型名找到
  - `internal` 方法限制用户脚本不能直接调 `InternalCalls.Debug_Log`（应通过 `Debug.Log`）
  - 参考项目 / Hazel 都是这个模式
- **缺点**：无

#### 方案 B：整个类 `internal`

- **缺点**：mono 通过反射查找类型时 `internal` 类型对 `mono_class_from_name` 是可见的（因为不看 CLR access modifier，只看元数据），但语义上不清晰

**结论**：采用**方案 A**。

---

## 5. 实现步骤

按下列顺序落地，每一步落地后 `premake5 vs2022` 都能生成，且工程能编译。

### Step 1：创建目录

在 `D:/Projects/C++/Luck3D/` 下新建：

```
Lucky-ScriptCore/
└─ Source/
    └─ Lucky/
```

Windows PowerShell：

```powershell
New-Item -ItemType Directory -Path "D:/Projects/C++/Luck3D/Lucky-ScriptCore/Source/Lucky" -Force
```

### Step 2：创建 `Build-Lucky-ScriptCore.lua`

**文件**：`Lucky-ScriptCore/Build-Lucky-ScriptCore.lua`

```lua
project "Lucky-ScriptCore"
    kind "SharedLib"
    language "C#"
    dotnetframework "4.7.2"

    targetdir ("%{wks.location}/Luck3DApp/Resources/Scripts")
    objdir ("%{wks.location}/Luck3DApp/Resources/Scripts/Intermediates")

    files
    {
        "Source/**.cs",
        "Properties/**.cs"
    }

    filter "configurations:Debug"
        optimize "Off"
        symbols "Default"

    filter "configurations:Release"
        optimize "On"
        symbols "Default"

    filter "configurations:Dist"
        optimize "Full"
        symbols "Off"
```

**说明**：

- `kind "SharedLib"` + `language "C#"` 是 premake 生成 C# 类库的标准写法
- `dotnetframework "4.7.2"` 对应 csproj 的 `<TargetFrameworkVersion>v4.7.2</TargetFrameworkVersion>`
- `targetdir` 与 `objdir` 都指向 `Luck3DApp/Resources/Scripts/`（Intermediates 是子目录）
- 参考项目相对路径 `../LuckyEditor/Resources/Scripts`，本项目改为 `%{wks.location}/Luck3DApp/Resources/Scripts`（更明确且不受 include 相对路径影响）
- `files` 通配符递归匹配 `Source/**.cs`，也保留了 `Properties/**.cs` 兼容位（若未来加 `AssemblyInfo.cs`）

### Step 3：修改 `Build.lua`

**文件**：`D:/Projects/C++/Luck3D/Build.lua`

在 `group "Core"` 之后、`group "Tools"` 之前新增：

```lua
group "Scripts"
    include "Lucky-ScriptCore/Build-Lucky-ScriptCore.lua"
group ""
```

改动后的完整 `Build.lua`：

```lua
-- premake5.lua
workspace "Luck3D"
    architecture "x64"
    configurations { "Debug", "Release", "Dist" }
    startproject "Luck3DApp"

    flags { "MultiProcessorCompile" }

outputdir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"

include "Dependencies.lua"

group "Dependencies"
    include "Lucky/Vendor/GLFW"
    include "Lucky/Vendor/GLAD"
    include "Lucky/Vendor/imgui"
    include "Lucky/Vendor/yaml-cpp"
group ""

group "Core"
    include "Lucky/Build-Lucky.lua"
group ""

group "Scripts"
    include "Lucky-ScriptCore/Build-Lucky-ScriptCore.lua"
group ""

group "Tools"
    include "Luck3DApp/Build-Luck3DApp.lua"
group ""
```

### Step 4：创建 `Vector3.cs`

**文件**：`Lucky-ScriptCore/Source/Lucky/Vector3.cs`

```csharp
namespace Lucky
{
    /// <summary>
    /// 三维向量（POD，与 C++ glm::vec3 内存布局对齐）
    /// </summary>
    public struct Vector3
    {
        public float x, y, z;

        public static Vector3 Zero => new Vector3(0.0f);
        public static Vector3 One => new Vector3(1.0f);
        public static Vector3 Right => new Vector3(1.0f, 0.0f, 0.0f);
        public static Vector3 Up => new Vector3(0.0f, 1.0f, 0.0f);
        public static Vector3 Forward => new Vector3(0.0f, 0.0f, -1.0f);

        public Vector3(float scalar)
        {
            x = scalar;
            y = scalar;
            z = scalar;
        }

        public Vector3(float x, float y, float z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public static Vector3 operator +(Vector3 a, Vector3 b)
        {
            return new Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
        }

        public static Vector3 operator -(Vector3 a, Vector3 b)
        {
            return new Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
        }

        public static Vector3 operator *(Vector3 vector, float scalar)
        {
            return new Vector3(vector.x * scalar, vector.y * scalar, vector.z * scalar);
        }

        public static Vector3 operator *(float scalar, Vector3 vector)
        {
            return new Vector3(vector.x * scalar, vector.y * scalar, vector.z * scalar);
        }
    }
}
```

**说明**：

- 在参考项目基础上追加了 `One / Right / Up / Forward` 四个便捷静态属性，方便 Roadmap 示例 `Vector3.Right * dt` 直接可用
- `Forward` 采用 `(0, 0, -1)`（OpenGL 右手系约定），与 Luck3D 的 `SceneCamera` / `EditorCamera` 前向一致
- 不加 `[StructLayout(Sequential)]`（决策 4.5）

### Step 5：创建 `Component.cs`

**文件**：`Lucky-ScriptCore/Source/Lucky/Component.cs`

```csharp
namespace Lucky
{
    /// <summary>
    /// 组件基类：所有 C# 侧组件代理的公共父类
    /// 引擎侧通过 GetComponent<T>() 实例化时注入 Entity
    /// </summary>
    public abstract class Component
    {
        public Entity Entity { get; internal set; }
    }

    /// <summary>
    /// Transform 组件代理：读写实体的世界坐标位置
    /// </summary>
    public class TransformComponent : Component
    {
        public Vector3 Position
        {
            get
            {
                InternalCalls.TransformComponent_GetPosition(Entity.ID, out Vector3 result);
                return result;
            }
            set
            {
                InternalCalls.TransformComponent_SetPosition(Entity.ID, ref value);
            }
        }
    }
}
```

**说明**：

- `Entity` 属性采用 `{ get; internal set; }`，只有 `Lucky` 程序集内代码能赋值（`Entity.GetComponent<T>` 会赋值）
- `TransformComponent` 只包含 `Position`（MVP 需要），Rotation / Scale 由 Phase 2+ 追加
- 类名 `TransformComponent`（不是参考项目的 `Transform`）：与 C++ 侧 `TransformComponent` 一致，方便双语阅读；Roadmap 明确写的是 `GetComponent<TransformComponent>()`

### Step 6：创建 `Entity.cs`

**文件**：`Lucky-ScriptCore/Source/Lucky/Entity.cs`

```csharp
using System;

namespace Lucky
{
    /// <summary>
    /// 脚本基类：所有用户脚本类型的父类
    /// 每个挂载了 ScriptComponent 的实体在运行时对应一个 Entity 派生类实例
    /// </summary>
    public class Entity
    {
        public readonly ulong ID;

        protected Entity()
        {
            ID = 0;
        }

        internal Entity(ulong id)
        {
            ID = id;
        }

        public bool HasComponent<T>() where T : Component, new()
        {
            Type componentType = typeof(T);
            return InternalCalls.Entity_HasComponent(ID, componentType);
        }

        public T GetComponent<T>() where T : Component, new()
        {
            if (!HasComponent<T>())
            {
                return null;
            }

            T component = new T { Entity = this };
            return component;
        }
    }
}
```

**说明**：

- `readonly ulong ID` 直接暴露，符合参考项目 `GameObject` 的做法
- `protected Entity()`：让用户派生类 `class PlayerController : Entity { }` 编译通过（用户无参构造隐式调用父类无参构造）；`ID` 会被 0 初始化，随后由 P1.3 引擎侧通过反射写入正确的 UUID
- `internal Entity(ulong id)`：内部构造，供 `Entity.GetComponent<T>()` 或 P1.5 未来的 `Entity Instantiate(...)` 之类工厂使用；本 Phase 只做占位
- `where T : Component, new()`：见决策 4.9

### Step 7：创建 `Debug.cs`

**文件**：`Lucky-ScriptCore/Source/Lucky/Debug.cs`

```csharp
namespace Lucky
{
    /// <summary>
    /// 日志：转发到引擎侧 spdlog
    /// </summary>
    public static class Debug
    {
        public static void Log(string message)
        {
            InternalCalls.Debug_Log(message);
        }

        public static void Warn(string message)
        {
            InternalCalls.Debug_Warn(message);
        }

        public static void Error(string message)
        {
            InternalCalls.Debug_Error(message);
        }
    }
}
```

### Step 8：创建 `InternalCalls.cs`

**文件**：`Lucky-ScriptCore/Source/Lucky/InternalCalls.cs`

```csharp
using System;
using System.Runtime.CompilerServices;

namespace Lucky
{
    /// <summary>
    /// Internal Call 声明表：C# 侧声明由 C++ 侧 ScriptGlue 注册的 native 方法
    /// 命名规范：目标类_操作，例如 TransformComponent_GetPosition
    /// </summary>
    public static class InternalCalls
    {
        // ---- Debug ----
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Debug_Log(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Debug_Warn(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Debug_Error(string message);

        // ---- Entity ----
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Entity_HasComponent(ulong entityID, Type componentType);

        // ---- TransformComponent ----
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void TransformComponent_GetPosition(ulong entityID, out Vector3 outPosition);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void TransformComponent_SetPosition(ulong entityID, ref Vector3 inPosition);
    }
}
```

**说明**：

- 6 个 Internal Call，对齐 Roadmap 的最小集（Log 三方法 + HasComponent + GetPosition + SetPosition）
- `MethodImpl(MethodImplOptions.InternalCall)` 告诉 mono 该方法由 native 侧提供实现（P1.5 落地）
- `extern static` 声明无方法体，由 C++ 侧 `mono_add_internal_call("Lucky.InternalCalls::Debug_Log", ...)` 绑定
- 参数命名 `entityID` / `outPosition` / `inPosition` 明确 in/out 方向

### Step 9：生成 VS 工程并编译

- 执行 `premake5 --file=Build.lua vs2022`（或运行项目里已有的 `Setup.bat`）
- 打开 `Luck3D.sln`，Solution Explorer 应显示 `Scripts` 分组下的 `Lucky-ScriptCore` 项目
- 右键 `Lucky-ScriptCore → 生成`，输出应落到 `Luck3DApp/Resources/Scripts/Lucky-ScriptCore.dll`
- 验证 `Luck3DApp/Resources/Scripts/` 目录存在且包含：
  - `Lucky-ScriptCore.dll`（约 10~20 KB）
  - `Lucky-ScriptCore.pdb`（Debug 配置下）

---

## 6. 坑点提醒

### 6.1 premake C# 项目要求 VS 安装 "?? .NET 桌面开发" 工作负载

`language "C#" + dotnetframework "4.7.2"` 生成的 csproj 依赖 `Microsoft.CSharp.targets`，若 VS 2022 安装时未勾选 ".NET 桌面开发"（含 .NET Framework 4.7.2 SDK），MSBuild 会报 `error MSB4019: 未找到导入的项目 "...Microsoft.CSharp.targets"`。

**规避**：VS Installer → 修改 → 勾选 ".NET 桌面开发" + 单独组件里勾选 ".NET Framework 4.7.2 SDK" 与 ".NET Framework 4.7.2 目标包"。

### 6.2 `startproject` 保持 `Luck3DApp`

`Lucky-ScriptCore` 是类库（`SharedLib`），不可执行。`workspace` 内 `startproject "Luck3DApp"` 保持不变；若误改为 `Lucky-ScriptCore`，F5 会报错。

### 6.3 `Luck3DApp/Resources/Scripts/` 目录首次生成前不存在

`targetdir` 指向的目录若不存在，MSBuild 会自动创建。但**注意**：`Intermediates/` 子目录里会积累 `.pdb` / `.cache` 等编译中间文件；建议在 `.gitignore` 中追加：

```
/Luck3DApp/Resources/Scripts/Intermediates/
```

`Lucky-ScriptCore.dll` 本身要不要提交，取决于团队策略：作为**引擎自带资源**，建议提交（避免每次拉代码都要先编 C# 才能跑起来）；配合 `.gitattributes` 使用 Git LFS 亦可。

### 6.4 `public` vs `internal` 类型对 Mono 的可见性

`InternalCalls` 类和 `Entity/Component` 基类**必须 `public`**??C++ 侧 mono API `mono_class_from_name` 查找类型时依赖元数据可见性，`internal` 类型（用 `NestedFamily` 等修饰）虽然也能查到，但 `mono_add_internal_call` 绑定 `internal` 方法会更容易踩到 `MethodAccessException`（当有 `[SecurityCritical]` 时）。所有会被 C++ 侧引用的类保持 `public`，方法可 `internal`。

### 6.5 `InternalCall` 方法签名与 C++ 侧必须严格对应

P1.5 落地时 C++ 侧函数签名必须与 C# 声明字对字节对齐：

| C# 声明 | C++ 签名 |
|---------|---------|
| `void Debug_Log(string message)` | `void Debug_Log(MonoString* message)` |
| `bool Entity_HasComponent(ulong id, Type componentType)` | `bool Entity_HasComponent(UUID id, MonoReflectionType* componentType)` |
| `void TransformComponent_GetPosition(ulong id, out Vector3 outPos)` | `void TransformComponent_GetPosition(UUID id, glm::vec3* outPos)` |
| `void TransformComponent_SetPosition(ulong id, ref Vector3 inPos)` | `void TransformComponent_SetPosition(UUID id, glm::vec3* inPos)` |

- `out T` / `ref T` 在 mono marshal 层等价于 `T*`（都是"指针"）
- `Vector3` 与 `glm::vec3` 内存布局 12 字节一致（无 padding）
- `ulong` 与 `uint64_t`（Luck3D `UUID` 底层类型）二进制一致

本 Phase 只写 C# 声明，C++ 实现在 P1.5，此处提前约定以便 P1.5 无需再决策。

### 6.6 项目文件夹结构（VS Solution Explorer 视图）

premake 生成后 VS Solution Explorer 会展示：

```
Solution 'Luck3D'
├─ Core
│   └─ Lucky (C++ Static Library)
├─ Dependencies
│   └─ GLFW / GLAD / ImGui / yaml-cpp
├─ Scripts
│   └─ Lucky-ScriptCore (C# Class Library)
└─ Tools
    └─ Luck3DApp (C++ Console App)
```

若发现 `Lucky-ScriptCore` 未出现在 `Scripts` 分组下，检查 `Build.lua` 的 `group "Scripts"` 与 `group ""`（空字符串关闭 group）是否配对。

### 6.7 参考项目 `GameObject.gameObject` 属性不迁移

参考项目 `GameObject` 有一个 `public GameObject gameObject => this;` 属性，模拟 Unity `MonoBehaviour.gameObject`。本 Phase 采用 Hazel 风格，脚本本身就是 `Entity` 派生，无 `gameObject` 概念，**不迁移该属性**。若未来 Phase 3 引入 `MonoBehaviour` 才再考虑。

### 6.8 `Debug` 类与 C# BCL 的 `System.Diagnostics.Debug` 命名冲突

C# BCL 有 `System.Diagnostics.Debug.Assert / Debug.WriteLine`。用户脚本若 `using System.Diagnostics;` + `using Lucky;` 后直接写 `Debug.Log(...)` 会出现二义性。

**规避**：
- 引擎自带 `Lucky.Debug` 优先级由用户代码 `using` 顺序决定；若冲突，用户应写全限定名 `Lucky.Debug.Log(...)`
- Roadmap 示例代码写的就是 `Lucky.Debug.Log("Hello Luck3D")`，全限定名天然规避
- 未来 Phase 3 可以考虑改名 `Log`（`public static class Log { }`）避免冲突，但当前 MVP 阶段保持 `Debug`

---

## 7. 验收标准

对应 Roadmap Phase 1 第 (2) 条"托管层最小工程 Lucky-ScriptCore"：

1. **premake 生成通过**：`premake5 --file=Build.lua vs2022` 无报错，`Luck3D.sln` 内出现 `Lucky-ScriptCore` 项目，归属 `Scripts` 分组
2. **C# 编译通过**：Debug / Release / Dist 三个配置下 `Lucky-ScriptCore` 均可编译，产物 `Lucky-ScriptCore.dll` 落到 `Luck3DApp/Resources/Scripts/`
3. **文件清单完整**：`Luck3DApp/Resources/Scripts/` 下有：
   - `Lucky-ScriptCore.dll`
   - `Lucky-ScriptCore.pdb`（Debug 配置下）
4. **元数据验证**（用 `ildasm` 或 `dnSpy` 打开 dll，或使用 `dotnet-ildasm`）：
   - 命名空间：`Lucky`
   - 公共类型：`Entity`、`Component`、`TransformComponent`、`Vector3`、`Debug`、`InternalCalls`
   - `Entity` 的公共成员：`ID`、`HasComponent<T>()`、`GetComponent<T>()`
   - `TransformComponent.Position` 属性存在 getter + setter
   - `InternalCalls` 的 6 个 `extern static` 方法签名与 §5.8 一致
5. **零 C++ 回归**：`Lucky` / `Luck3DApp` 三配置编译通过，编辑器启动行为与 P1.1 完成时一致（本 Phase 完全不动 C++ 代码）
6. **烟囱代码验证（可选）**：用 VS 打开一个空 C# 控制台工程，`Add Reference → Browse → Lucky-ScriptCore.dll`，然后写：
   ```csharp
   using Lucky;
   class Program
   {
       static void Main()
       {
           System.Console.WriteLine(Vector3.Right.x);  // 输出 1
       }
   }
   ```
   （只是验证 dll 结构可被 CLR 加载和使用；实际运行需要能定位到 `InternalCalls` 的 native 实现，本 Phase 无此能力，故只测不 P/Invoke 的路径）

---

## 8. 后续 Phase 的接入点

本 Phase 落地后，下列节点可以在后续 Phase 中直接使用：

| 位置 | 后续 Phase | 会做什么 |
|------|-----------|---------|
| `Luck3DApp/Resources/Scripts/Lucky-ScriptCore.dll` | P1.3 | `ScriptEngine::LoadCoreAssembly` 从此路径加载 |
| `Lucky.Entity` / `Lucky.Component` 类型 | P1.3 | 反射查找用户 App.dll 中继承自 `Entity` 的类；`GetComponent<T>` 反射派发 |
| `Lucky.InternalCalls` 类 | P1.5 | `mono_add_internal_call("Lucky.InternalCalls::Debug_Log", &ScriptGlue::Debug_Log)` 逐条注册 |
| `Vector3` 12 字节布局 | P1.5 | `TransformComponent_GetPosition/SetPosition` 直接 `glm::vec3*` 与 `Vector3*` 对拷 |
| `Lucky.Debug` 三方法 | P1.5 | `Debug_Log/Warn/Error` 内部转发到 `LF_TRACE/LF_WARN/LF_ERROR` |
| `Source/Lucky/InternalCalls.cs` | P1.5 后续 / Phase 2 | 新增 Input、Time、其他 Component 的 Internal Call 声明 |
| `Source/Lucky/KeyCode.cs`（未创建） | Phase 3 | 引入 Input 事件化时按 [KeyCodes.h](../../Lucky/Source/Lucky/Core/Input/KeyCodes.h) 逐值同步 |

---

## 9. 变更清单速览

- **新增**
  - `Lucky-ScriptCore/Build-Lucky-ScriptCore.lua`
  - `Lucky-ScriptCore/Source/Lucky/Entity.cs`
  - `Lucky-ScriptCore/Source/Lucky/Component.cs`（含 `Component` + `TransformComponent`）
  - `Lucky-ScriptCore/Source/Lucky/Debug.cs`
  - `Lucky-ScriptCore/Source/Lucky/Vector3.cs`
  - `Lucky-ScriptCore/Source/Lucky/InternalCalls.cs`
  - `Lucky-ScriptCore/Lucky-ScriptCore.csproj`（由 premake 生成，无需手写）
- **修改**
  - `Build.lua`：新增 `group "Scripts"` 分组并 include `Lucky-ScriptCore/Build-Lucky-ScriptCore.lua`
- **删除**：无

---

## 10. 参考

- 参考项目 `D:/Projects/C++/Lucky/Lucky-ScriptCore/`：目录结构、csproj 形态、premake C# 写法
- Hazel Engine `Hazel-ScriptCore`：`Entity` / `Component` 基类模型
- Roadmap [ScriptSystem_Roadmap.md](ScriptSystem_Roadmap.md) 第 3 章 (2)
- Mono Embedding 官方文档 - Internal Call: <https://www.mono-project.com/docs/advanced/embedding/#accessing-c-code-from-c>
- premake 文档 - C# projects: <https://premake.github.io/docs/language/>
