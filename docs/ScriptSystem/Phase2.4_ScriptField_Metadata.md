# Phase 2.4：脚本字段元信息（反射）

## 1. 概述

让 `ScriptClass` 能**枚举自己的 public 实例字段**，并提供一个通用的"字段值"容器。这是 P2.5（把值存进组件）、P2.6（画控件 + 灌值）共同的底座。

产出三件东西：

1. **`ScriptFieldType` + `ScriptFieldValue`**（放新头 `Scripting/ScriptFieldValue.h`）—— 纯数据，零 mono 依赖
2. **`ScriptField`**：字段元信息（名字 + 类型 + `MonoClassField*`），只在 `Scripting` 层内部使用
3. **`ScriptClass::GetFields()`**：懒构建并缓存字段列表；配套 `GetFieldValue` / `SetFieldValue` 读写任意实例的字段

**本 Phase 不产出任何 UI，也不改组件。** 目标是把"脚本里有哪些字段"这件事变成可查询的数据。

### 1.1 关键约束

- **`ScriptFieldValue` 必须零 mono 依赖**。`ScriptComponent`（P2.5）要把它用作 `FieldMap` 的 value 类型；如果它定义在 `ScriptEngine.h` 里，`ScriptComponent.h` 就得 include 整个 `ScriptEngine.h`（那个头又 include 了 `Scene.h`），把脚本运行时、场景、mono 前向声明全拖进组件层。**所以它单独一个头。** 详见决策点 4.3。
- **必须过滤三类字段**，否则 Inspector 上会冒出幽灵项：
  - 非 public（`mono_field_get_flags` 的访问位不是 `MONO_FIELD_ATTR_PUBLIC`）
  - static（`MONO_FIELD_ATTR_STATIC`）
  - **编译器生成**（C# 的 `public float Speed { get; set; }` 会生成 `<Speed>k__BackingField`，名字里带 `<`）
- **基类 `Lucky.Entity` 的 `ID` 字段必须排除**。`Entity` 里是 `public readonly ulong ID;` —— 它是 public 实例字段，会被枚举出来。`ID` 是只读的（`MONO_FIELD_ATTR_INIT_ONLY`），而且用户不该在 Inspector 里改它。
- **字段类型支持范围要写死并明确报"不支持"**：本 Phase 只支持 `float` / `int` / `bool` / `Lucky.Vector3`；其余类型**跳过并打一条 WARN**，不要静默、也不要崩（`Lucky.Entity` 引用、`string`、数组、自定义 struct 都留到以后）。
- **`GetFields()` 只在 `ScriptClass` 内部缓存**，不做跨重编译的全局缓存（同 P2.2 决策点 4.4 的理由）。
- mono API 可用性**已核实**（vendor 头里都有）：`mono_class_get_fields`（`class.h:212`）、`mono_field_get_name/type/flags`（`class.h:238/241/247`）、`mono_field_get_value/set_value`（`object.h:342/336`）、`mono_type_get_type`（`metadata.h:344`）、`mono_type_get_class`（`metadata.h:352`）。
- ⚠️ **两个可能需要补的 include**（实现时按报错补，不要提前加）：
  - `MONO_FIELD_ATTR_*` 定义在 **`mono/metadata/attrdefs.h`** —— 这个头**不在**当前传递包含链里（P1 加的 `class.h` 带不进来），**大概率需要显式加**
  - `MONO_TYPE_*` 定义在 `mono/metadata/blob.h`（该头**已在**传递链里，通常不用加）
- 代码风格遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：`enum class`（§8.1）、`struct` 纯数据（§6.1）、控制语句强制花括号（§5.2）、`while` 里 `continue` 前不加 `else`、`auto` 只用于迭代器和范围 for（§13.9）。

### 1.2 前置条件

| 依赖 | 说明 |
|------|------|
| P1.3 / P1.5 | `ScriptEngine.cpp` 里已 include `<mono/metadata/class.h>`（P1.5 为 `MonoProperty` 加的），且 `ScriptClass` 持有 `MonoClass* m_MonoClass` |
| mono 运行时 | `mono_class_get_fields` 只在 Mono 初始化后可用；`ScriptClass` 的构造本身就要求 mono 已就绪，天然满足 |
| 托管侧 | `Lucky.Entity` 存在（其 `ID` 字段是需要过滤的样本）；`Lucky.Vector3` 存在（3 个 float 的 struct） |
| P2.3 | `ScriptComponent::ScriptAsset`（P2.5 会用到；本 Phase 不依赖） |

### 1.3 本 Phase **不做**的事

- 不改 `ScriptComponent`、不做 `FieldMap`（P2.5）
- 不做 Inspector 控件（P2.6）
- 不做把值写进托管实例的"实例侧"封装（`ScriptInstance::SetFieldValues` 属 P2.6；本 Phase 只提供需要实例参数的**底层读写函数**）
- 不支持 `string` / 数组 / 自定义 struct / `Entity` 引用字段
- 不做 `[HideInInspector]` 特性（见决策点 4.5）

---

## 2. 涉及的文件

### 2.1 新建

| 路径 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Scripting/ScriptFieldValue.h` | `ScriptFieldType` 枚举 + `ScriptFieldValue` 结构体（**纯数据，零 mono 依赖**） |

### 2.2 修改

| 文件 | 改动 |
|------|------|
| `Lucky/Source/Lucky/Scripting/ScriptEngine.h` | `ScriptClass` 加 `ScriptField` 结构、字段列表成员、`GetFields()` / `GetFieldValue()` / `SetFieldValue()` |
| `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp` | 加字段解析辅助函数与三个成员函数实现；补 `attrdefs.h` / `blob.h` include |

### 2.3 不修改

- `Scene/Components/ScriptComponent.h`：本 Phase 不感知（P2.5 才改）
- `Scripting/ScriptGlue.cpp`：不涉及
- 托管 C# 代码：**不需要改**（`Lucky.Entity.ID` 的过滤在 native 侧做）
- premake 脚本：新头文件在同目录，仍**需要重跑一次** `Scripts/Setup-Windows.bat` 让 `.vcxproj` 收录

---

## 3. 现状回顾

### 3.1 `ScriptClass` 当前形态

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

`m_MonoClass` 是私有成员，本 Phase 的 `GetFields()` 作为成员函数可以直接用。

### 3.2 `ScriptEngine.cpp` 现有的 mono include

```cpp
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>        // P1.5 为 MonoProperty 加的
#include <mono/metadata/object.h>
#include <mono/metadata/tabledefs.h>
```

`class.h` 会带进 `metadata.h`（`mono_type_get_type` / `mono_type_get_class` 在 `metadata.h:344/352`）和 `blob.h`（`MONO_TYPE_*`）。但 **`attrdefs.h`（`MONO_FIELD_ATTR_*`）不在链上**。

### 3.3 托管侧的 `Entity`（要过滤的字段来源）

`Lucky-ScriptCore/Source/Lucky/Entity.cs`：

```csharp
namespace Lucky
{
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

        public bool HasComponent<T>() where T : Component, new() { /* ... */ }
        public T GetComponent<T>() where T : Component, new() { /* ... */ }
    }
}
```

三个关键点：

1. `ID` 是 **`public` 实例字段** → 会被 `mono_class_get_fields` 枚举到 → **必须过滤**
2. `ID` 是 `readonly` → 编译后带 `MONO_FIELD_ATTR_INIT_ONLY`，写它没有意义
3. 两个方法不是字段，本 Phase 不关心

### 3.4 `Lucky.Vector3` 的内存布局契约

`ScriptGlue.cpp` 顶部已经有一条编译期契约（P1.5 加的）：

```cpp
    // glm::vec3 与 C# Lucky.Vector3 的内存布局必须一致（3 个连续 float）
    static_assert(sizeof(glm::vec3) == 3 * sizeof(float), "glm::vec3 layout mismatch with C# Lucky.Vector3");
```

`mono_field_get_value` / `mono_field_set_value` 是**按字节块拷贝**的（拷贝 `sizeof(fieldType)` 字节）。所以只要 `Lucky.Vector3` 与 `glm::vec3` 布局一致，就可以直接把 `glm::vec3*` 交给 mono —— 这条 `static_assert` 就是本 Phase 能安全用 `glm::vec3` 承载 `Vector3` 字段的依据。**不要**在 `ScriptFieldValue` 里换成自己的 `float x,y,z` 结构，那会引入一个新的布局契约。

### 3.5 `ScriptEngine.h` 的 mono 前向声明惯例

```cpp
extern "C"
{
    typedef struct _MonoAssembly MonoAssembly;
    typedef struct _MonoClass MonoClass;
    typedef struct _MonoMethod MonoMethod;
    typedef struct _MonoObject MonoObject;
    typedef struct _MonoImage MonoImage;
    typedef struct _MonoDomain MonoDomain;
}
```

本 Phase 需要再加一个 **`typedef struct _MonoClassField MonoClassField;`** —— 这是 `ScriptEngine.h` 里唯一需要的 mono 相关改动，不需要 include 任何 mono 头。

---

## 4. 关键设计决策（多方案对比）

### 4.1 决策点 1：`ScriptFieldValue` 用什么表示

#### 方案 A：扁平结构体 + 类型标记（**推荐 ✅**）

```cpp
struct ScriptFieldValue
{
    ScriptFieldType Type = ScriptFieldType::None;
    float FloatValue = 0.0f;
    int IntValue = 0;
    bool BoolValue = false;
    glm::vec3 Vector3Value = glm::vec3(0.0f);
};
```

- **优点**：
  1. **零模板开销**：`= default` 构造/拷贝天然正确，不需要 `std::visit`、不需要 `get_if`、不需要处理 `bad_variant_access`
  2. **序列化最直**：P2.5 里 `switch (value.Type)` 直接写/读对应字段，能直接映射成 YAML 的 `{Type: Float, FloatValue: 3.0}`，用户手改存档也看得懂
  3. 支持的类型只有 4 种，多存 20 来个字节在这种数据量（每个实体几个字段）下**完全不敏感**
- **缺点**：不是严格类型安全 —— 理论上可以同时填 `FloatValue` 和 `IntValue`。**用"永远先判 `Type` 再取值"这个约定约束住**，并在 `ScriptFieldValue` 的 XML 注释里写明这条约定

#### 方案 B：`std::variant<float, int, bool, glm::vec3>`

- **优点**：真正的类型安全，不可能出现"两个值同时有效"
- **缺点**：
  1. 每个消费点都要 `std::visit` 或 `std::get_if` + 一大坨分支，读起来比 `switch` 绕
  2. 序列化要再写一个 visitor，代码量翻倍
  3. 项目里**目前没有任何 `std::variant` 用法**，引入一种新范式只为 4 个类型，收益不成比例
- **次优**：如果你更看重类型安全，选 B 也行，但要接受 Inspector / 序列化 / 默认值三处都会变啰嗦

#### 方案 C：`std::any`

- **优点**：理论上能塞任何类型
- **缺点**：**取出来要 `any_cast` 且类型不对会抛异常**；序列化无从下手（拿不到类型的可读名字）；属于典型的"用错工具"。**否决。**

#### 方案 D：手写 tagged union（`union { float f; int i; ... }`）

- **优点**：内存最省
- **缺点**：`glm::vec3` 是**非平凡类型**，放进 `union` 必须手动 `placement new` + 手动析构，否则是 UB。为了省 20 字节引入一处手动生命周期管理，**性价比极差**。**否决。**

**结论：方案 A。**

---

### 4.2 决策点 2：怎么判定字段的"类型"

#### 方案 A（混合）：primitive 用 `mono_type_get_type`，`VALUETYPE` 再用类名兜底（**推荐 ✅**）

```cpp
switch (mono_type_get_type(type))
{
    case MONO_TYPE_R4:      return ScriptFieldType::Float;
    case MONO_TYPE_I4:      return ScriptFieldType::Int;
    case MONO_TYPE_BOOLEAN: return ScriptFieldType::Bool;
    case MONO_TYPE_VALUETYPE:
    {
        MonoClass* valueTypeClass = mono_type_get_class(type);
        if (valueTypeClass && IsCoreClass(valueTypeClass, "Vector3"))   // namespace == "Lucky" && name == "Vector3"
        {
            return ScriptFieldType::Vector3;
        }
        return ScriptFieldType::None;
    }
    default:                return ScriptFieldType::None;
}
```

- **优点**：
  1. primitive 走 **mono 原生的类型枚举**，不依赖任何字符串，改不了名、认错不了
  2. struct 类型（`Vector3`）必然都是 `MONO_TYPE_VALUETYPE`，这个层面无法区分，**只能**靠类名 —— 而这里只比一个类，范围极小
  3. 未知类型统一落到 `default → None`，天然支持"跳过不支持的字段"
- **缺点**：`Vector3` 的识别依赖"命名空间 + 类名"这两个字符串，如果以后托管侧把 `Vector3` 挪到别的命名空间，这里要跟着改（**一处**，可接受）

#### 方案 B（纯字符串）：`strcmp(mono_type_get_name(type), "System.Single")` 之类

- **优点**：实现最短，一眼看懂
- **缺点**：
  1. 每个字段都要做一次字符串比较（`mono_type_get_name` 还会拼字符串）
  2. 字符串是**弱契约**：`"System.Single"` 写错一个字母就静默变成"不支持的类型"
  3. 有 `mono_type_get_type` 这种强类型 API 却不用，属于"用字符串做类型分发"的反模式
- **不推荐**（但作为最小实现也能跑）

#### 方案 C：把 `MonoType*` 缓存在静态变量里做指针比较

```cpp
static MonoType* s_FloatType = mono_reflection_type_from_name("System.Single", coreImage);
```

- **优点**：最快（指针相等）
- **缺点**：
  1. 需要初始化时机管理（`coreImage` 就绪后、且每个 AppDomain 一次），跨重编译/热重载时 `MonoType*` 会失效，得记得重建
  2. **收益在"每帧几十次比较"这个量级上完全看不见**（`GetFields()` 只在拖入脚本和进 Play 时调用）
- **不必要。**

**结论：方案 A。** 只在 `VALUETYPE` 这一个分支上用类名比较，把字符串依赖压到最小。

---

### 4.3 决策点 3：`ScriptFieldValue` 放哪个头

#### 方案 A：新建 `Scripting/ScriptFieldValue.h`（**推荐 ✅**）

- **优点**：
  1. **`ScriptComponent.h`（P2.5）能只依赖这个轻头**，不用 include `ScriptEngine.h`
  2. 这个头是纯数据（一个 `enum class` + 一个 `struct` + `<glm/glm.hpp>`），**零 mono 依赖、零场景依赖**，谁都能安全包含
  3. 依赖方向清晰：`Scene/Components → Scripting/ScriptFieldValue.h`（数据），而 `Scripting/ScriptEngine.* → ScriptFieldValue.h`（实现）。**没有任何环**
- **缺点**：多一个头文件

#### 方案 B：直接放进 `ScriptEngine.h`

- **优点**：少一个文件
- **缺点**：`ScriptEngine.h` include 了 `Lucky/Scene/Scene.h` 与 `Lucky/Scene/Entity.h`。`ScriptComponent.h` 一旦包含它，就把**场景层、mono 前向声明、脚本引擎全套**拖进每个 include `Components.h` 的编译单元 —— 包括 `Scene.cpp`、`SceneSerializer` 等。**编译时间与耦合双输，而且极易演变成循环包含。**
- **否决。**

#### 方案 C：放进 `Scene/Components/ScriptComponent.h`

- **优点**：用的人就在这儿
- **缺点**：**层级倒挂** —— 脚本字段的概念属于脚本系统，塞进组件头会让"谁拥有这个概念"变得模糊；而且 P2.6 的 Inspector 也要用它，从组件头里取数据很别扭。**否决。**

**结论：方案 A。**

---

### 4.4 决策点 4：过滤规则的具体实现

#### 方案 A：按 flags 精确过滤（**推荐 ✅**）

```cpp
const uint32_t flags = mono_field_get_flags(field);

// 只要 public
if ((flags & MONO_FIELD_ATTR_FIELD_ACCESS_MASK) != MONO_FIELD_ATTR_PUBLIC)
{
    continue;
}

// 排除 static
if ((flags & MONO_FIELD_ATTR_STATIC) != 0)
{
    continue;
}

// 排除 readonly / initonly（Lucky.Entity.ID 就是这种）
if ((flags & MONO_FIELD_ATTR_INIT_ONLY) != 0)
{
    continue;
}
```

- **优点**：
  1. **`ID` 由 `INIT_ONLY` 一条就挡掉了**，不需要为它写"名字叫 ID 就跳过"这种硬编码
  2. 语义精确：`readonly` 字段在 Inspector 里本来就不该可改
  3. `FUNCTION_ACCESS_MASK` 比较会把 `private` / `protected` / `internal` 全挡掉
- **缺点**：需要 `attrdefs.h`（已在上面的 include 说明里交代）

#### 方案 B：靠名字约定过滤（跳过 `ID`、跳过以 `m_` / `_` 开头的）

- **优点**：不依赖 flags
- **缺点**：**约定是隐式的**，用户写了个 public 字段叫 `ID` 就被悄悄吃掉，莫名其妙。**否决。**

#### 方案 C：额外过滤编译器生成字段（**必做，不是可选**）

```cpp
// C# 的属性会被编译成 "<Speed>k__BackingField"
if (fieldName[0] == '<')
{
    continue;
}
```

- **说明**：这一条**不是方案选项，是必须做的**。C# 里写 `public float Speed { get; set; }`，编译器会生成一个名字形如 `<Speed>k__BackingField` 的**私有**字段 —— 但**不是所有编译器/版本都生成带 `<` 的名字**，且有些自动属性的 backing field 访问级可能不是 private。**用两个判据叠加**：名字里含 `<` → 一定是编译器生成的。这条判据在 mono 上稳定成立。

**结论：方案 A + C 一起用。**（C 是"编译器生成"的必要保险，A 是可见性/只读的必要保险。）

---

### 4.5 决策点 5：`[HideInInspector]` 要不要在本 Phase 做

Unity 用 `[HideInInspector]` 让某个 public 字段不出现在 Inspector 里。

#### 方案 A：本 Phase **不做**，只把字段全暴露（**推荐 ✅**）

- **优点**：
  1. 需要改动**托管侧**（新增一个 `Lucky.HideInInspectorAttribute` 类）、native 侧读取自定义特性、以及**编译后才能生效**的验证链路 —— 这是一整条独立的小工作流
  2. 当前脚本只有 1 个，字段要么有用要么删掉，暂时没有"想藏起来"的真实诉求
  3. 不做它**不阻塞**任何后续步骤：P2.5 / P2.6 都不依赖"能不能隐藏字段"
- **缺点**：用户以后想藏字段时得等一个后续 Phase

#### 方案 B：现在就做

- **优点**：一步到位，对齐 Unity
- **缺点**：托管侧要加新类型；native 侧要用 `mono_custom_attrs_from_field` + `mono_custom_attrs_has_attr`（都在 `reflection.h`，该头可达）取特性并逐个比对特性类；**而且"特性类来自 Core 程序集"还是"用户程序集"要统一**，否则匹配不上会静默失效。工作量明显大于前四步之和。
- **结论**：作为**后续增强**，不塞进 P2.4。

#### 方案 C：先做一个"命名约定"版的隐藏（字段名以 `_` 开头就隐藏）

- **优点**：零托管改动
- **缺点**：**隐式约定**，且与 C# 社区惯例（`_camelCase` 表示 private 字段）撞车 —— 用户会以为 `_speed` 是私有的，结果它既没被隐藏、又出现在 Inspector 里。**否决。**

**结论：方案 A。** 后续要做的实现要点（`reflection.h` 的 `mono_custom_attrs_from_field` / `mono_custom_attrs_has_attr`）已记在 §8 接线点，届时直接接上。

---

### 4.6 决策点 6：字段列表的缓存策略

#### 方案 A：缓存到 `ScriptClass` 内部，首次调用时 lazy 构建（**推荐 ✅**）

```cpp
    private:
        std::vector<ScriptField> m_Fields;
        bool m_FieldsInitialized = false;
```

- **优点**：
  1. `ScriptClass` 的生命周期 = `EntityClasses` 的生命周期 = 一次程序集加载。**程序集重载时会整个重建 `EntityClasses`**（`LoadAssemblyClasses()` 里第一行就是 `s_Data->EntityClasses.clear()`），缓存**自动**跟着失效，不需要任何手动清理
  2. 懒构建：没人用的类不花代价（`EntityClasses` 里可能有几十个类，只有挂上实体的才需要枚举字段）
- **缺点**：如果同一个类被反复 `GetFields()`（比如 Inspector 每帧画），第一次之后就是读缓存，没问题

#### 方案 B：不缓存，每次重新枚举

- **优点**：最简单，绝对不会有失效问题
- **缺点**：Inspector 每帧都会调 `GetFields()`，每次都 `mono_class_get_fields` 遍历 + 每次分配 `std::vector` —— 完全是可避免的浪费。**次优。**

#### 方案 C：缓存在 `ScriptEngine` 的全局 map 里（按类名索引）

- **优点**：和 `EntityClasses` 分开放，职责更单一
- **缺点**：多一张表要维护，且**必须记得在 `LoadAssemblyClasses()` 里 clear** —— 这正是方案 A 自动解决的问题。**不必要。**

**结论：方案 A。**

---

## 5. 实现步骤（按依赖顺序）

每步完成后 `Lucky` 与 `Luck3DApp` 都应能编译通过。

### Step 1：新建 `ScriptFieldValue.h`

**文件**：`Lucky/Source/Lucky/Scripting/ScriptFieldValue.h`

```cpp
#pragma once

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 脚本字段类型：本引擎支持在 Inspector 中编辑的字段类型
    /// None 表示不支持的类型，枚举时会被跳过
    /// </summary>
    enum class ScriptFieldType : uint8_t
    {
        None = 0,
        Float,
        Int,
        Bool,
        Vector3
    };

    /// <summary>
    /// 脚本字段值：一个字段的运行时取值
    /// 使用约定：先读 Type 判断有效载荷，再取对应的值成员；不要依赖"未被选中的成员"
    /// </summary>
    struct ScriptFieldValue
    {
        ScriptFieldType Type = ScriptFieldType::None;
        float FloatValue = 0.0f;
        int IntValue = 0;
        bool BoolValue = false;
        glm::vec3 Vector3Value = glm::vec3(0.0f);
    };
}
```

**要点**：

- **刻意不 include 任何 `Lucky/` 内的头**（除 glm），保持零依赖。`uint8_t` 来自 `<cstdint>`，通常由 PCH 提供；若你的配置报 `uint8_t` 未定义，补 `#include <cstdint>`
- `Vector3Value` 用 **`glm::vec3`** 而不是自定义结构：`ScriptGlue.cpp` 里已有的 `static_assert(sizeof(glm::vec3) == 3 * sizeof(float))` 就是它与 `Lucky.Vector3` 的布局契约，复用它比新造一个类型安全（见 3.4）
- **XML 注释里写清使用约定**（"先读 Type 再取值"）—— 这是方案 A 唯一的缺点，用文档约定兜住

### Step 2：`ScriptEngine.h` 加 mono 前向声明与 `ScriptField`

**文件**：`Lucky/Source/Lucky/Scripting/ScriptEngine.h`

**1) 补一个 mono 前向声明**（在 `extern "C"` 块里）：

```cpp
extern "C"
{
    typedef struct _MonoAssembly MonoAssembly;
    typedef struct _MonoClass MonoClass;
    typedef struct _MonoClassField MonoClassField;
    typedef struct _MonoMethod MonoMethod;
    typedef struct _MonoObject MonoObject;
    typedef struct _MonoImage MonoImage;
    typedef struct _MonoDomain MonoDomain;
}
```

**2) 顶部加 include**：

```cpp
#include "Lucky/Scripting/ScriptFieldValue.h"
```

按规范 §3.3 放在工程内头分组（与 `Lucky/Core/Base.h` 等同组）。

**3) 在 `ScriptClass` 之前定义 `ScriptField`**：

```cpp
    /// <summary>
    /// 脚本字段元信息：字段名、支持的类型、以及 mono 侧字段句柄
    /// 仅供 Scripting 层内部使用；组件层只需要 ScriptFieldValue
    /// </summary>
    struct ScriptField
    {
        std::string Name;
        ScriptFieldType Type = ScriptFieldType::None;
        MonoClassField* Field = nullptr;
    };
```

**4) `ScriptClass` 加三个方法声明**：

```cpp
        /// <summary>
        /// 获取该类的可编辑字段列表（public 实例字段、类型受支持）
        /// 首次调用时枚举并缓存；程序集重新加载时随 EntityClasses 一起重建
        /// </summary>
        const std::vector<ScriptField>& GetFields();

        /// <summary>
        /// 读取指定实例上某个字段的值
        /// </summary>
        /// <param name="instance">托管对象实例</param>
        /// <param name="field">字段元信息（必须来自本类的 GetFields()）</param>
        /// <param name="outValue">输出：字段值</param>
        /// <returns>是否读取成功</returns>
        bool GetFieldValue(MonoObject* instance, const ScriptField& field, ScriptFieldValue& outValue) const;

        /// <summary>
        /// 写入指定实例上某个字段的值
        /// </summary>
        /// <param name="instance">托管对象实例</param>
        /// <param name="field">字段元信息（必须来自本类的 GetFields()）</param>
        /// <param name="value">要写入的值（按 value.Type 决定写入哪个成员）</param>
        /// <returns>是否写入成功</returns>
        bool SetFieldValue(MonoObject* instance, const ScriptField& field, const ScriptFieldValue& value) const;
```

**5) `ScriptClass` 私有区加两个成员**：

```cpp
    private:
        std::string m_ClassNamespace;
        std::string m_ClassName;

        MonoClass* m_MonoClass = nullptr;

        std::vector<ScriptField> m_Fields;
        bool m_FieldsInitialized = false;
```

**要点**：

- `ScriptField` 定义在**命名空间作用域**（不在 `ScriptClass` 内部）：`GetFields()` 返回 `const std::vector<ScriptField>&`，类型需要在类外可见才能被调用方持有引用
- `GetFields()` 是**非 const**（首次调用会改缓存）
- `GetFieldValue` / `SetFieldValue` 不修改 `ScriptClass` 本身 → 标 **`const`**（规范 §13.1：不修改对象的方法标 const）
- `ScriptField` 是 `struct`（纯数据，规范 §6.1），成员 `PascalCase`（规范 §2.1 公有成员）

### Step 3：`ScriptEngine.cpp` 加辅助函数

**文件**：`Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`

**1) 补 include**（按报错补，先试不加 `attrdefs.h`）：

```cpp
#include <mono/metadata/attrdefs.h>     // MONO_FIELD_ATTR_*
```

按规范 §3.3 放在 mono 组内、与 `class.h` 相邻。

**2) 在已有的匿名命名空间里**（`GetExceptionStringProperty` / `LogScriptException` 旁边）加两个辅助函数：

```cpp
        /// <summary>
        /// 判断字段名是否为编译器生成（C# 属性的 backing field 形如 "<Speed>k__BackingField"）
        /// </summary>
        bool IsCompilerGeneratedFieldName(const char* fieldName)
        {
            return fieldName && std::strchr(fieldName, '<') != nullptr;
        }

        /// <summary>
        /// 判断 MonoClass 是否为核心程序集里的指定类型
        /// </summary>
        bool IsCoreClass(const char* nameSpace, const char* className, MonoClass* monoClass, const char* expectedNameSpace, const char* expectedClassName)
        {
            if (!monoClass)
            {
                return false;
            }

            return std::strcmp(nameSpace, expectedNameSpace) == 0 && std::strcmp(className, expectedClassName) == 0;
        }

        /// <summary>
        /// 把 mono 字段类型映射为本引擎支持的 ScriptFieldType；不支持时返回 None
        /// </summary>
        ScriptFieldType ResolveScriptFieldType(MonoType* fieldType)
        {
            switch (mono_type_get_type(fieldType))
            {
                case MONO_TYPE_R4:
                {
                    return ScriptFieldType::Float;
                }
                case MONO_TYPE_I4:
                {
                    return ScriptFieldType::Int;
                }
                case MONO_TYPE_BOOLEAN:
                {
                    return ScriptFieldType::Bool;
                }
                case MONO_TYPE_VALUETYPE:
                {
                    MonoClass* valueTypeClass = mono_type_get_class(fieldType);
                    if (!valueTypeClass)
                    {
                        return ScriptFieldType::None;
                    }

                    const char* nameSpace = mono_class_get_namespace(valueTypeClass);
                    const char* className = mono_class_get_name(valueTypeClass);
                    if (IsCoreClass(nameSpace, className, valueTypeClass, "Lucky", "Vector3"))
                    {
                        return ScriptFieldType::Vector3;
                    }

                    return ScriptFieldType::None;
                }
                default:
                {
                    return ScriptFieldType::None;
                }
            }
        }
```

**要点**：

- `IsCompilerGeneratedFieldName` 只判 `<`，**不要**再叠加 `k__BackingField` 的字符串匹配 —— 那是冗余的，且名字格式是编译器实现细节
- **`IsCoreClass` 的签名刻意传了 `nameSpace` / `className` 进来**：这样它在内部只做比较，不重复调用 mono API。若你觉得参数太多，也可以简化成"就地比较"（把这两行比较直接写在 `ResolveScriptFieldType` 里，去掉这个辅助函数）—— 二者都行，**不要**留一个参数用不上的版本
- `switch` 的每个 `case` 都用花括号包住（规范 §5.2 的强制花括号延伸到 `switch` 的 case 块；项目里 `LoadAssemblyClasses` 之外的 mono 相关代码也这么写）
- `MONO_TYPE_BOOLEAN` / `MONO_TYPE_R4` 来自 `blob.h`（已在传递链里）。若报未定义，补 `#include <mono/metadata/blob.h>`

### Step 4：实现三个成员函数

**文件**：`Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`

加在 `ScriptClass::InvokeMethod` 实现**之后**（同一组成员函数放在一起）：

```cpp
    const std::vector<ScriptField>& ScriptClass::GetFields()
    {
        if (m_FieldsInitialized)
        {
            return m_Fields;
        }
        m_FieldsInitialized = true;
        m_Fields.clear();

        if (!m_MonoClass)
        {
            return m_Fields;
        }

        void* iterator = nullptr;
        MonoClassField* field = nullptr;
        while ((field = mono_class_get_fields(m_MonoClass, &iterator)) != nullptr)
        {
            const uint32_t flags = mono_field_get_flags(field);

            if ((flags & MONO_FIELD_ATTR_FIELD_ACCESS_MASK) != MONO_FIELD_ATTR_PUBLIC)
            {
                continue;
            }
            if ((flags & MONO_FIELD_ATTR_STATIC) != 0)
            {
                continue;
            }
            if ((flags & MONO_FIELD_ATTR_INIT_ONLY) != 0)
            {
                continue;
            }

            const char* fieldName = mono_field_get_name(field);
            if (IsCompilerGeneratedFieldName(fieldName))
            {
                continue;
            }

            const ScriptFieldType fieldType = ResolveScriptFieldType(mono_field_get_type(field));
            if (fieldType == ScriptFieldType::None)
            {
                LF_CORE_WARN("ScriptClass::GetFields - Unsupported type for field '{0}.{1}', ignored", m_ClassName, fieldName);
                continue;
            }

            ScriptField scriptField;
            scriptField.Name = fieldName;
            scriptField.Type = fieldType;
            scriptField.Field = field;
            m_Fields.push_back(scriptField);
        }

        return m_Fields;
    }

    bool ScriptClass::GetFieldValue(MonoObject* instance, const ScriptField& field, ScriptFieldValue& outValue) const
    {
        if (!instance || !field.Field)
        {
            return false;
        }

        switch (field.Type)
        {
            case ScriptFieldType::Float:
            {
                float rawValue = 0.0f;
                mono_field_get_value(instance, field.Field, &rawValue);
                outValue.Type = ScriptFieldType::Float;
                outValue.FloatValue = rawValue;
                return true;
            }
            case ScriptFieldType::Int:
            {
                int rawValue = 0;
                mono_field_get_value(instance, field.Field, &rawValue);
                outValue.Type = ScriptFieldType::Int;
                outValue.IntValue = rawValue;
                return true;
            }
            case ScriptFieldType::Bool:
            {
                // C# bool 在托管侧是 1 字节，用 uint8_t 中转避免与 native bool 的宽度假设混用
                uint8_t rawValue = 0;
                mono_field_get_value(instance, field.Field, &rawValue);
                outValue.Type = ScriptFieldType::Bool;
                outValue.BoolValue = (rawValue != 0);
                return true;
            }
            case ScriptFieldType::Vector3:
            {
                glm::vec3 rawValue = glm::vec3(0.0f);
                mono_field_get_value(instance, field.Field, &rawValue);
                outValue.Type = ScriptFieldType::Vector3;
                outValue.Vector3Value = rawValue;
                return true;
            }
            default:
            {
                return false;
            }
        }
    }

    bool ScriptClass::SetFieldValue(MonoObject* instance, const ScriptField& field, const ScriptFieldValue& value) const
    {
        if (!instance || !field.Field)
        {
            return false;
        }

        switch (value.Type)
        {
            case ScriptFieldType::Float:
            {
                float rawValue = value.FloatValue;
                mono_field_set_value(instance, field.Field, &rawValue);
                return true;
            }
            case ScriptFieldType::Int:
            {
                int rawValue = value.IntValue;
                mono_field_set_value(instance, field.Field, &rawValue);
                return true;
            }
            case ScriptFieldType::Bool:
            {
                uint8_t rawValue = value.BoolValue ? 1 : 0;
                mono_field_set_value(instance, field.Field, &rawValue);
                return true;
            }
            case ScriptFieldType::Vector3:
            {
                glm::vec3 rawValue = value.Vector3Value;
                mono_field_set_value(instance, field.Field, &rawValue);
                return true;
            }
            default:
            {
                return false;
            }
        }
    }
```

**要点**：

- **`SetFieldValue` 的 switch 判的是 `value.Type`，不是 `field.Type`** —— 这正是决策点 4.1 方案 A 的约定所在。**写入时以"值携带的类型"为准**，因为调用方（P2.6）是从 `FieldMap` 里取出 `ScriptFieldValue` 来灌的，它的 `Type` 就是权威。
  - 如果想更严格，可以在写入前判 `field.Type != value.Type` 并拒写 + 报错。**推荐加上这个校验**（属于"花两行换一个静默类型错配的保险"），本 Phase 先按上面写，P2.6 接入时按实测决定
- **`bool` 用 `uint8_t` 中转而不是 `bool`**：C# 的 `bool` 在 mono 里是 1 字节（`MonoBoolean`），而 C++ 的 `bool` 大小是实现定义的（MSVC 下是 1）。用 `uint8_t` 中转可以避免"依赖 sizeof(bool) == 1"这个隐含假设。虽然 MSVC 下两者等价，但显式写出来更好读
- **`Vector3` 直接传 `glm::vec3*`**：依据是 3.4 里那条 `static_assert`
- **`mono_field_get_value` 的 `value` 参数是 `void*`**，写进去的数据必须与字段实际大小匹配 —— 这也是为什么类型不支持的字段**必须**在 `GetFields()` 阶段就被过滤掉（否则这里会写越界）
- 每个 `case` 声明一个局部 `rawValue` 再传地址：不要在 `SetFieldValue` 里对 `value.FloatValue` 直接取地址（虽然它是 const 成员的地址，`mono_field_set_value` 只是读它，能工作，但语义上"写入的值"和"源值"混在一起容易误读）

### Step 5：编译验证

```bash
Scripts/Setup-Windows.bat
```

- 确认 `Lucky.vcxproj` 里出现 `ScriptFieldValue.h`
- Debug 编译 `Lucky` → 通过
- Debug 编译 `Luck3DApp` → 通过
- **若报 `MONO_FIELD_ATTR_PUBLIC` 未定义**：Step 3-1 的 `#include <mono/metadata/attrdefs.h>` 没加
- **若报 `MONO_TYPE_R4` / `MONO_TYPE_BOOLEAN` 未定义**：补 `#include <mono/metadata/blob.h>`
- **若报 `std::strchr` / `std::strcmp` 未定义**：补 `#include <cstring>`（PCH 里通常已有）

### Step 6（一次性自证，验收后删除）：给示例脚本加字段并打印枚举结果

**前置**：给 `Luck3DApp/Project/Assets/Scripts/PlayerController.cs` **临时**加两个字段（验证完**可以保留**，因为 P2.5/P2.6 正好需要它当测试样本）：

```csharp
    public class PlayerController : Entity
    {
        public float Speed = 3.0f;
        public bool LogPosition = false;

        void Awake() { /* ... */ }
        void Update(float deltaTime) { /* ... */ }
        void OnDestroy() { /* ... */ }
    }
```

**在 `ScriptEngine::Init()` 末尾、`LF_CORE_INFO` 之前**临时插入：

```cpp
    Ref<ScriptClass> debugClass = ResolveScriptClass("PlayerController");
    if (debugClass)
    {
        for (const ScriptField& field : debugClass->GetFields())
        {
            LF_CORE_INFO("ScriptEngine: field test - {0} (type {1})", field.Name, static_cast<int>(field.Type));
        }
    }
```

**预期**（顺序不保证，`mono_class_get_fields` 的返回顺序不承诺）：

```
ScriptEngine: field test - Speed (type 1)
ScriptEngine: field test - LogPosition (type 3)
```

**必须验证的负向结果**：

- **没有** `ID`（被 `MONO_FIELD_ATTR_INIT_ONLY` 挡掉）
- **没有** `<Speed>k__BackingField`（被 `<` 判据挡掉 —— 前提是你**没有**改成自动属性；若想验这条，把 `Speed` 临时改成 `public float Speed { get; set; } = 3.0f;` 再看一次）

**⚠️ 验收完成后必须删除 `Init` 里这段临时代码**（`.cs` 里的两个字段**可以保留**）。

---

## 6. 疑点问答

### 6.1 `mono_class_get_fields` 的遍历方式为什么这么别扭（`void* iterator`）？

这是 mono 的固定 API 形态：

```cpp
void* iterator = nullptr;
MonoClassField* field = nullptr;
while ((field = mono_class_get_fields(m_MonoClass, &iterator)) != nullptr)
{
    // ...
}
```

`iterator` 由 mono 内部维护，调用方**不能**自己改它。**`iterator` 必须在循环外初始化一次**（`= nullptr`），在循环内重复传入同一个变量。写成 `while ((field = mono_class_get_fields(m_MonoClass, &iter)))` 用 `auto` 是不行的 —— mono 需要 `void**`。

### 6.2 为什么 `GetFields()` 要缓存，`ResolveScriptClass`（P2.2）却不缓存？

调用频次不同：

- `ResolveScriptClass`：只在"拖入脚本"和"进 Play"时调用，一次遍历几十个类不痛
- `GetFields()`：**Inspector 每帧都会调**（要画控件），每次都重新 `mono_class_get_fields` + 分配 `vector` 是纯浪费

而缓存的失效问题在 `GetFields()` 这里是**免费解决**的：`ScriptClass` 对象活在 `EntityClasses` 里，程序集重载时 `LoadAssemblyClasses()` 第一行 `EntityClasses.clear()` 会把所有 `ScriptClass` **连同缓存一起丢掉**。所以"缓存 + 自动失效"两者都拿到了。

### 6.3 `ID` 字段为什么用 `INIT_ONLY` 就能挡住，而不必判名字？

`readonly` 字段在 IL 里带 `InitOnly` 标志，`mono_field_get_flags` 会如实返回。`Lucky.Entity.ID` 声明为 `public readonly ulong ID;`，所以 `MONO_FIELD_ATTR_INIT_ONLY` 必然命中。

好处是**规则是通用的**：以后用户自己写 `public readonly int SomeId;` 也一样会被挡掉 —— 这比"跳过名字叫 ID 的字段"这种硬编码正确得多。

### 6.4 如果用户写了 `public string Name = "abc";` 会怎样？

`ResolveScriptFieldType` 走到 `MONO_TYPE_STRING` → `default` → `None` → 在 `GetFields()` 里被跳过，并打一条：

```
ScriptClass::GetFields - 字段 'PlayerController.Name' 的类型暂不支持，已忽略
```

**不会崩、不会出现在 Inspector 里。** 这条 WARN 是必要的：否则用户会奇怪"我明明写了 public 字段怎么不显示"。

### 6.5 `GetFieldValue` / `SetFieldValue` 传进来的 `field` 必须是本类的字段吗？

**是的，这是前置契约**（XML 注释里写了"必须来自本类的 `GetFields()`"）。如果传了一个别的类的 `MonoClassField*`，`mono_field_get_value` 会按错误的字段布局去读实例内存 —— 轻则读到垃圾，重则越界。

**不打算在函数里校验**（校验需要比较 `mono_field_get_parent(field) == m_MonoClass`，`mono_field_get_parent` 在 `class.h` 里有）—— 调用方（P2.6）本来就是从 `GetFields()` 拿的字段再用的，属于内部契约。若你希望更稳，可以加一行 `LF_CORE_ASSERT`。

### 6.6 字段的声明顺序在 Inspector 里能保证吗？

**不能，也不该依赖**。`mono_class_get_fields` 走的是元数据表的顺序，**mono 不承诺它等于源码声明顺序**。

如果需要"在源码里控制 Inspector 顺序"，唯一可靠的办法是加一个显式的排序特性（Unity 也是靠自定义 Inspector 才能调序）。**本 Phase 不做，也不建议在后续阶段做** —— 字段少的时候顺序无所谓，字段多了本来就该用分组/自定义 Inspector。

### 6.7 这一步能不能不做，直接在 P2.5 里连字段一起搞？

技术上可以，但**不推荐**：

- 字段反射是**最容易踩 mono API 细节**的一步（flags 常量、遍历方式、类型枚举、bool 宽度、Vector3 布局）
- 把它独立出来，可以**先用日志把"枚举结果对不对"验干净**（Step 6 的自证），再往上叠"存进组件 + 序列化 + 画控件"
- 混在一起做的话，一旦 Inspector 显示不对，你分不清是"反射错了"还是"存储错了"还是"控件错了"

---

## 7. 验收标准

1. **编译通过**：`Lucky` 与 `Luck3DApp` 的 Debug / Release / Dist 三个 configuration 全通过
2. **枚举正确**：`PlayerController` 的 `GetFields()` 返回 `Speed`(Float) 与 `LogPosition`(Bool)；**不含** `ID`、**不含**编译器生成字段
3. **顺序不承诺**：第 2 条的字段顺序不要求与源码一致（验证代码不要假设顺序）
4. **不支持类型被跳过**：临时给脚本加 `public string Title = "x";` → 该字段被跳过，日志里有 `类型暂不支持`
5. **读值正确**：对 `Speed` 调 `GetFieldValue`，返回 `Type == Float` 且 `FloatValue == 3.0f`
6. **写值生效**：对 `Speed` 调 `SetFieldValue(FloatValue = 7.0f)`，再 `GetFieldValue` 读回是 `7.0f`（**这一条必须在托管对象上实测**，是整套字段系统能不能用的关键）
7. **缓存生效且无副作用**：连续调 `GetFields()` 10 次，返回的 `vector` 地址不变、`size()` 不变、内容一致
8. **不破坏既有行为**：Play / Stop / 脚本 `Awake` / `Update` / `OnDestroy` 全部与改动前一致
9. **临时代码已删除**：Step 6 在 `Init` 里的自证代码确认已移除
10. **代码规范**：通过人工 checklist —— `enum class`（§8.1）；纯数据用 `struct`（§6.1）；`switch` 各 case 带花括号（§5.2）；不修改对象的方法标 `const`（§13.1）；`while` / `for` 不带 `else`；公有接口有 `/// <summary>` 中文注释（§4.1）；无"为对齐而对齐"的空格（§5.4）；无引用外部文档的注释、无"P2.5 会用到"这类阶段性注释

> ⚠️ **第 6 条是这一整步的"生死线"**：`mono_field_set_value` 写不进去（或写进去类型不对）的话，P2.6 的"Inspector 改值 → 脚本读到"整条链就是空的。**必须在托管对象上真读真写一次**，不能只看编译过。

---

## 8. 对下一 Phase 的接线点

| 位置 | 后续 Phase | 打开方式 |
|------|-----------|---------|
| `ScriptFieldValue`（`ScriptFieldValue.h`） | **P2.5** | 直接作为 `ScriptComponent::FieldMap` 的 value 类型；组件头只依赖这个轻头，不碰 `ScriptEngine.h` |
| `ScriptClass::GetFields()` | **P2.5 / P2.6** | P2.5 用它在"首次挂上脚本"时遍历字段、读默认值填 `FieldMap`；P2.6 用它画控件 |
| `ScriptClass::GetFieldValue` | **P2.5** | 传入一个**临时实例**（`Instantiate()` 出来的）即可读出字段的初值 |
| `ScriptClass::SetFieldValue` | **P2.6** | 在 `ScriptInstance::InvokeAwake()` **之前**把所有 `FieldMap` 值灌进去 |
| `ScriptFieldType` 枚举 | 后续扩展 | 加 `String` / `Entity` / 枚举 等类型时，只需：① 枚举加值 ② `ResolveScriptFieldType` 加分支 ③ `GetFieldValue`/`SetFieldValue` 加 case ④ P2.5 序列化加分支 ⑤ P2.6 控件加分支 |
| `[HideInInspector]` | 后续增强 | 托管侧加 `Lucky.HideInInspectorAttribute`；native 侧用 `reflection.h` 的 `mono_custom_attrs_from_field` + `mono_custom_attrs_has_attr` 在 `GetFields()` 里判一次即可 |

---

## 9. 变更清单速览

- **新增文件（1 个）**
  - `Lucky/Source/Lucky/Scripting/ScriptFieldValue.h`
- **修改文件（2 个）**
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.h`：mono 前向声明加 `MonoClassField`；include `ScriptFieldValue.h`；新增 `ScriptField` 结构；`ScriptClass` 加 `GetFields` / `GetFieldValue` / `SetFieldValue` 与两个私有成员
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`：补 `attrdefs.h`（可能还有 `blob.h` / `cstring`）；匿名命名空间加三个辅助函数；实现三个成员函数
- **删除**：无
- **不改动**：`Scene/Components/ScriptComponent.h`、`ScriptGlue.cpp`、托管 C# 代码、premake 脚本（新头文件在同目录，但**仍需重跑一次 premake**）
- **测试样本会改动**：`Luck3DApp/Project/Assets/Scripts/PlayerController.cs` 临时加 `Speed` / `LogPosition` 两个字段（**建议保留**，P2.5/P2.6 需要）

---

## 10. 参考

- 编码规范 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：§2.1 命名、§3.3 include 顺序、§4.1 XML 注释、§5.2 强制花括号、§5.4 对齐规则、§6.1 class/struct、§8.1 enum class、§13.1 const 正确性、§13.9 auto 使用规范
- 前置详设 [Phase2.3_ScriptComponent_AssetRef.md](Phase2.3_ScriptComponent_AssetRef.md)（`ScriptComponent` 改造；本 Phase 不依赖但顺序上在其后）
- 布局契约 [Phase1.5_ScriptGlue.md](Phase1.5_ScriptGlue.md) 决策点 9（`glm::vec3` 与 C# `Lucky.Vector3` 的内存布局对齐）
- 托管类型 [Phase1.2_ScriptCore_Assembly.md](Phase1.2_ScriptCore_Assembly.md)（`Lucky.Vector3` / `Lucky.Entity` 的定义）
- 脚本系统路线图 [ScriptSystem_Roadmap.md](ScriptSystem_Roadmap.md) 第 4 节 Phase 2「`ScriptEngine` 缓存 `ScriptClass` 元信息（字段名 / 类型 / 默认值）」
- mono 字段 API：`mono_class_get_fields` / `mono_field_get_name` / `mono_field_get_type` / `mono_field_get_flags`（`mono/metadata/class.h`）、`mono_field_get_value` / `mono_field_set_value`（`mono/metadata/object.h`）、`mono_type_get_type` / `mono_type_get_class`（`mono/metadata/metadata.h`）
