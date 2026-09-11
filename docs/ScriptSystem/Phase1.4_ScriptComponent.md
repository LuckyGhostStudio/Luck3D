# Phase 1.4：ScriptComponent（把"脚本类名"变成一个真正的组件）

## 1. 概述

P1.4 目标：为 Luck3D 的 ECS 增加一个**纯数据组件** `ScriptComponent`，只带一个 `std::string ClassName` 字段（例如 `"Sandbox.PlayerController"`），并把它接入项目已有的四条基础设施：

1. `ComponentType` 枚举 / `ComponentTrait<T>` 特化
2. `ComponentRegistry` 三段式注册（Core / Serialization / Inspector）
3. `Scene::OnComponentAdded<T>` 主模板的空特化
4. `EditorIconManager` 的组件图标映射

**本 Phase 只做"数据侧"**：让实体可以挂上"脚本类名"，让 `ClassName` 参与 Scene::Copy（Play/Stop 拷贝）、SceneSerializer（存盘/读盘）、InspectorPanel（字符串编辑）。

**本 Phase 明确不做**：

- **不实例化 Mono 对象**：不在 `ScriptEngine::OnCreateEntityScript` 打开被注释掉的分支（留给 P1.6）
- **不引入字段序列化**：`ScriptFieldMap` / Inspector 字段动态绘制留给 Phase 2
- **不加合法性红字提示**：Inspector 里 `Class` 字段用最简单的字符串编辑框，不校验 `EntityScriptClassExists`（这是 Phase 2 的 UX 优化）
- **不改 Scene runtime 挂钩**：`Scene::OnUpdateRuntime` 内的脚本 view 遍历留给 P1.6

### 1.1 关键约束

- **组件文件位置**：`Lucky/Source/Lucky/Scene/Components/ScriptComponent.h`（与其它组件同目录）
- **枚举位置**：`ComponentType` 枚举中 `Camera` 之后、`// TODO 添加新组件` 之前追加 `Script`
- **注册顺序**：三段式（Cores / Serializations / Inspectors）都在末尾追加同一顺序，与既有约定一致
- **YAML 键名**：`ScriptComponent`（与所有组件命名风格统一：`XxxComponent`）
- **图标路径**：`Luck3DApp/Resources/Icons/Component/Script.png`（64×64，与其它组件图标同规格 [[memory:njikb5xe]]）；缺失时不阻塞功能（`GetComponentIcon` 会返回 static null Ref）
- **不 include `ScriptEngine.h`**：`ScriptComponent` 保持对脚本子系统的零依赖，避免把 mono 前向声明拉进 [Components.h](../../Lucky/Source/Lucky/Scene/Components/Components.h) 这个被广泛引用的汇总头

### 1.2 前置条件

- P0.4 已完成：三段式 `ComponentRegistry` 已就位（[ComponentRegistry.h](../../Lucky/Source/Lucky/Scene/ComponentRegistry.h)）
- P0.3 已完成：`Scene::Copy` 由 `ComponentRegistry::ForEach + desc.Copy` 驱动
- P1.3 已完成：`ScriptEngine::Init` 已能加载 Core / App Assembly；`EntityScriptClassExists / GetEntityClasses` 已可用（Inspector 后续加合法性提示时会用到，本 Phase 不用）

### 1.3 本 Phase **不做**的事

- 不打开 [ScriptEngine.cpp](../../Lucky/Source/Lucky/Scripting/ScriptEngine.cpp) 中 `OnCreateEntityScript` 里被注释的实例化分支（P1.6）
- 不为 `ScriptComponent` 增加除 `ClassName` 外的任何字段（Phase 2 才引入 `FieldMap`）
- 不给 Inspector 加"下拉选择用户脚本类"（Phase 2）
- 不加"ClassName 非法时的红字提示"（Phase 2）
- 不改 `SceneSerializer.cpp` 本体（三段式注册后 YAML 分发全靠 `ComponentRegistry`）

---

## 2. 涉及的文件

### 2.1 新建

| 路径 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Scene/Components/ScriptComponent.h` | 纯数据组件：`std::string ClassName` |
| `Luck3DApp/Resources/Icons/Component/Script.png` | 组件图标（64×64，可后补，不阻塞） |

### 2.2 修改

| 文件 | 改动 |
|------|------|
| [ComponentType.h](../../Lucky/Source/Lucky/Scene/Components/ComponentType.h) | 枚举 `Camera` 之后追加 `Script` |
| [Components.h](../../Lucky/Source/Lucky/Scene/Components/Components.h) | `#include "ScriptComponent.h"` + `ComponentTrait<ScriptComponent>` 特化 |
| [ComponentCores.cpp](../../Lucky/Source/Lucky/Scene/ComponentCores.cpp) | `RegisterAllCores()` 末尾追加 `ScriptComponent` 的 Core 块 |
| [ComponentSerializers.cpp](../../Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp) | 新增 `Serialize_Script / Deserialize_Script` + `RegisterAllSerializations` 尾部注册 |
| [ComponentInspectors.cpp](../../Lucky/Source/Lucky/Editor/ComponentInspectors.cpp) | 新增 `Draw_Script` + `RegisterAllInspectors` 尾部注册 |
| [EditorIconManager.cpp](../../Lucky/Source/Lucky/Editor/EditorIconManager.cpp) | `Init()` 为 `ComponentType::Script` 加载图标 |
| [Scene.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp) | 追加 `OnComponentAdded<ScriptComponent>` 空特化 |

### 2.3 不修改

- `ScriptEngine.h/.cpp`：本 Phase 不 include `ScriptComponent.h`，`OnCreateEntityScript` 内注释保留原样（P1.6 才打开）
- `SceneSerializer.cpp`：YAML 分发已交给 `ComponentRegistry`，本体无需感知新组件
- `Scene::Copy`：已由 `ComponentRegistry::ForEach + desc.Copy` 驱动，Core 注册后自动生效
- `Entity.h`：`AddComponent<T> / GetComponent<T> / HasComponent<T> / RemoveComponent<T>` 是模板，无需为新组件改任何代码

---

## 3. 现状回顾

### 3.1 三段式注册套路

由 P0.4 建立、[ComponentRegistry.h](../../Lucky/Source/Lucky/Scene/ComponentRegistry.h) 顶部注释明确说明的：

```
RegisterCore（Type / Name / Copy / Has）
    位于 Scene/ComponentCores.cpp
RegisterSerialization（SerializedKey / Serialize / Deserialize）
    位于 Serialization/ComponentSerializers.cpp
RegisterInspector（Draw / GetIcon / AddMenuItems / Remove / 展示控制）
    位于 Editor/ComponentInspectors.cpp
```

三处的注册顺序必须一致（`ComponentRegistry::RegisterAll` 会依次调用三个 `RegisterAllXxx()`）。项目现有 9 个组件（`Name / Transform / Relationship / Light / MeshFilter / MeshRenderer / SpriteRenderer / PostProcessVolume / Camera`）均按此顺序注册，`Script` 追加到末尾即可。

### 3.2 `Scene::Copy` 天然覆盖

[Scene.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp) 中 `Scene::Copy` 已改造为遍历 `ComponentRegistry` 并调用每个 `desc.Copy`。只要 `ScriptComponent` 的 Core 注册到位（`desc.Copy = &CopyComponentValue<ScriptComponent>`），Play/Stop 的场景快照就会天然带上 `ClassName`——`std::string` 是值类型，`entt::registry::emplace_or_replace` 做的就是值拷贝。

### 3.3 `Scene::OnComponentAdded` 主模板陷阱

[Scene.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp) 的模板主实现是：

```cpp
template<typename TComponent>
void Scene::OnComponentAdded(Entity entity, TComponent& component)
{
    static_assert(sizeof(TComponent) == 0);
}
```

任何**未特化**的组件类型在 `AddComponent<T>` 被实例化时都会编译期报错。因此必须为 `ScriptComponent` 追加一个空特化——**这一步忘了就编不过**。

### 3.4 Inspector 的 `PropertyString` 已就绪

[PropertyGrid.h](../../Lucky/Source/Lucky/UI/PropertyGrid.h) 已提供：

```cpp
bool PropertyString(const char* label, char* value, size_t bufSize);
void PropertyReadOnlyString(const char* label, const char* value);
```

配合 `BeginPropertyGrid / EndPropertyGrid` 即可实现 ClassName 的编辑框，无需新增控件。

### 3.5 `EditorIconManager` 图标注册模式

[EditorIconManager.cpp](../../Lucky/Source/Lucky/Editor/EditorIconManager.cpp) 的 `Init()` 里为每个 `ComponentType` 加一行 `LoadIcon("Component/Xxx.png")` 即可；图标缺失时 `LoadIcon` 打 warn 并返回 nullptr，`GetComponentIcon` 会 fallback 到 static null Ref——**不会崩，只会显示空白**。

---

## 4. 关键设计决策（多方案对比）

### 4.1 决策点 1：`ScriptComponent` 的字段范围

#### 方案 A：只保留 `std::string ClassName`（**推荐 ✅**）

```cpp
struct ScriptComponent
{
    std::string ClassName;

    ScriptComponent() = default;
    ScriptComponent(const ScriptComponent& other) = default;
    ScriptComponent(const std::string& className)
        : ClassName(className) {}
};
```

- **优点**
  - 保持"纯 POD 数据"，与项目现有组件设计一致（数据在组件、运行时状态在系统）
  - Scene::Copy 天然可拷贝（`std::string` 值语义）
  - 不引入对 `ScriptEngine` 的 include 依赖，`Components.h` 汇总头保持零 mono 依赖
  - 符合 [Phase1.3 §4.10](Phase1.3_ScriptEngine_Runtime.md) 已经拍板的架构决定：`ScriptInstance` 由 `ScriptEngine::EntityInstances` 统一管理，**不放到组件里**
- **缺点**
  - `OnUpdateRuntime` 里查找 `ScriptInstance` 要走 `unordered_map<UUID, Ref<ScriptInstance>>` 一层间接，但 Phase 1.3 已完成该基础设施，不是本 Phase 引入的额外成本

#### 方案 B：现在就加 `FieldMap`

```cpp
struct ScriptComponent
{
    std::string ClassName;
    std::unordered_map<std::string, ScriptFieldValue> Fields;  // Phase 2 引入
};
```

- **优点**：一步到位，Phase 2 少改动
- **缺点**
  - `ScriptFieldValue` 还未定义（Phase 2 才引入）
  - 序列化 / Inspector 的字段动态绘制都要一起做，工作量翻数倍
  - 违反"当下形态" [[memory:du4s18ic]]：本 Phase 只需支持 MVP，字段留给 Phase 2

#### 方案 C：加 `Ref<ScriptInstance> Instance` 指针

- **缺点**（3 条硬伤）
  - `ScriptComponent` 是纯数据 `struct`，塞 `Ref<ScriptInstance>` 破坏"POD 语义"
  - Scene::Copy 会误拷 MonoObject 句柄（哪怕改成不拷也是特殊处理）
  - SceneSerializer 又必须跳过该字段
  - 违反 [Phase1.3 §4.10](Phase1.3_ScriptEngine_Runtime.md) 已拍板的架构

**结论**：采用 **方案 A**。

### 4.2 决策点 2：`ScriptComponent` 是否 include `ScriptEngine.h`

#### 方案 A：完全不 include（**推荐 ✅**）

```cpp
// ScriptComponent.h
#pragma once
#include <string>
namespace Lucky { struct ScriptComponent { std::string ClassName; ... }; }
```

- **优点**
  - `Components.h` 是引擎汇总头，被 [Entity.h](../../Lucky/Source/Lucky/Scene/Entity.h) 大量间接引用；若 `ScriptComponent.h` 把 `ScriptEngine.h` 拉进来，则 mono `extern "C"` 前向声明会污染每一个引用 `Components.h` 的编译单元
  - 保持 mono 的隔离性：只有 `ScriptEngine.cpp` 及后续 `ScriptGlue.cpp` 才 include mono 头
  - 组件只是"数据描述"，没有理由知道运行时的存在
- **缺点**：无

#### 方案 B：include `ScriptEngine.h`（"顺便存个 Instance"）

- **缺点**：mono 前向声明扩散到 `Components.h` → `Entity.h` → 整个引擎；同时也把方案 4.1-C 的架构问题带回来

**结论**：采用 **方案 A**。

### 4.3 决策点 3：Inspector 中 `Class` 字段的控件形态

#### 方案 A：`UI::PropertyString`（纯字符串编辑框，**推荐 ✅**）

```cpp
char buf[256] = {};
std::strncpy(buf, sc.ClassName.c_str(), sizeof(buf) - 1);

UI::BeginPropertyGrid();
if (UI::PropertyString("Class", buf, sizeof(buf)))
{
    sc.ClassName = buf;
}
UI::EndPropertyGrid();
```

- **优点**
  - 与 Roadmap 明确要求一致："Inspector 展示当前 ClassName（字符串编辑框即可，Phase 2 再做下拉选择）"
  - 用现成 `PropertyString`，零新控件成本
  - 用户可以输入任意值（含还没写完的类名），不阻挡工作流
- **缺点**：无合法性提示（这是 Phase 2 的目标）

#### 方案 B：直接做下拉选择（Combo from `ScriptEngine::GetEntityClasses()`）

- **优点**：用户体验好，一步到位
- **缺点**
  - Roadmap 明确把下拉留给了 Phase 2
  - 需要 include `ScriptEngine.h`（4.2 的问题回来了）
  - `ScriptEngine::GetEntityClasses` 会因 `App.dll` 缺失/未编译返回空 map，此时 Inspector 完全无法输入类名 → 反而增加了阻塞

#### 方案 C：字符串 + 合法性红字提示

```cpp
if (UI::PropertyString("Class", buf, sizeof(buf))) { sc.ClassName = buf; }
if (!ScriptEngine::EntityScriptClassExists(sc.ClassName))
{
    ImGui::TextColored({1,0.3f,0.3f,1}, "class not found");
}
```

- **优点**：兼具"随意输入"和"提示错误"
- **缺点**
  - include `ScriptEngine.h`（4.2 问题）
  - Roadmap 未列入本 Phase
  - 属于渐进增强，Phase 2 一起做更集中

**结论**：采用 **方案 A**（Phase 2 再升级为方案 B 或 C）。

### 4.4 决策点 4：`ScriptComponent` 的构造函数集

#### 方案 A：默认 + 拷贝 + `const std::string&` 便利构造（**推荐 ✅**）

```cpp
ScriptComponent() = default;
ScriptComponent(const ScriptComponent& other) = default;
ScriptComponent(const std::string& className)
    : ClassName(className) {}
```

- **优点**：与 [NameComponent.h](../../Lucky/Source/Lucky/Scene/Components/NameComponent.h) 完全一致的模式；`AddComponent<ScriptComponent>("Sandbox.PlayerController")` 一步到位
- **缺点**：无

#### 方案 B：只有默认构造

- **缺点**：不能 `AddComponent<ScriptComponent>("Sandbox.Foo")`，测试/工具代码要多两行

#### 方案 C：加上移动构造 / `std::string&&` 版本

- **缺点**：`std::string` 已经有移动构造，编译器合成的 `= default` 拷贝构造已经足够；额外加移动语义对**只有一个 string 字段**的结构体收益极小，反而与其它组件不一致

**结论**：采用 **方案 A**。

### 4.5 决策点 5：`ComponentType` 枚举中 `Script` 的位置

#### 方案 A：追加到 `Camera` 之后（**推荐 ✅**）

```cpp
enum class ComponentType : uint8_t
{
    None = 0,
    Name,
    ...
    Camera,
    Script,   // ← 新增

    // TODO 添加新组件
};
```

- **优点**
  - 三段式注册顺序在末尾追加，最不破坏既有场景 YAML 的字段书写顺序
  - 与代码库里"新组件追加"的约定一致
- **缺点**：无（枚举的数值本身不参与 YAML 序列化，只用作 map key）

#### 方案 B：按字母序插入（在 `PostProcessVolume` 后）

- **缺点**
  - 破坏"注册顺序 = Inspector 出现顺序"的既有约定
  - 老场景文件重新保存后 YAML 字段顺序改变，diff 噪音大

**结论**：采用 **方案 A**。

### 4.6 决策点 6：`Draw_Script` 的字符串缓冲区策略

Inspector 每帧都会重绘。`sc.ClassName` 是 `std::string`，但 `UI::PropertyString` 要 `char*`。有三种做法：

#### 方案 A：函数内栈上 `char buf[256]`，每帧拷贝一次（**推荐 ✅**）

```cpp
void Draw_Script(Entity entity)
{
    ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

    char buf[256] = {};
    std::strncpy(buf, sc.ClassName.c_str(), sizeof(buf) - 1);

    UI::BeginPropertyGrid();
    if (UI::PropertyString("Class", buf, sizeof(buf)))
    {
        sc.ClassName = buf;
    }
    UI::EndPropertyGrid();
}
```

- **优点**
  - 与项目内其它字符串型 Inspector 的既有做法一致（可以在 Inspector 相关代码中搜到同型模式）
  - 无额外状态，无生命周期问题
  - 256 字节栈开销可忽略
- **缺点**：256 字符上限（`Namespace.ClassName` 极少超过，实际上 Hazel/Unity 也是同样上限）

#### 方案 B：`ImGui::InputText(..., &sc.ClassName)` 使用 stdlib 扩展

- **优点**：无长度上限
- **缺点**
  - 需要 include `<misc/cpp/imgui_stdlib.h>`（项目当前 UI 层没有统一使用）
  - 绕过 `UI::PropertyString`，无法自动融入 `PropertyGrid` 两列布局，需要自己拼 `PropertyLabel + PropertyValueBegin/End`
  - 与既有 UI 一致性差

#### 方案 C：`static char buf[256]` 保留跨帧状态

- **缺点**：切换实体后残留旧值，严重的 UX bug

**结论**：采用 **方案 A**。

### 4.7 决策点 7：图标缺失时的行为

#### 方案 A：`LoadIcon` 缺失时打 warn，`GetComponentIcon` 返回 static null Ref（**推荐 ✅**，现状即如此）

- **优点**
  - 图标可后补，不阻塞开发流水线
  - 即使美术资源没到位，Inspector / Hierarchy 也能正常运作，只是显示空白格
- **缺点**：无

#### 方案 B：图标必须存在，缺失时 `LF_CORE_ASSERT`

- **缺点**：把功能验收和资源制作强耦合，不利于并行开发

**结论**：采用 **方案 A**（沿用 [EditorIconManager](../../Lucky/Source/Lucky/Editor/EditorIconManager.cpp) 现有语义）。**图标 PNG 制作可作为独立子任务，与代码改动并行**。

---

## 5. 实现步骤（按依赖顺序）

每一步做完后，`Lucky` 与 `Luck3DApp` 都应能编译通过。步骤按"底向上、破坏面最小"的顺序：

### Step 1：新建 `ScriptComponent.h`

**文件**：`Lucky/Source/Lucky/Scene/Components/ScriptComponent.h`

```cpp
#pragma once

#include <string>

namespace Lucky
{
    /// <summary>
    /// 脚本组件：把一个用户 C# 脚本类挂到实体上
    /// 字段仅有 ClassName（例如 "Sandbox.PlayerController"）
    /// 运行时的 ScriptInstance 由 ScriptEngine 统一管理，不放在本组件里
    /// </summary>
    struct ScriptComponent
    {
        std::string ClassName;              // 用户脚本类的全名（Namespace.ClassName）

        ScriptComponent() = default;
        ScriptComponent(const ScriptComponent& other) = default;
        ScriptComponent(const std::string& className)
            : ClassName(className) {}
    };
}
```

**要点**：

- 不 include `ScriptEngine.h`（决策 4.2-A）
- 构造函数集与 `NameComponent` 对齐（决策 4.4-A）
- 字段行注释与结构体行末对齐允许 [[memory:ebdvlsub]]（这是"结构体字段声明后行末注释前"的允许对齐场景）

### Step 2：`ComponentType` 枚举追加 `Script`

**文件**：[ComponentType.h](../../Lucky/Source/Lucky/Scene/Components/ComponentType.h)

在 `Camera` 之后、`// TODO 添加新组件` 之前追加：

```cpp
enum class ComponentType : uint8_t
{
    None = 0,
    Name,
    Relationship,
    Transform,
    Light,
    MeshFilter,
    MeshRenderer,
    SpriteRenderer,
    PostProcessVolume,
    Camera,
    Script,             // ← 新增

    // TODO 添加新组件
};
```

**要点**：只追加一行，不动其它枚举项。

### Step 3：`Components.h` 追加 include + `ComponentTrait` 特化

**文件**：[Components.h](../../Lucky/Source/Lucky/Scene/Components/Components.h)

在 `#include "CameraComponent.h"` 之后追加：

```cpp
#include "ScriptComponent.h"
```

在文件末尾 `ComponentTrait<CameraComponent>` 特化块之后追加：

```cpp
    template<> struct ComponentTrait<ScriptComponent>
    {
        static constexpr ComponentType Type = ComponentType::Script;
    };
```

**要点**：保持与其它 Trait 特化完全同型（单空格、无对齐 [[memory:ebdvlsub]]）。

### Step 4：`Scene::OnComponentAdded<ScriptComponent>` 空特化

**文件**：[Scene.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp)

在 `OnComponentAdded<CameraComponent>` 特化之后、`// TODO 添加新组件` 之前追加：

```cpp
    template<>
    void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent& component)
    {

    }
```

**要点**：

- 空实现即可——本 Phase 不做 runtime 挂钩，P1.6 才可能往这里加"如果当前场景处于 Play 状态且 `EntityScriptClassExists` 则实例化并 `InvokeOnCreate`"（但按 Phase1.3 §8 的接线表，那段逻辑更倾向于放在 `Scene::OnRuntimeStart` 的初始扫描里，而非这里）
- 不写"P1.6 会补齐"这类阶段性注释 [[memory:du4s18ic]]

### Step 5：`ComponentCores.cpp` 注册 Core

**文件**：[ComponentCores.cpp](../../Lucky/Source/Lucky/Scene/ComponentCores.cpp)

在 `// ---- CameraComponent ----` 注册块之后追加：

```cpp
        // ---- ScriptComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::Script;
            desc.Name = "Script";
            desc.Copy = &CopyComponentValue<ScriptComponent>;
            desc.Has = [](Entity e) { return e.HasComponent<ScriptComponent>(); };
            RegisterCore(std::move(desc));
        }
```

**要点**：与其它组件的 Core 块完全同构。`CopyComponentValue<T>` 是同文件里的通用模板，`std::string` 值拷贝即为深拷贝。

### Step 6：`ComponentSerializers.cpp` 注册序列化

**文件**：[ComponentSerializers.cpp](../../Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp)

在 `Deserialize_Camera` 之后、匿名 namespace 关闭之前追加：

```cpp
        // ======== ScriptComponent ========

        void Serialize_Script(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<ScriptComponent>())
            {
                return;
            }
            const ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

            out << YAML::Key << "ScriptComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "ClassName" << YAML::Value << sc.ClassName;
            out << YAML::EndMap;
        }

        void Deserialize_Script(Entity entity, const YAML::Node& entityNode)
        {
            YAML::Node node = entityNode["ScriptComponent"];
            if (!node)
            {
                return;
            }
            ScriptComponent& sc = entity.AddComponent<ScriptComponent>();
            sc.ClassName = node["ClassName"].as<std::string>("");
        }
```

在 `RegisterAllSerializations` 末尾（`RegisterSerialization(ComponentType::Camera, ...)` 后）追加：

```cpp
        RegisterSerialization(ComponentType::Script,            "ScriptComponent",            &Serialize_Script,            &Deserialize_Script);
```

**要点**：

- YAML 键 `ScriptComponent`（与既有组件的命名一致）
- 字段键 `ClassName`
- `Deserialize` 中 `node["ClassName"].as<std::string>("")` 使用 yaml-cpp 的默认值形式，保护老场景（`ClassName` 缺失或为空时 fallback 到空串，不抛异常）
- 注册行的对齐是"注册块内部的高度对称重复"，允许沿用现有对齐风格 [[memory:ebdvlsub]]

### Step 7：`ComponentInspectors.cpp` 注册 Inspector

**文件**：[ComponentInspectors.cpp](../../Lucky/Source/Lucky/Editor/ComponentInspectors.cpp)

顶部 include 保持不变（无需 include `ScriptEngine.h`，见决策 4.2）。

在 `Draw_Camera` 之后、`RegisterAllInspectors` 之前追加：

```cpp
        // ======== ScriptComponent ========

        void Draw_Script(Entity entity)
        {
            ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

            char buf[256] = {};
            std::strncpy(buf, sc.ClassName.c_str(), sizeof(buf) - 1);

            UI::BeginPropertyGrid();
            if (UI::PropertyString("Class", buf, sizeof(buf)))
            {
                sc.ClassName = buf;
            }
            UI::EndPropertyGrid();
        }
```

在 `RegisterAllInspectors` 末尾 `CameraComponent` 注册块之后追加：

```cpp
        // ---- ScriptComponent ----
        RegisterInspector(ComponentType::Script,
            &Draw_Script,
            &DefaultIcon<ScriptComponent>,
            {
                { "Script",
                  []() -> const Ref<Texture2D>& { return EditorIconManager::GetComponentIcon(ComponentType::Script); },
                  [](Entity e) { e.AddComponent<ScriptComponent>(); } },
            },
            [](Entity e) { e.RemoveComponent<ScriptComponent>(); });
```

**要点**：

- 若 `<cstring>` 未在 PCH（`lcpch.h`）中，可能需要在 `ComponentInspectors.cpp` 顶部 `#include <cstring>`。项目现状下 PCH 已包含 `<string>`，`std::strncpy` 通常随 `<cstring>` 一并可用，如编译报错再显式补 include
- `DefaultIcon<ScriptComponent>` 依赖 Step 3 的 `ComponentTrait<ScriptComponent>` 特化
- AddMenu 项标签用 `"Script"`，与 `Camera` 等单一入口组件一致
- Remove 回调直接使用现成的 `RemoveComponent<T>`

### Step 8：`EditorIconManager.cpp` 加载 Script 图标

**文件**：[EditorIconManager.cpp](../../Lucky/Source/Lucky/Editor/EditorIconManager.cpp)

在 `Init()` 内 `s_IconData.ComponentIcons[ComponentType::Camera] = LoadIcon("Component/Camera.png");` 之后追加：

```cpp
        s_IconData.ComponentIcons[ComponentType::Script]             = LoadIcon("Component/Script.png");
```

**要点**：

- 允许沿用块内既有的注册对齐风格（"注册块内部的高度对称重复" [[memory:ebdvlsub]]）
- 图标 PNG 缺失时 `LoadIcon` 打 warn 并返回 nullptr，`GetComponentIcon` 会 fallback；不阻塞验收
- 图标路径：`Luck3DApp/Resources/Icons/Component/Script.png`（64×64 [[memory:njikb5xe]]）

### Step 9：premake 重新生成 + 编译验证

- 运行 `Scripts/Setup-Windows.bat`（或直接跑 `premake5 --file=Build.lua vs2022`），确保 `ScriptComponent.h` 被 `Lucky.vcxproj` 收录
- Debug 编译 `Lucky` → 通过
- Debug 编译 `Luck3DApp` → 通过
- 启动 `Luck3DApp.exe`：
  - 无 mono 相关新报错（本 Phase 未改脚本引擎）
  - 日志中出现（若图标缺失）：`EditorIconManager: Failed to load icon 'Resources/Icons/Component/Script.png'`——可接受
- 打开任意场景，Hierarchy 选中一个实体 → Inspector 面板底部 `Add Component` → 应能看到 **"Script"** 选项

---

## 6. 疑点问答

### 6.1 为什么不顺便把 `Scene::OnRuntimeStart` 里的脚本实例化打开？

答：**分层解耦**。P1.6 是"Scene runtime 集成"的独立里程碑；P1.4 只负责让 `ScriptComponent` 作为数据存在。这样做的好处：

1. 提交粒度清晰，一个 PR 只做一件事
2. 若 P1.6 出现 mono/Scene 交互问题（AppDomain 生命周期、GC pin 等），可以单独回退而不影响 P1.4 的数据能力
3. P1.5（`ScriptGlue`）可以在 P1.6 之前独立完成、独立验收（`RegisterFunctions` 在 `ScriptEngine::Init` 里挂即可，不依赖 `Scene::OnRuntimeStart`）

### 6.2 `Deserialize_Script` 是否要检查 `ClassName` 是否为空？

答：**不检查**。允许"空 ClassName"存在——这对应"用户已挂 `ScriptComponent`，但还没决定绑哪个类"的正常中间状态。P1.6 的 `OnCreateEntityScript` 会在 `EntityScriptClassExists(sc.ClassName) == false` 时静默跳过，不会崩。

### 6.3 `Scene::OnComponentAdded<ScriptComponent>` 空特化里，未来会加什么？

答：由 P1.6 决定。目前 P1.3 §8 的接线表倾向于把"实例化脚本"放在 `Scene::OnRuntimeStart` 的一次性扫描中，而不是 `OnComponentAdded`——因为 Play 中间新加 `ScriptComponent` 是罕见路径。**但这不是 P1.4 需要决定的**，空实现即可。

### 6.4 `ScriptComponent` 需要提供 `operator std::string&()` 之类的转换吗？

答：**不需要**。`NameComponent` 提供隐式转 `std::string&` 是因为它经常被"直接当字符串用"（比如日志输出）。`ScriptComponent.ClassName` 语义上是"类名索引"，永远显式 `.ClassName` 使用更清晰，隐式转换反而会引起意外重载解析。

### 6.5 序列化时是否要把 `ClassName` 写成 `Class`（更短）？

答：**不要**。字段名 `ClassName` 与 C++ 侧字段名保持一致，可读性 > 简洁性；也和 Hazel Engine 场景文件的写法一致，便于对照参考。

### 6.6 `ComponentType::Script` 后续被 P2 增加了 `FieldMap` 会怎样？

答：Phase 2 会在 `ScriptComponent` 里追加 `Fields` 字段，`Serialize_Script / Deserialize_Script` 里追加 Fields 段的读写，Inspector 里追加字段动态绘制。**不会修改 `ComponentType::Script` 本身**，也不会影响 P1.4 保存的场景文件（Phase 2 反序列化时 `Fields` 段不存在则默认空 map）。P1.4 的接口对 Phase 2 完全前向兼容。

### 6.7 Inspector 里 `Class` 字段是否需要 tooltip 说明 "Namespace.ClassName" 格式？

答：**不加**。项目现有 Inspector 控件都不带 tooltip，风格统一优先。用户手册与 Phase 2 的下拉选择会更好地解决"用户不知道格式"的问题。

### 6.8 `Scene::Copy` 里需要为 `ScriptComponent` 做特殊处理吗？

答：**不需要**。Copy 已改造为 `ComponentRegistry` 驱动，Core 注册后 `desc.Copy = &CopyComponentValue<ScriptComponent>` 会自动被调用。`std::string` 值拷贝语义天然正确。**唯一要注意的**：若 Play 中触发 Scene::Copy（当前项目中 Play/Stop 才触发一次），`ScriptComponent` 会被拷贝，但对应的 `ScriptInstance`（存在 `ScriptEngine::EntityInstances`，key=UUID）**不会**被拷贝——这是 Phase1.3 §4.10 决策的直接结果，也正是我们想要的隔离。

---

## 7. 验收标准

MVP 级验收（P1.4 独立完成后应满足全部）：

1. **编译通过**：`Lucky` 与 `Luck3DApp` 的 Debug / Release / Dist 三个 configuration 全通过；`Scene::OnComponentAdded<ScriptComponent>` 空特化到位
2. **AddComponent 工作**：Inspector 底部 `Add Component` 弹出菜单中有 **"Script"** 项，点击后实体上出现 "Script" 折叠块，`Class` 字段为空可输入
3. **字段编辑**：`Class` 字段输入 `"Sandbox.PlayerController"`，切换焦点后值保留（`sc.ClassName` 被正确赋值）
4. **场景存盘**：`Ctrl+S` 保存场景 → 打开 `.lucky` 文件，可见：
   ```yaml
   ScriptComponent:
     ClassName: Sandbox.PlayerController
   ```
5. **场景读盘**：重启编辑器，重新打开该场景，选中实体，`Class` 字段仍显示 `Sandbox.PlayerController`
6. **Play/Stop 拷贝**：
   - Play 前实体的 `ClassName = A`
   - Play 期间不改 `ClassName`，Stop 后仍为 `A`
   - Play 期间把 `ClassName` 改为 `B`，Stop 后应回到 `A`（Scene::Copy 的既有回滚能力）
7. **Remove 工作**：Script 折叠块右上齿轮 → Remove Component → `ScriptComponent` 被移除
8. **不影响脚本引擎**：`Luck3DApp` 启动日志仍显示 `ScriptEngine: initialized (N user script classes loaded)`；Play/Stop 期间无 mono 相关 warn/error（因为 P1.4 完全不触发脚本实例化）
9. **图标显示（可选）**：若 `Resources/Icons/Component/Script.png` 已准备，Hierarchy 面板右侧和 Inspector 折叠块左侧显示 Script 图标；若未准备，显示空白格，不影响其它功能

---

## 8. 对下一 Phase 的接线点

本 Phase 完成后，这些钩子已"数据侧就绪、待运行时打开"：

| 位置 | 后续 Phase | 打开方式 |
|------|-----------|---------|
| `ScriptEngine::OnCreateEntityScript` 内被注释的分支 | **P1.6** | 取消注释，`entity.GetComponent<ScriptComponent>()` 已可编译（`ScriptEngine.cpp` 需 `#include "Lucky/Scene/Components/ScriptComponent.h"`） |
| `Scene::OnRuntimeStart` 首次实现 | **P1.6** | 扫描 `m_Registry.view<ScriptComponent>()`，对每个实体调 `ScriptEngine::OnCreateEntityScript(entity)` |
| `Scene::OnUpdateRuntime` 脚本 tick | **P1.6** | 扫描 `m_Registry.view<ScriptComponent>()`，对每个实体调 `ScriptEngine::OnUpdateEntityScript(entity, dt)` |
| `Scene::OnRuntimeStop` 首次实现 | **P1.6** | 调 `ScriptEngine::OnRuntimeStop()`（清空 `EntityInstances`） |
| Inspector `Class` 字段合法性提示 | **Phase 2** | 增加红字：`if (!sc.ClassName.empty() && !ScriptEngine::EntityScriptClassExists(sc.ClassName))` |
| Inspector `Class` 字段下拉选择 | **Phase 2** | 用 `ScriptEngine::GetEntityClasses()` 生成 combo 选项 |
| `ScriptComponent::Fields`（字段序列化 + Inspector 动态字段） | **Phase 2** | 结构体追加 `FieldMap`；`Serialize_Script / Deserialize_Script` 增加 Fields 段读写；`Draw_Script` 追加字段循环绘制 |

---

## 9. 变更清单速览

- **新增文件（1 个 .h + 1 张可选 PNG）**
  - `Lucky/Source/Lucky/Scene/Components/ScriptComponent.h`
  - `Luck3DApp/Resources/Icons/Component/Script.png`（可后补）
- **修改文件（7 个）**
  - [ComponentType.h](../../Lucky/Source/Lucky/Scene/Components/ComponentType.h) —— 枚举追加 `Script`
  - [Components.h](../../Lucky/Source/Lucky/Scene/Components/Components.h) —— include + Trait 特化
  - [Scene.cpp](../../Lucky/Source/Lucky/Scene/Scene.cpp) —— `OnComponentAdded<ScriptComponent>` 空特化
  - [ComponentCores.cpp](../../Lucky/Source/Lucky/Scene/ComponentCores.cpp) —— Core 注册块
  - [ComponentSerializers.cpp](../../Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp) —— Serialize/Deserialize + 注册行
  - [ComponentInspectors.cpp](../../Lucky/Source/Lucky/Editor/ComponentInspectors.cpp) —— Draw + Inspector 注册块
  - [EditorIconManager.cpp](../../Lucky/Source/Lucky/Editor/EditorIconManager.cpp) —— 图标加载行
- **删除**：无
- **不改动**：`ScriptEngine.h/.cpp`、`SceneSerializer.cpp`、`Entity.h`、`Build-Lucky.lua`

---

## 10. 参考

- Roadmap [ScriptSystem_Roadmap.md](ScriptSystem_Roadmap.md) 第 3 节 (4) "ScriptComponent" 与第 3 节 (8) "Inspector 展示"
- 前置详设 [Phase0.4_ComponentRegistry.md](Phase0.4_ComponentRegistry.md)（三段式注册的原始约定）
- 前置详设 [Phase1.3_ScriptEngine_Runtime.md](Phase1.3_ScriptEngine_Runtime.md) §4.10（`ScriptInstance` 归属决策，本 Phase 沿用）与 §8（接线点表）
- 代码规范 [Coding_Style_Guide.md](../Coding_Style_Guide.md) 第 5 节（格式）、第 6 节（类/结构体）、第 13.3 节（ECS 组件模式）
- Hazel Engine `ScriptComponent`（结构参考）
