# Phase 2.2：`.cs` → 脚本类解析

## 1. 概述

把 `Script` 资产（只知道"文件叫什么"）解析成 `ScriptClass`（程序集里那个**真实的、能 `Instantiate` 的类**）。

这是从"资产层"跨到"脚本运行时层"的那座桥：P2.1 产出的 `Script::GetClassName()` 只是文件名词干，**不保证程序集里真的有这个类**；本 Phase 负责按约定把它查出来，并且查不到时给出**能照着做**的错误信息。

### 1.1 关键约束

- **解析依赖已加载的程序集**：`ScriptEngine::LoadAssemblyClasses()` 已经把用户程序集里所有 `Entity` 派生类收进了 `EntityClasses`（key = 全名 `"Namespace.ClassName"`）。所以本 Phase 的接口**只能在 `ScriptEngine::Init()` 之后调用**，不能在 `Init` 之前。
- **匹配规则**：`ScriptClass::GetName()`（简单类名）**等于**文件名词干。这是对齐 Unity 的"文件名必须与类名一致"约定 —— 你的 `PlayerController.cs` → `Sandbox.PlayerController` 正好符合。
- **三种结果必须区分对待**：命中 1 个 → 成功；命中 0 个 → 大概率是**没参与编译**或改了名；命中多个 → 命名空间冲突。三者的日志内容不一样。
- **失败返回 `nullptr`，不抛异常、不兜底**。调用方（P2.3 的 Inspector / 运行态）需要靠 `nullptr` 判断并降级。
- **失败日志用 `LF_CORE_ERROR` 而不是 `WARN`**：这是个"用户以为能用、实际不能用"的状态，WARN 会被淹没。
- 代码风格遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：控制语句强制花括号、`for` 元素用 `auto`（迭代器/范围 for）、智能指针引用**显式声明类型**（§13.9）。

### 1.2 前置条件

| 依赖 | 说明 |
|------|------|
| P2.1 | `AssetType::Script` + `class Script` 已就位 |
| `ScriptEngine::EntityClasses` | 已就绪，`LoadAssemblyClasses()` 已填好 |
| `ScriptEngine::GetEntityClasses()` | 已就绪，返回 `const std::unordered_map<std::string, Ref<ScriptClass>>&` |
| `ScriptClass::GetName()` / `GetNamespace()` | 已就绪 |
| 调用时机 | 一切调用都发生在 `Application::Init` 里的 `ScriptEngine::Init()` **之后**（`Project::Load` → `AssetManager::Init` → `Renderer::Init` → `ScriptEngine::Init`，顺序已满足） |

### 1.3 本 Phase **不做**的事

- 不改 `ScriptComponent`（那是 P2.3）
- 不做字段元信息反射（那是 P2.4）
- 不触发脚本编译、不做热重载（那是 P3）
- 不做任何 UI 显示
- 不做"解析结果缓存"（理由见决策点 4.4）

---

## 2. 涉及的文件

### 2.1 新建

无。

### 2.2 修改

| 文件 | 改动 |
|------|------|
| `Lucky/Source/Lucky/Scripting/ScriptEngine.h` | `ScriptEngine` 加一个静态方法声明 |
| `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp` | 加对应实现 |

### 2.3 不修改

- `Script/Components/ScriptComponent.h`：本 Phase 完全不感知
- `Asset/*`：`Script` 资产只提供名字，解析逻辑在 `Scripting` 层（依赖方向正确：`Scripting → Asset`，不反向）
- `Serialization/*`、`Editor/*`、托管 C# 代码：都不涉及
- premake：无新增文件，不用重新生成

---

## 3. 现状回顾

### 3.1 `EntityClasses` 是怎么填出来的

`Scripting/ScriptEngine.cpp` 的 `LoadAssemblyClasses()`：

```cpp
    void ScriptEngine::LoadAssemblyClasses()
    {
        s_Data->EntityClasses.clear();

        if (!s_Data->AppAssemblyImage)
        {
            return;     // 用户程序集没加载成功：EntityClasses 保持为空
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
                continue;   // 跳过 Lucky.Entity 基类自身
            }

            if (!mono_class_is_subclass_of(monoClass, entityBaseClass, false))
            {
                continue;   // 跳过所有非 Entity 派生类
            }

            Ref<ScriptClass> scriptClass = CreateRef<ScriptClass>(nameSpace, className, false);
            s_Data->EntityClasses[fullName] = scriptClass;

            LF_CORE_TRACE("ScriptEngine: found user script class '{}'", fullName);
        }
    }
```

三个关键事实：

1. **key 是"全名"**（有命名空间则 `Namespace.ClassName`，没有则 `ClassName`）
2. **value 是 `Ref<ScriptClass>`**，由 `ScriptClass(classNamespace, className, isCore=false)` 构造
3. **只有 `Entity` 的派生类会进来**，基类和无关类都被跳过 —— 所以本 Phase 匹配到的结果**必定是合法的脚本类**，不需要再自己验一遍继承关系

### 3.2 `ScriptClass` 暴露什么

`Scripting/ScriptEngine.h`：

```cpp
    class ScriptClass
    {
    public:
        ScriptClass() = default;
        ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore = false);

        MonoObject* Instantiate();
        MonoMethod* GetMethod(const std::string& name, int parameterCount);
        MonoObject* InvokeMethod(MonoObject* instance, MonoMethod* method, void** params = nullptr);

        const std::string& GetNamespace() const { return m_ClassNamespace; }
        const std::string& GetName() const { return m_ClassName; }
    private:
        std::string m_ClassNamespace;
        std::string m_ClassName;
        MonoClass* m_MonoClass = nullptr;
    };
```

**`GetName()` 返回的就是简单类名**（`"PlayerController"`，不含命名空间），正是本 Phase 要匹配的东西。

### 3.3 现有的"按全名查"接口

```cpp
    bool ScriptEngine::EntityScriptClassExists(const std::string& fullClassName)
    {
        return s_Data->EntityClasses.find(fullClassName) != s_Data->EntityClasses.end();
    }
```

这是**按全名精确查**（key 直接命中 map）。本 Phase 要新增的是**按简单名模糊查**（需要遍历 value 比较 `GetName()`），两者用途不同，都要保留：

- `EntityScriptClassExists(fullName)`：运行态用，`ScriptComponent` 存全名时校验
- `ResolveScriptClass(className)`：本 Phase 新增，资产态用，从文件名词干找类

### 3.4 `s_Data->EntityClasses` 的生命周期

- 在 `ScriptEngine::Init()` 里由 `LoadAssemblyClasses()` 填充
- **只在 `Init()` 里调一次**（`Application::Init` 时）
- `ScriptEngine::Shutdown()` 释放 `s_Data`
- 也就是说：**运行期这张表是只读的**，本 Phase 的查询不需要加锁、不需要担心并发

---

## 4. 关键设计决策（多方案对比）

### 4.1 决策点 1：匹配规则

#### 方案 A：`ScriptClass::GetName() == 文件名词干`（**推荐 ✅**）

- **优点**：
  1. **对齐 Unity 的既有约定**（MonoBehaviour 的文件名必须与类名一致），用户从 Unity 过来零学习成本
  2. 项目的 `PlayerController.cs` → `Sandbox.PlayerController` 已经符合，无需改动示例
  3. 规则简单到可以一句话说清："你的类叫什么，文件就叫什么"
- **缺点**：命名空间不参与匹配，理论上可能出现两个不同命名空间里的同名类 → 需要用决策点 4.3 的"命中多个"分支处理

#### 方案 B：`全名 == 文件名（含点）`，即要求文件叫 `Sandbox.PlayerController.cs`

- **优点**：无歧义
- **缺点**：文件名带点，**Windows 资源管理器和多数工具处理起来别扭**；且违反 Unity 惯例。**否决。**

#### 方案 C：大小写不敏感匹配

- **优点**：容忍 `playercontroller.cs` 这种写法
- **缺点**：
  1. C# **区分大小写**，`Foo` 和 `foo` 是两个不同的类，模糊匹配会挑错类
  2. 一旦两个类只差大小写，匹配结果不确定
- **否决。**

#### 方案 D：不按名字，直接找程序集里"唯一的那一个 Entity 派生类"

- **优点**：完全不怕改名
- **缺点**：项目里只要有两个脚本就立刻歧义；而且"文件名 = 类名"本身就是用户必须理解的概念（Unity 用户已经理解），**回避规则反而增加困惑**。**否决。**

**结论：方案 A。**

---

### 4.2 决策点 2：返回值与失败语义

#### 方案 A：返回 `Ref<ScriptClass>`，失败返回 `nullptr` 并打日志（**推荐 ✅**）

```cpp
static Ref<ScriptClass> ResolveScriptClass(const std::string& className);
```

- **优点**：
  1. 调用方一行 `if (!scriptClass)` 就能判断并降级，语义直白
  2. 返回 `Ref<ScriptClass>` 与 `EntityClasses` 的 value 类型一致，拿到就能直接 `Instantiate()`
  3. 与项目现有风格一致（`AssetManager::GetAsset<T>` 失败也返回 `nullptr`）
- **缺点**：无

#### 方案 B：`bool ResolveScriptClass(const std::string& className, Ref<ScriptClass>& outClass)`

- **优点**：显式表达"这是查询，不是获取"
- **缺点**：多一个出参，调用点啰嗦；且返回 `bool` 与"拿到对象"是两个信息，容易忘检查出参。项目里 `AssetManager::GetAsset<T>` 用的是返回指针的风格，**保持一致更重要**。

#### 方案 C：抛异常

- **优点**：不可能被忽略
- **缺点**：项目里没有任何异常约定（错误处理靠 `LF_CORE_ASSERT` + 日志 + fallback，见规范 §13.7）；解析失败是**常见的用户操作错误**（忘记编译），不是程序缺陷，不该用异常传达。**否决。**

#### 方案 D：失败时返回任意一个类做兜底

- **优点**：界面"看起来能用"
- **缺点**：把"你拖错了/没编译"变成"挂了别人的脚本"，**最难查的一类 bug**。**否决。**

**结论：方案 A。**

---

### 4.3 决策点 3：命中多个同名类怎么办

#### 方案 A：视为失败，日志里**列出全部候选全名**（**推荐 ✅**）

```
ScriptEngine::ResolveScriptClass - 'PlayerController' 匹配到多个类：Sandbox.PlayerController, Tools.PlayerController。请在其中一个上加命名空间前缀或重命名文件以区分。
```

- **优点**：
  1. 用户一眼知道问题在哪、怎么改
  2. 不引入"不确定选哪个"的静默行为
- **缺点**：需要拼字符串列表（几行代码），但对"最难查的错误"来说非常划算

#### 方案 B：取第一个命中

- **优点**：零额外代码
- **缺点**：`unordered_map` 的遍历顺序**不保证**，同一份代码两次运行可能选中不同的类 —— 这是"偶发、不可复现"的 bug 温床。**否决。**

#### 方案 C：直接报错，不建议怎么办

- **优点**：实现最简
- **缺点**：用户看到一个类名列表却不知道该做什么。**A 只是多写一句提示，没理由不做。**

**结论：方案 A。**

---

### 4.4 决策点 4：要不要缓存解析结果

#### 方案 A：不缓存，每次遍历 `EntityClasses` 查（**推荐 ✅**）

- **优点**：
  1. `EntityClasses` 通常只有几个到几十个类，遍历一次的代价可以忽略；而这个函数**只会在"拖入脚本时"和"进入 Play 时"被调用**，不是每帧
  2. **不存在缓存失效问题**。缓存一旦引入，就要处理"脚本重编译后类没了/改名了"的失效逻辑，而 P3 的热重载恰恰会频繁触发这种情况
  3. 代码量最小
- **缺点**：如果将来脚本数量涨到几百且每帧都解析，才需要考虑优化 —— 那是**有了实测数据再优化**的场景，不是现在

#### 方案 B：在 `Script` 资产上缓存（加一个 `Ref<ScriptClass> m_ResolvedClass`）

- **优点**：一次解析多次用
- **缺点**：
  1. `Script` 是**资产层**的类，把 `ScriptClass`（脚本运行时层）塞进去会造成**层级倒挂**（Asset 依赖 Scripting）
  2. 资产的生命周期比程序集长：脚本重编译后 `Script` 对象还在，缓存的 `ScriptClass` 已经指向旧 `MonoClass*` —— 典型的悬垂缓存
- **否决。**

#### 方案 C：在 `ScriptEngine` 里加 `unordered_map<std::string, Ref<ScriptClass>> s_ClassNameCache`

- **优点**：解析加速，且留在 `Scripting` 层内，没有层级问题
- **缺点**：**同样要处理重编译失效**（`LoadAssemblyClasses()` 里得记得 `clear()`）。收益（省一次几十项的遍历）远小于成本（多一处必须同步的状态）。**本 Phase 不做**；将来若真成为瓶颈，在 `LoadAssemblyClasses()` 里 `clear()` 即可，是个局部改动。

**结论：方案 A。**

---

### 4.5 决策点 5：日志的级别与措辞

#### 方案 A：`LF_CORE_ERROR` + 指明"大概率是没编译"（**推荐 ✅**）

- **级别理由**：`WARN` 在这个项目里被大量使用（各种降级路径），一条"脚本用不了"的用户错误混在里面很容易被忽略
- **措辞理由**：用户看到"找不到类"的第一反应是"我名字写错了"，但**真正最常见的原因是脚本没参与编译**（改了 `.cs` 没重新编译，或新建的 `.cs` 没进 `Assembly-CSharp`）。日志必须把这条放在最前面：

```
ScriptEngine::ResolveScriptClass - 未找到脚本类 'PlayerController'（来自文件 PlayerController.cs）。请确认该脚本已参与编译，且类名与文件名一致。
```

- **优点**：一句话同时覆盖"名字不对"和"没编译"两种可能，用户能自己往下走
- **缺点**：无

#### 方案 B：`LF_CORE_WARN` + 只报"未找到 X"

- **优点**：更短的日志
- **缺点**：用户拿到"未找到 PlayerController"会反复检查文件名，而真正的原因（没编译）猜不到。**否决。**

**结论：方案 A。**

---

## 5. 实现步骤（按依赖顺序）

本 Phase 只改两个文件，每步完成后都应能编译通过。

### Step 1：`ScriptEngine.h` 加声明

**文件**：`Lucky/Source/Lucky/Scripting/ScriptEngine.h`

在 `EntityScriptClassExists` 声明**之后**追加（这两个接口职责相邻，放一起）：

```cpp
        /// <summary>
        /// 用户程序集中是否存在指定全名的 Entity 派生类
        /// </summary>
        /// <param name="fullClassName">"Namespace.ClassName" 形式的类全名</param>
        static bool EntityScriptClassExists(const std::string& fullClassName);

        /// <summary>
        /// 按简单类名在用户程序集里解析脚本类
        /// 匹配规则：ScriptClass::GetName() 等于 className（对齐"文件名与类名一致"的约定）
        /// 由脚本资产（.cs）解析到可实例化的类；找到多个同名类时视为失败
        /// </summary>
        /// <param name="className">简单类名（通常是 .cs 的文件名词干，如 "PlayerController"）</param>
        /// <returns>脚本类；未找到或有多个同名类时返回 nullptr</returns>
        static Ref<ScriptClass> ResolveScriptClass(const std::string& className);
```

**要点**：

- 声明放在 `public` 区（调用方是编辑器的 Inspector 与运行态，都需要访问）
- XML 注释里**明确写出匹配规则和失败语义** —— 这是本 Phase 最容易被误用的地方
- 参数名叫 `className` 而不是 `fileName`：接口收的是**类名**，至于它从哪来（文件名词干）是调用方的事，别把来源写进接口语义

### Step 2：`ScriptEngine.cpp` 加实现

**文件**：`Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`

加在 `EntityScriptClassExists` 的实现**之后**：

```cpp
    Ref<ScriptClass> ScriptEngine::ResolveScriptClass(const std::string& className)
    {
        if (className.empty())
        {
            return nullptr;
        }

        Ref<ScriptClass> matchedClass = nullptr;
        bool hasMultipleMatches = false;
        std::string matchedFullNames;

        for (const auto& [fullName, scriptClass] : s_Data->EntityClasses)
        {
            if (scriptClass->GetName() != className)
            {
                continue;
            }

            if (matchedClass)
            {
                // 已经命中过一个：记为冲突，继续收集候选名用于日志
                hasMultipleMatches = true;
                matchedFullNames += ", " + fullName;
                continue;
            }

            matchedClass = scriptClass;
            matchedFullNames = fullName;
        }

        if (hasMultipleMatches)
        {
            LF_CORE_ERROR("ScriptEngine::ResolveScriptClass - 类名 '{0}' 匹配到多个脚本类：{1}。请重命名文件或调整命名空间使其唯一。", className, matchedFullNames);
            return nullptr;
        }

        if (!matchedClass)
        {
            LF_CORE_ERROR("ScriptEngine::ResolveScriptClass - 未找到脚本类 '{0}'。请确认该脚本已参与编译，且类名与文件名一致。", className);
            return nullptr;
        }

        return matchedClass;
    }
```

**要点**：

- **遍历用结构化绑定** `const auto& [fullName, scriptClass]`，和 `OnRuntimeStop` 里的写法一致（规范 §13.9：范围 for 元素适合用 `auto`）
- `scriptClass` 是 `const Ref<ScriptClass>&`（map 的 value），但**返回类型是 `Ref<ScriptClass>`**，返回时拷一份引用计数 —— 这是安全的，调用方拿到后不会被 map 变更影响
- **不要**用 `auto& scriptClass = it->second;` 之类的显式迭代器写法 —— 范围 for 更短且不需要 `it` 管理
- **冲突检测写在循环内而不是先 `count` 再 `find`**：只需要遍历一次，且能顺带把候选全名收集齐
- 错误日志**用 `LF_CORE_ERROR`**，措辞包含"请确认该脚本已参与编译"（决策点 4.5）
- 两条日志都带 `ScriptEngine::ResolveScriptClass -` 前缀，和项目里其它日志的命名风格一致，便于 grep

### Step 3：编译验证

- Debug 编译 `Lucky` → 通过
- Debug 编译 `Luck3DApp` → 通过
- 不需要重跑 premake（无新增文件）

### Step 4（一次性自证，验收后删除）：在 `Init` 末尾解析一次示例脚本

**文件**：`Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`

在 `ScriptEngine::Init()` 末尾、`LF_CORE_INFO` **之前**临时插入：

```cpp
    Ref<ScriptClass> debugClass = ResolveScriptClass("PlayerController");
    if (debugClass)
    {
        LF_CORE_INFO("ScriptEngine: resolve test - PlayerController -> {}.{}", debugClass->GetNamespace(), debugClass->GetName());
    }
```

**预期**：启动日志出现 `ScriptEngine: resolve test - PlayerController -> Sandbox.PlayerController`。
**若没有**：说明程序集里确实没有这个类 —— 先确认 `Assembly-CSharp.dll` 编译过、且 `Luck3DApp/Project/Binaries/Assembly-CSharp.dll` 是最新的。

**⚠️ 验收完成后必须删除这段临时代码。** 不要留在 `Init` 里。

---

## 6. 疑点问答

### 6.1 为什么不用 `mono_class_from_name(..., "Sandbox", "PlayerController")` 直接找类？

因为**命名空间未知**。文件名只给了 `PlayerController`，它可能在 `Sandbox`、`Game`、全局命名空间下。`mono_class_from_name` 要求命名空间和名字都精确给全，做不到"按简单名找"。

而且 `EntityClasses` 是**引擎已经建好的、只含合法脚本类**的表 —— 直接查它比重新调 mono API 更省事、也更不容易出错（mono 那边还要自己判 `mono_class_is_subclass_of`）。

### 6.2 嵌套类（`Sandbox.Outer.Inner`）会怎样？

`LoadAssemblyClasses()` 用的是 `MONO_TABLE_TYPEDEF` 表 + `mono_metadata_string_heap` 取类名，**嵌套类在 C# 元数据里的名字是 `Outer/Inner`**（用斜杠分隔），存在但不会等于任何合法的文件名（文件名不能含 `/`）。

所以：

- 嵌套类**进得来** `EntityClasses`（key 形如 `Sandbox.Outer/Inner`），但 `GetName()` 是 `Outer/Inner`
- 匹配 `"Inner"` 会**失败**（正确行为 —— 嵌套类本来就不该挂在实体上）
- 结论：**不需要为嵌套类做特殊处理**，现有的"匹配不上就报错"已经是对的

### 6.3 类名和文件名一致、但那个类没继承 `Entity` 会怎样？

**不会命中**。`LoadAssemblyClasses()` 里有 `mono_class_is_subclass_of(monoClass, entityBaseClass, false)` 的过滤，非派生类根本不进 `EntityClasses`。所以本 Phase 的匹配结果**必定是合法脚本类**，不需要再验继承。

用户看到的现象是"未找到脚本类 XXX"，配上日志里那句"请确认该脚本已参与编译"，能覆盖"我把 `: Entity` 写漏了"这种情况（虽然措辞没直说，但方向对）。如果实测发现这种误写很常见，再补一句提示即可。

### 6.4 命名空间冲突的实际场景

比如两个人都写了个 `PlayerController`：

```csharp
namespace Sandbox { public class PlayerController : Entity { } }
namespace Tools   { public class PlayerController : Entity { } }
```

此时拖入 `PlayerController.cs` 会命中两个 → 报错并列出 `Sandbox.PlayerController, Tools.PlayerController`。

**注意**：这和"一个 `.cs` 文件里有多个类"是两回事。一个文件里写两个类，只要**只有一个**叫 `PlayerController`，匹配就是唯一的。文件名只约束"哪个类能被当组件用"，不约束"文件里能写几个类"。

### 6.5 能不能直接复用 `EntityScriptClassExists`？

不能，两者语义不同：

- `EntityScriptClassExists` 判断的是**全名是否存在**（`Sandbox.PlayerController`）
- `ResolveScriptClass` 要的是**按简单名找出那一个类**

可以组合出 `EntityScriptClassExists` 的新实现（先解析再判空），但**不建议** —— 现有的 map 直接 `find` 是最快的路径，改成遍历会变慢，且运行态调用频次高于资产态。**两个接口各留各的。**

### 6.6 什么时候会调用这个函数？（生命周期）

| 调用时机 | 调用方 | 说明 |
|---------|-------|------|
| 拖入 / 选中脚本时 | P2.3 的 Inspector | 用来判断"这个脚本能不能用"，解析失败要显示错误态 |
| 进入 Play 时 | P2.3 改造后的 `Scene::OnRuntimeStart` | 由 `Ref<Script>` 解析出类再实例化 |

**都不会在每帧调用**，所以性能不是问题（决策点 4.4）。

### 6.7 如果 `AppAssemblyImage` 没加载成功（`App.dll` 不存在）会怎样？

`LoadAssemblyClasses()` 会在 `if (!s_Data->AppAssemblyImage) return;` 处直接返回，`EntityClasses` 保持为空。此时任何 `ResolveScriptClass` 调用都会走进"未找到"分支并打 ERROR。

这是**正确**的行为：用户程序集不存在时，脚本本来就没法用。启动时 `LoadAppAssembly` 已经会打一条 WARN（`ScriptEngine: app assembly not found '...'`），两条日志连起来看因果很清晰。

---

## 7. 验收标准

1. **编译通过**：`Lucky` 与 `Luck3DApp` 的 Debug / Release / Dist 三个 configuration 全通过
2. **正例**：调用 `ResolveScriptClass("PlayerController")` 返回非空，且 `GetNamespace()` 是 `Sandbox`、`GetName()` 是 `PlayerController`
3. **反例（不存在）**：调用 `ResolveScriptClass("NotExist")` 返回 `nullptr`，日志出现 `未找到脚本类 'NotExist'` 且包含"请确认该脚本已参与编译"
4. **反例（空串）**：调用 `ResolveScriptClass("")` 返回 `nullptr`，**不产生日志**（空串是调用方没填，不是用户错误，不该报错）
5. **无副作用**：连续调用同一名字 100 次，返回结果稳定一致（验证没有引入不确定状态）
6. **不破坏既有行为**：`EntityScriptClassExists("Sandbox.PlayerController")` 仍返回 `true`；`Scene::OnRuntimeStart` 的脚本实例化行为与改动前完全一致
7. **临时代码已删除**：Step 4 的自证代码确认已从 `Init` 里移除
8. **代码规范**：通过人工 checklist —— 控制语句全带花括号（§5.2）；`for` 元素用 `auto`、智能指针引用的声明（§13.9）；公有接口有 `/// <summary>` 中文注释（§4.1）；无"为对齐而对齐"的空格（§5.4）；无引用外部文档的注释、无"P2.3 会用到"这类阶段性注释

> ⚠️ 第 3 条和第 4 条是**必须实测**的：日志内容决定用户能不能自己走出困境，光看代码看不出来措辞效果。

---

## 8. 对下一 Phase 的接线点

| 位置 | 后续 Phase | 打开方式 |
|------|-----------|---------|
| `ScriptEngine::ResolveScriptClass` | **P2.3** | Inspector 用它判断脚本可用性；`Scene::OnRuntimeStart` 用它把 `Ref<Script>` 变成 `ScriptClass` |
| 返回值 `Ref<ScriptClass>` | **P2.4** | 拿到它就能反射字段列表（`GetFields()` 会加在 `ScriptClass` 上） |
| 冲突/未找到两条日志 | **P2.3 / P3** | P2.3 的 Inspector 错误态可以直接复用同一套措辞；P3 热重载后重新 `LoadAssemblyClasses` 时天然重新解析 |

---

## 9. 变更清单速览

- **新增文件**：无
- **修改文件（2 个）**
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.h`：`ScriptEngine` 加 `ResolveScriptClass` 声明
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`：加 `ResolveScriptClass` 实现；临时自证代码（Step 4）用完删除
- **删除**：无
- **不改动**：`ScriptComponent.h`、`Asset/*`、`Serialization/*`、`Editor/*`、托管 C# 代码、premake（无新增文件）
- **接线给 P2.3**：`ResolveScriptClass` + `Script::GetClassName()`

---

## 10. 参考

- 编码规范 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：§2.3 命名惯例、§4.1 XML 注释、§5.2 强制花括号、§5.4 对齐规则、§13.9 auto 使用规范
- 前置详设 [Phase2.1_Script_As_Asset.md](Phase2.1_Script_As_Asset.md)（`Script` 资产与文件名词干）
- 前置实现 [Phase1.2_ScriptCore_Assembly.md](Phase1.2_ScriptCore_Assembly.md)（托管层 `Entity` 基类）
- 前置实现 [Phase1.3_ScriptEngine_Runtime.md](Phase1.3_ScriptEngine_Runtime.md)（`LoadAssemblyClasses` / `ScriptClass`）
- Unity「文件名必须与 MonoBehaviour 类名一致」的约定（本 Phase 匹配规则的对齐目标）
