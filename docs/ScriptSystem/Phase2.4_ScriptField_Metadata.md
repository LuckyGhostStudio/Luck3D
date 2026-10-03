# Phase 2.4：脚本字段元信息（反射）

## 1. 概述

让 `ScriptClass` 能**枚举自己的 public 实例字段**，并提供一个通用的"字段值"容器。这是 P2.5（把值存进组件）、P2.6（画控件 + 灌值）共同的底座。

产出三件东西：

1. **`ScriptFieldType` + 类型表**（放新头 `Scripting/ScriptFieldType.h`）—— 类型集合的**唯一来源**，纯数据、零依赖
2. **`ScriptFieldValue`**（放新头 `Scripting/ScriptFieldValue.h`）—— 字段值的统一容器，承载全部受支持类型
3. **`ScriptClass::GetFields()`**：懒构建并缓存字段列表（**含默认值**）；配套 `GetFieldValue` / `SetFieldValue` 读写任意实例的字段

**本 Phase 不产出任何 UI，也不改组件。** 目标是把"脚本里有哪些字段、每个字段当前是什么值"变成可查询的数据。

### 1.1 关键约束

- **类型集合收敛到一张表。** 本引擎支持的字段类型由 `ScriptFieldType` 枚举 + `s_ScriptFieldTypeInfos[]` 表**唯一**描述。**任何"关于某个类型的事实"（序列化名、托管全名、控件种类）都必须从表里取**，不允许在别处再写一份类型清单。理由与取舍见决策点 4.1。
- **`ScriptFieldType.h` 必须零依赖**（除 `<cstdint>` / `<cstring>`）。**绝对不能把 `ImGuiDataType` 放进去** —— 那会让 `ScriptEngine.cpp` 这个引擎侧编译单元被 imgui 污染。控件种类用零依赖的 `ScriptFieldWidgetKind` 表示，由 UI 层（P2.6）再映射一次。详见决策点 4.4。
- **`ScriptFieldValue.h` 零 mono 依赖、零场景依赖。** `ScriptComponent`（P2.5）要把它用作 `FieldMap` 的 value 类型；如果它定义在 `ScriptEngine.h` 里，`ScriptComponent.h` 就得 include 整个 `ScriptEngine.h`（那个头又 include 了 `Scene.h`），把脚本运行时、场景、mono 前向声明全拖进组件层。**所以它单独一个头。**
- **必须过滤四类字段**，否则 Inspector 上会冒出幽灵项：
  - 非 public（`mono_field_get_flags` 的访问位不是 `MONO_FIELD_ATTR_PUBLIC`）
  - static（`MONO_FIELD_ATTR_STATIC`）
  - readonly（`MONO_FIELD_ATTR_INIT_ONLY`）—— 基类 `Lucky.Entity.ID` 就是这种
  - **编译器生成**（C# 的 `public float Speed { get; set; }` 会生成 `<Speed>k__BackingField`）
- **类型不在表里、且不是 `Lucky.Entity` 的派生类，一律跳过并打一条 WARN**，不静默、不崩。数组、枚举、自定义 struct/class 都会走到这条路径（它们的托管全名匹配不上表）。`Lucky.Entity` 派生类字段（脚本互引，如 `public PlayerController Other;`）按 `Entity` 引用处理，见决策点 4.10。
- **`GetFields()` 只在 `ScriptClass` 内部缓存**，不做跨重编译的全局缓存（同 P2.2 决策点 4.4 的理由）。
- mono API 可用性**已逐一核实**（vendor 头里都有，行号见 §10）：`mono_class_get_fields`、`mono_field_get_name/type/flags`、`mono_class_get_field_from_name`、`mono_class_from_mono_type`、`mono_class_get_parent`、`mono_class_is_subclass_of`、`mono_type_get_name`、`mono_type_get_type/class`、`mono_field_get_value/set_value`、`mono_object_new`、`mono_object_get_class`、`mono_string_new/to_utf8`、`mono_domain_get`、`mono_free`。
- ⚠️ **两个可能需要补的 include**（实现时按报错补，不要提前加）：
  - `MONO_FIELD_ATTR_*` 定义在 **`mono/metadata/attrdefs.h`** —— 这个头**不在**当前传递包含链里（P1 加的 `class.h` 带不进来），**大概率需要显式加**
  - `mono_domain_get` 在 `appdomain.h`（该头**已在**传递链里，通常不用加）
- 代码风格遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：`enum class`（§8.1）、`struct` 纯数据（§6.1）、控制语句强制花括号（§5.2）、`while` 里 `continue` 前不加 `else`、`auto` 只用于迭代器和范围 for（§13.9）。**日志内容必须全英文**（§9.2）。

### 1.2 前置条件

| 依赖 | 说明 |
|------|------|
| P1.3 / P1.5 | `ScriptEngine.cpp` 里已 include `<mono/metadata/class.h>`（P1.5 为 `MonoProperty` 加的），且 `ScriptClass` 持有 `MonoClass* m_MonoClass` |
| mono 运行时 | `mono_class_get_fields` 只在 Mono 初始化后可用；`ScriptClass` 的构造本身就要求 mono 已就绪，天然满足 |
| 托管侧 | `Lucky.Entity` / `Lucky.Vector3` 存在（见 §3.6 的现状与缺口） |
| P2.3 | `ScriptComponent::ScriptAsset`（P2.5 会用到；本 Phase 不依赖） |
| `AssetManager` | `GetAsset<T>(AssetHandle)` 已对 `Material` / `Mesh` / `Texture2D` / `Script` 做了显式实例化（P2.1 补的 `Script` 那次） |

### 1.3 本 Phase **不做**的事

- 不改 `ScriptComponent`、不做 `FieldMap`（P2.5）
- 不做 Inspector 控件（P2.6）
- 不做把值写进托管实例的"实例侧"封装（`ScriptInstance::SetFieldValues` 属 P2.6；本 Phase 只提供需要实例参数的**底层读写函数**）
- **不支持数组**（`float[]` / `List<T>`）。类型表里不登记数组，且托管全名带 `[]` 后缀天然匹配不上，自动跳过
- **不支持枚举**（`enum`）—— 它要额外反射成员名字列表 + 专用下拉控件 + 存档格式决策，成本远大于当前收益
- **不支持自定义 `[Serializable]` 类 / struct**（嵌套结构）—— 需要递归容器 + 深度上限，属独立设计
- **不支持 `Dictionary`**
- **不支持 `System.Char`**（2 字节 UTF-16 字符，使用频率极低；需要时按决策点 4.1 在表里加行即可）
- 不做 `[HideInInspector]` 特性（见决策点 4.7）

> 以上四项属于"明确推迟"，`ScriptFieldType` 枚举里**不预留**它们的值 —— 需要时按决策点 4.1 的方式在表里加行。

---

## 2. 涉及的文件

### 2.1 新建

| 路径 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Scripting/ScriptFieldType.h` | `ScriptFieldType` / `ScriptFieldWidgetKind` 枚举 + `ScriptFieldTypeInfo` 表 + 查表函数（**零依赖，header-only**） |
| `Lucky/Source/Lucky/Scripting/ScriptFieldValue.h` | `ScriptFieldScalar`（variant）+ `ScriptFieldValue`（**零 mono / 零场景依赖**） |

### 2.2 修改

| 文件 | 改动 |
|------|------|
| `Lucky/Source/Lucky/Scripting/ScriptEngine.h` | mono 前向声明加 `MonoClassField`；include 两个新头与 `Asset/AssetHandle.h`；新增 `ScriptField` 结构；`ScriptClass` 加 `GetFields` / `GetFieldValue` / `SetFieldValue` 与两个私有成员；`ScriptEngine` 加 `ResolveAssetByFieldType` 公开静态声明 |
| `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp` | 补 `attrdefs.h`（可能还有 `cstring`）与资产类型的 include；匿名命名空间加辅助函数（`ResolveScriptFieldType` / `IsEntityDerivedFieldType` / `TryGetReferenceHandle` / `SetReferenceHandle` / `ResolveAssetByFieldType` / 标量模板两个）；实现三个成员函数 |

### 2.3 不修改

- `Scene/Components/ScriptComponent.h`：本 Phase 不感知（P2.5 才改）
- `Scripting/ScriptGlue.cpp`：不涉及
- 托管 C# 代码：**本 Phase 的机制不需要改**（`Entity.ID` 的过滤在 native 侧做）。但 B 组类型（§3.6）需要 ScriptCore 先补齐对应类型才**能实际被使用**
- premake 脚本：新头文件在同目录，但仍**需要重跑一次** `Scripts/Setup-Windows.bat` 让 `.vcxproj` 收录

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

两个和本 Phase 相关的点：

- `m_MonoClass` 是私有成员，本 Phase 的 `GetFields()` 作为成员函数可以直接用
- **`Instantiate()` 可用**：实现是 `ScriptEngine::InstantiateClass(m_MonoClass)`，创建托管对象并调用其构造函数。本 Phase 靠它造临时实例来读默认值（见决策点 4.6）

### 3.2 `ScriptEngine.cpp` 现有的 mono include

```cpp
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>        // P1.5 为 MonoProperty 加的
#include <mono/metadata/object.h>
#include <mono/metadata/tabledefs.h>
```

`class.h` 会带进 `metadata.h`（`mono_type_get_type` / `mono_type_get_class` 在 `metadata.h:344/352`）、`appdomain.h`（`mono_domain_get`）和 `blob.h`（`MONO_TYPE_*`）。但 **`attrdefs.h`（`MONO_FIELD_ATTR_*`）不在链上**。

### 3.3 托管侧的 `Entity`（要过滤的字段来源，也是 `Entity` 字段的读写样本）

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

1. `ID` 声明在基类 `Lucky.Entity` 上，而 `mono_class_get_fields` **只枚举本类声明的字段、不含父类**（mono 源码实证，见决策点 4.9）→ 只枚举本类时它根本不会出现；但本 Phase 会沿继承链枚举（决策点 4.9），那时 `ID` 会重新冒出来，**必须过滤**
2. `ID` 是 `readonly` → 编译后带 `MONO_FIELD_ATTR_INIT_ONLY` → 正好被过滤规则挡掉；写它也没有意义
3. **`ID` 同时是 `Entity` 类型字段的取值入口** —— native 侧要把一个托管 `Entity` 对象化成 `UUID`，唯一办法就是读它的 `ID`（见 Step 5）

### 3.4 `Lucky.Vector3` 的内存布局契约

`ScriptGlue.cpp` 顶部已经有一条编译期契约（P1.5 加的）：

```cpp
    // glm::vec3 与 C# Lucky.Vector3 的内存布局必须一致（3 个连续 float）
    static_assert(sizeof(glm::vec3) == 3 * sizeof(float), "glm::vec3 layout mismatch with C# Lucky.Vector3");
```

`mono_field_get_value` / `mono_field_set_value` 是**按字节块拷贝**的（拷贝 `sizeof(fieldType)` 字节）。所以只要托管结构体与 glm 对应类型布局一致，就可以直接把 `glm::vec3*` 交给 mono —— 这条 `static_assert` 就是本 Phase 能安全用 glm 类型承载 `Vector2` / `Vector3` / `Vector4` / `Quaternion` / `Color` 字段的依据。**不要**在 `ScriptFieldValue` 里换成自己的 `float x,y,z` 结构，那会引入一套新的布局契约。

**B 组类型（§3.6）补齐托管结构体时，必须照这条契约写**（成员顺序、类型、无 padding）。`Quaternion` 与 `Color` 也按"4 个连续 float"对齐到 `glm::quat` / `glm::vec4`。

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

本 Phase 需要再加一个 **`typedef struct _MonoClassField MonoClassField;`**（`ScriptField` 要持有它）。这是 `ScriptEngine.h` 里唯一需要的 mono 相关改动，不需要 include 任何 mono 头。

### 3.6 ★ 托管侧类型现状与缺口

`Lucky-ScriptCore/Source/Lucky/` 当前只有 5 个文件：

| 文件 | 提供 |
|------|------|
| `Entity.cs` | `Lucky.Entity` |
| `Component.cs` | `Lucky.Component` / `Lucky.TransformComponent` |
| `Vector3.cs` | `Lucky.Vector3` |
| `Debug.cs` | `Lucky.Debug` |
| `InternalCalls.cs` | `Lucky.InternalCalls` |

**这是本 Phase 最实际的约束**：类型表里登记一个类型，只代表"引擎认识它"；要让用户脚本真的能写出 `public Material Mat;` 这样的字段，**托管侧必须存在 `Lucky.Material` 这个类**，否则反射时匹配不上，那个类型永远不会出现。

所以类型清单分两组：

**A 组 —— 托管类型现成（可立即验收）**

| 来源 | 类型 |
|------|------|
| .NET 内置（反射直接能拿到） | `bool`、各宽度整数、`float`、`double`、`string` |
| 已有托管类型 | `Lucky.Vector3`、`Lucky.Entity` |

**B 组 —— 需要先补托管类型（表里登记，但没有托管类就不会实际出现）**

| 需新增的托管类型 | 形态建议 |
|------------------|---------|
| `Lucky.Vector2` / `Lucky.Vector4` | 值类型 `struct`，2 / 4 个连续 `float`；照 `Vector3.cs` 写 |
| `Lucky.Quaternion` | 值类型 `struct`，4 个连续 `float` |
| `Lucky.Color` | 值类型 `struct`，4 个连续 `float`（r/g/b/a） |
| `Lucky.Material` / `Lucky.Mesh` / `Lucky.Texture2D` / `Lucky.Script` | 引用类型 `class`，**统一暴露一个 `ulong Handle` 字段**（可见性 `public` 或 `internal` 都可，native 侧按名字取） |

**B 组的前置形态约定（`Lucky.Material` 示例）**：

```csharp
namespace Lucky
{
    public class Material
    {
        internal ulong Handle;

        internal Material(ulong handle)
        {
            Handle = handle;
        }
    }
}
```

native 侧按字段名 `"Handle"` 读写这个句柄（`Entity` 是唯一例外，它用 `"ID"`）。**这条约定必须写进托管侧的类型文件注释里**，否则将来有人改字段名会静默破坏字段系统。

> ⚠️ **B 组的取值范围**：本 Phase 的机制（表、容器、读写框架）对 A / B 两组一视同仁；**A 组必须实测验收，B 组在托管类型补齐后才能实际验证**。补齐托管类型属独立小任务，见 Step 0。

---

## 4. 关键设计决策（多方案对比）

### 4.1 决策点 1：类型集合怎么管理 —— 建表 vs 不建表

#### 背景：同一个"类型事实"会被四个地方消费

`ScriptFieldType::Float` 这一个事实，需要被以下四处知道：

1. **反射**：托管全名 `System.Single` → `ScriptFieldType::Float`
2. **序列化**：`ScriptFieldType::Float` → 字符串 `"Float"`（写盘），反过来读回
3. **Inspector**：`ScriptFieldType::Float` → 用哪个控件

#### 方案 A：一张 `constexpr` 表 + 查表/遍历（**推荐 ✅**）

```cpp
static constexpr ScriptFieldTypeInfo s_ScriptFieldTypeInfos[] =
{
    { ScriptFieldType::Float,   "Float",   "System.Single",   ScriptFieldWidgetKind::Float  },
    { ScriptFieldType::Vector3, "Vector3", "Lucky.Vector3",   ScriptFieldWidgetKind::Float3 },
    // ...
};
```

四个消费点全部变成"查表 / 遍历"：反射遍历表建映射、序列化查 `Name`、反序列化遍历找 `Name`、Inspector 查 `Widget`。

- **优点**：
  1. **加一种类型 = 表里加一行**，四个消费点全部自动生效，**不可能漏**
  2. **表可以被遍历** —— "每种类型都存盘再读回"这种测试可以写成一个循环；将来 Inspector 想列"支持哪些类型"也是一个循环
  3. 类型数量会长期增长（本 Phase 就 22 种），手工清单的成本是**乘性**的
- **缺点**：多一层间接（读代码时要跳到表那边看）

#### 方案 B：不建表，四个 `switch` 放在同一个文件里

- **优点**：没有间接层，调试时一眼看到；代码更少
- **缺点**：
  1. 加一种类型要改 **4 处**（虽然在同一文件）
  2. **漏改不会编译报错** —— 漏了"托管名 → 类型"会让该类型字段**静默消失**；漏了"字符串 → 类型"会让存盘的类型**重启后变 `None`**。后者我们在 P2.1 已经真实踩过一次（`StringToAssetType` 漏了 `Script` 分支）
  3. 无法遍历

**结论：方案 A。** 核心理由不是"省几行代码"，而是**消掉一类静默出错的可能**：规则集中在一处时，"加类型"这个动作不可能只完成一半。

> 补充说明：表**解决不了**的一处是 `GetFieldValue` / `SetFieldValue` 里的"从托管对象读/写值"——每种类型的 mono 调用形态不同（值类型直接 `mono_field_get_value`、引用类型要先拿 `MonoObject*` 再取句柄），那里必须按类型写 `switch`。所以准确的说法是：**四个消费点里，表覆盖三个，读写那一处还得手写。**

#### 本 Phase 登记的类型（共 22 种）

| 组 | 类型 |
|----|------|
| 布尔 | `Bool` |
| 整数 | `SByte` `Byte` `Short` `UShort` `Int` `UInt` `Long` `ULong` |
| 浮点 | `Float` `Double` |
| 文本 | `String` |
| 数学 | `Vector2` `Vector3` `Vector4` `Quaternion` `Color` |
| 实体引用 | `Entity` |
| 资产引用 | `Material` `Mesh` `Texture2D` `Script` |

---

### 4.2 决策点 2：`ScriptFieldValue` 用什么表示

#### 方案 A：`std::variant` + 显式类型标记（**推荐 ✅**）

```cpp
using ScriptFieldScalar = std::variant<
    bool,
    int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t,
    float, double,
    std::string,
    glm::vec2, glm::vec3, glm::vec4, glm::quat,
    UUID,           // Entity 引用
    Ref<Asset>      // 资产引用（Material / Mesh / Texture2D / Script 共用这一个载荷）
>;

struct ScriptFieldValue
{
    ScriptFieldType  Type = ScriptFieldType::None;
    ScriptFieldScalar Data;
};
```

- **优点**：
  1. **类型安全** —— 不可能出现"两个值同时有效"，也不可能读到未被写入的载荷
  2. **加类型只改 variant 定义一处**（配着表一起改）
  3. **`Ref<Asset>` 一个载荷覆盖全部资产类型**，不需要每个资产类型一个 alternative
  4. 项目里已有成熟先例：`Renderer/Material.h` 的 `MaterialPropertyValue` 就是同构的 `std::variant` + `ShaderUniformType` 标记，消费方式也是 `switch (type) { std::get<T>(value); }`
- **缺点**：
  1. `Type` 与 `Data` 实际持有的 alternative **理论上可能不一致**（比如 `Type = Float` 而 `Data` 里是 `int32_t`）。用"**永远先看 `Type`、再用 `std::get` 取**"的约定约束，并在 XML 注释里写明
  2. 需要 `<variant>`，编译期有一点点开销

> **为什么必须有显式 `Type`**：`Color` 与 `Vector4` 都落在 `glm::vec4`、`Entity` 落在 `UUID` —— 光看 `variant::index()` 分不清 `Color` 和 `Vector4`。所以 `Type` 是**权威**，`Data` 只是载荷。

#### 方案 B：扁平结构体 + 每类型一个具名成员

```cpp
struct ScriptFieldValue
{
    ScriptFieldType Type = ScriptFieldType::None;
    float     FloatValue      = 0.0f;
    int       IntValue        = 0;
    bool      BoolValue       = false;
    glm::vec3 Vector3Value    = glm::vec3(0.0f);
    std::string StringValue;
    UUID      EntityValue;
    Ref<Asset> AssetValue;
    // ... 22 种类型就要 22 个成员
};
```

- **优点**：`= default` 构造/拷贝天然正确，读代码最直白
- **缺点**：
  1. **22 个成员永远同时存在、最多只有 1 个有效**，其余全是浪费的默认值
  2. **加一种类型就要改结构体定义**，并连带检查所有用到具名成员的地方
  3. 每个 `ScriptFieldValue` 都要带上 `std::string` + `Ref<Asset>` 这些"重"成员的构造/析构成本，哪怕它实际是个 `float`
- **否决**（4 种类型时它是最优解，22 种时不是）

#### 方案 C：定长字节缓冲 + 类型解释（Hazel 新版的做法）

```cpp
struct ScriptFieldValue
{
    ScriptFieldType Type;
    uint32_t Count;
    std::vector<std::byte> Data;   // Count * TypeSize(Type)
};
```

- **优点**：一个结构容纳一切（连数组都天然支持），加类型只改 `TypeSize` 表
- **缺点**：
  1. **完全放弃类型安全** —— 每个读写点都要 `Read<T>()` + 手工核对 `Type`，算错大小就是内存破坏
  2. `std::string` 不是"元素 × 长度"，要单独特殊处理（Hazel 的 string 数组那段代码最终是**注释掉的**）
  3. 我们不做数组（§1.3），这个方案最大的优势对我们无效
- **不采用**（它是"要支持数组"时才划算的取舍）

#### 方案 D：`std::any` / 手写 tagged union

- `std::any`：取出来要 `any_cast`，类型不对抛异常；序列化拿不到可读类型名。**否决。**
- 手写 `union`：`glm::vec3` / `std::string` / `Ref<Asset>` 都是非平凡类型，放进裸 `union` 要手动 `placement new` + 手动析构，否则 UB。**否决。**

**结论：方案 A。**

---

### 4.3 决策点 3：怎么判定字段的类型

#### 方案 A：查表（**推荐 ✅**）

```cpp
        ScriptFieldType ResolveScriptFieldType(MonoType* fieldType)
        {
            // mono_type_get_name 给出完整托管名：float -> "System.Single"，Lucky.Vector3 -> "Lucky.Vector3"
            // 数组会带 "[]" 后缀（如 "System.Single[]"），因此不在表里 -> 自动跳过，无需特判
            // 注意：mono_type_get_name 返回堆上新分配的字符串，调用方必须 mono_free
            char* managedName = mono_type_get_name(fieldType);

            ScriptFieldType fieldTypeResult = ScriptFieldType::None;
            const bool found = TryGetScriptFieldTypeByManagedName(managedName, fieldTypeResult);
            mono_free(managedName);

            return found ? fieldTypeResult : ScriptFieldType::None;
        }
```

- **优点**：
  1. **与决策点 4.1 的表天然配套** —— 类型名就是表里的 `ManagedName`，加类型不用碰这个函数
  2. **数组自动被排除**：数组的托管全名带 `[]` 后缀，匹配不上 → 走 `None` → 被跳过。**不需要任何数组特判代码**
  3. `enum` / 自定义 `struct` / `class` 同理自动跳过
- **缺点**：每次字段枚举都做一次字符串查找（表有 22 项）。但 `GetFields()` 有缓存、调用频次低，**这个开销看不见**

#### 方案 B：`switch (mono_type_get_type(...))` + 类名兜底

primitive 用 mono 的原生类型枚举（`MONO_TYPE_R4` → `Float`），`MONO_TYPE_VALUETYPE` / `MONO_TYPE_CLASS` 再用"命名空间 + 类名"比较。

- **优点**：primitive 那一半不依赖字符串，认错不了；理论上更快
- **缺点**：
  1. **又是一份独立的类型清单** —— 加一种 primitive 要改这个 `switch`，和表重复
  2. 需要手写"哪个 `MONO_TYPE_*` 对应哪个 `ScriptFieldType`"的映射，正是决策点 4.1 要消除的东西
  3. 数组要**显式**判断并排除（`MONO_TYPE_SZARRAY` / `MONO_TYPE_ARRAY`），而方案 A 是白送的
- **次优**：如果不想碰 `mono_type_get_name` 的堆字符串，可退回这个方案，但要把映射写成"表驱动"的形式

> **`mono_type_get_name` 的内存归属（已核实，容易写错）**：它返回的是**堆上新分配的字符串，调用方必须 `mono_free`** —— mono 源码实证：`mono_type_get_name` → `mono_type_get_name_full` 内部是 `g_string_new` + `g_string_free(result, FALSE)`，字符缓冲区归调用方释放，与 `mono_string_to_utf8` 同一套约定（`ScriptGlue.cpp` 里已有 `mono_free` 先例）。**不是**"mono 内部有缓存、不要 free"。忘了 free 不会立刻出事（`GetFields()` 有缓存，每类每字段只漏几十字节），但这段代码形态会被后续 Phase 复制。返回的名字确认为 `"System.Single"` 风格：老 Lucky 工程的工作代码用同一套字符串表匹配，实证可用。

#### 方案 C：把 `MonoType*` 缓存在静态变量里做指针比较

- **优点**：最快（指针相等）
- **缺点**：需要初始化时机管理（core image 就绪后、且每个 AppDomain 一次），跨重编译 / 热重载时 `MonoType*` 会失效，得记得重建。**收益在"每帧几十次比较"这个量级上完全看不见。否决。**

**结论：方案 A。**

---

### 4.4 决策点 4：容器放哪个头 —— 为什么拆成两个文件

#### 方案 A：拆成 `ScriptFieldType.h`（零依赖）+ `ScriptFieldValue.h`（有依赖）（**推荐 ✅**）

| 文件 | 内容 | 依赖 |
|------|------|------|
| `Scripting/ScriptFieldType.h` | `ScriptFieldType`、`ScriptFieldWidgetKind`、`ScriptFieldTypeInfo`、查表函数 | `<cstdint>` `<cstring>` |
| `Scripting/ScriptFieldValue.h` | `ScriptFieldScalar`、`ScriptFieldValue` | glm、`Asset/Asset.h`、`Core/UUID.h` |

- **优点**：
  1. **`Scripting/` 层不被 imgui 污染**：控件种类用零依赖的 `ScriptFieldWidgetKind` 表示，UI 层（P2.6）再把 kind → `PropertyFloat` / `PropertyCheckbox` 映射一次。若把 `ImGuiDataType` 直接放进类型表，`ScriptEngine.cpp`（引擎侧编译单元）就会被迫依赖 imgui
  2. "只想用类型表"的地方（UI 做映射、将来做类型清单展示）**不会被拖进 `Asset.h` / `UUID.h`**
  3. 依赖方向单向：`ScriptFieldValue.h` → `ScriptFieldType.h`，反过来不成立
- **缺点**：多一个头文件

#### 方案 B：一个文件全放

- **优点**：少一个文件
- **缺点**：要么零依赖的那部分被迫带上 glm / Asset 依赖，要么反过来（类型信息里塞进 imgui 类型）。**两个方向都有污染。否决。**

#### 方案 C：全部放进 `ScriptEngine.h`

- **优点**：少两个文件
- **缺点**：`ScriptEngine.h` include 了 `Lucky/Scene/Scene.h` 与 `Lucky/Scene/Entity.h`。`ScriptComponent.h`（P2.5）一旦包含它，就把**场景层、mono 前向声明、脚本引擎全套**拖进每个 include `Components.h` 的编译单元 —— 包括 `Scene.cpp`、`SceneSerializer` 等。**编译时间与耦合双输，而且极易演变成循环包含。否决。**

#### 方案 D：放进 `Scene/Components/ScriptComponent.h`

- **缺点**：**层级倒挂** —— 脚本字段的概念属于脚本系统，塞进组件头会让"谁拥有这个概念"变得模糊；而且 P2.6 的 Inspector 也要用它，从组件头里取数据很别扭。**否决。**

**结论：方案 A。**

---

### 4.5 决策点 5：过滤规则的具体实现

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
  1. **`ID` 由 `INIT_ONLY` 一条就挡掉了**，不需要写"名字叫 ID 就跳过"这种硬编码
  2. 语义精确：`readonly` 字段在 Inspector 里本来就不该可改
  3. `FIELD_ACCESS_MASK` 比较会把 `private` / `protected` / `internal` 全挡掉
- **缺点**：需要 `attrdefs.h`（已在上面的 include 说明里交代）

#### 方案 B：靠名字约定过滤（跳过 `ID`、跳过 `_` / `m_` 开头）

- **缺点**：**约定是隐式的**。用户写了个 public 字段叫 `ID` 就被悄悄吃掉，莫名其妙。**否决。**

#### 方案 C：额外过滤编译器生成字段（**必做，不是可选**）

```cpp
            const char* fieldName = mono_field_get_name(field);
            if (fieldName && std::strchr(fieldName, '<') != nullptr)
            {
                continue;
            }
```

- **说明**：C# 里写 `public float Speed { get; set; }`，编译器会生成形如 `<Speed>k__BackingField` 的字段。**判据用"名字里含 `<`"一条就够** —— 不要再叠加 `k__BackingField` 的字符串匹配（冗余，且名字格式是编译器实现细节）

**结论：方案 A + C 一起用。**（C 挡编译器生成，A 挡可见性 / static / readonly。）

---

### 4.6 决策点 6：默认值在哪一步读

字段的默认值（比如 `public float Speed = 3f;` 里的 `3f`）**不是元数据** —— 它被编译进构造函数的函数体，`mono_class_get_fields` 拿不到，必须**在一个实例上读**。

#### 方案 A：在 `GetFields()` 里读掉，存进 `ScriptField::DefaultValue`（**推荐 ✅**）

`GetFields()` 首次调用时（本来就在做枚举），顺手 `Instantiate()` 一个临时实例，逐个字段读值，读完丢弃实例。

- **优点**：
  1. **P2.5 因此少一整节** —— "首次挂上脚本时造临时实例读默认值"这段工作被吸收在这里，P2.5 直接从 `field.DefaultValue` 拿
  2. **枚举时本来就有机会拿到实例**，不需要额外时机；而且 `GetFields()` 有缓存，实例只造一次
  3. 语义完整：`ScriptField` 一条记录就描述清楚"这个字段是什么、默认值是多少"
- **缺点**：
  1. `GetFields()` 从"纯枚举"变成"枚举 + 实例化一次" —— 副作用要写在 XML 注释里（会调用脚本的构造函数）
  2. 若用户脚本构造函数有副作用（访问场景、打印日志），会提前触发一次。**但 `Awake` 不会被调用**（`Instantiate()` 只跑构造函数），影响可控。注意编辑期 `SceneContext` 为空：构造函数里调 `GetComponent` 会走 InternalCall 的空场景路径（warn + 返回空），日志里出现这类 warn 时不要误判为脚本坏了
  3. 临时实例由 mono 管理，不需要显式释放（GC 会回收），但要**注意不要在枚举过程中持有它做长期引用**

#### 方案 B：`GetFields()` 只枚举，默认值留到 P2.5 读

- **优点**：`GetFields()` 职责更单一（纯元信息），没有实例化副作用
- **缺点**：
  1. P2.5 要**再走一遍**"造临时实例 → 遍历字段 → 读值"的流程 —— 这和 `GetFields()` 里的枚举逻辑高度重复
  2. `ScriptField` 不带默认值，P2.5 的 `FieldMap` 初始化要额外处理"字段没有默认值"的中间状态
- **次优**：如果你希望 `GetFields()` 绝对无副作用，选这个，但要接受 P2.5 变大一节

**结论：方案 A。**

---

### 4.7 决策点 7：`[HideInInspector]` 要不要在本 Phase 做

Unity 用 `[HideInInspector]` 让某个 public 字段不出现在 Inspector 里。

#### 方案 A：本 Phase **不做**，只把字段全暴露（**推荐 ✅**）

- **优点**：
  1. 需要改动**托管侧**（新增一个特性类）、native 侧读取自定义特性、以及**编译后才能生效**的验证链路 —— 这是一整条独立的小工作流
  2. 当前脚本只有 1 个，字段要么有用要么删掉，暂时没有"想藏起来"的真实诉求
  3. 不做它**不阻塞**任何后续步骤
- **缺点**：用户以后想藏字段时得等一个后续 Phase

#### 方案 B：现在就做

- **缺点**：托管侧要加新类型；native 侧要用 `mono_custom_attrs_from_field` + `mono_custom_attrs_has_attr`（都在 `reflection.h`）取特性并逐个比对；**而且"特性类来自哪个程序集"要统一**，否则匹配不上会静默失效。工作量明显大于前几步之和。

#### 方案 C：用命名约定（字段名以 `_` 开头就隐藏）

- **缺点**：**隐式约定**，且与 C# 社区惯例（`_camelCase` 表示 private 字段）撞车 —— 用户会以为 `_speed` 是私有的。**否决。**

**结论：方案 A。** 后续实现要点已记在 §8。

---

### 4.8 决策点 8：字段列表的缓存策略

#### 方案 A：缓存到 `ScriptClass` 内部，首次调用时 lazy 构建（**推荐 ✅**）

```cpp
    private:
        std::vector<ScriptField> m_Fields;
        bool m_FieldsInitialized = false;
```

- **优点**：
  1. `ScriptClass` 的生命周期 = `EntityClasses` 的生命周期 = 一次程序集加载。**程序集重载时会整个重建 `EntityClasses`**（`LoadAssemblyClasses()` 里第一行就是 `s_EntityClasses.clear()`），缓存**自动**跟着失效，不需要任何手动清理
  2. 懒构建：没人用的类不花代价（`EntityClasses` 里可能有几十个类，只有挂上实体的才需要枚举字段）
  3. 默认值也只读一次（见决策点 4.6）
- **缺点**：如果同一个类被反复 `GetFields()`，第一次之后是读缓存，没问题

#### 方案 B：不缓存，每次重新枚举

- **缺点**：Inspector 每帧都会调 `GetFields()`，每次都 `mono_class_get_fields` 遍历 + 分配 `vector` + **造一个临时实例读默认值** —— 完全是可避免的浪费。**次优。**

#### 方案 C：缓存在 `ScriptEngine` 的全局 map 里（按类名索引）

- **缺点**：多一张表要维护，且**必须记得在 `LoadAssemblyClasses()` 里 clear** —— 这正是方案 A 自动解决的问题。**不必要。**

**结论：方案 A。**

---

### 4.9 决策点 9：基类字段要不要枚举 —— `mono_class_get_fields` 不含父类

#### 背景：一个容易漏掉的 mono 行为

`mono_class_get_fields` **只枚举本类声明的字段，不含继承来的字段**（mono 源码实证：迭代器只遍历 `m_class_get_fields(klass)` 这一个数组，父类要用 `mono_class_get_parent` 自己往上走。注意区分：`mono_class_get_field_from_name` **会**搜父类 —— 那是另一个 API，`TryGetReferenceHandle` 靠它才有保障）。

后果：用户只要写一个中间基类（`class Enemy : Entity`，再 `class Boss : Enemy`），`Enemy` 上的 public 字段在 Inspector 里**全部消失**。而 Unity 是显示继承字段的 —— 用户第一次写脚本基类就会撞上。

#### 方案 A：沿继承链逐级枚举（**推荐 ✅**）

```cpp
        for (MonoClass* currentClass = m_MonoClass; currentClass != nullptr; currentClass = mono_class_get_parent(currentClass))
        {
            void* iterator = nullptr;
            while (MonoClassField* field = mono_class_get_fields(currentClass, &iterator))
            {
                // ... 同一套过滤 + 收集 ...
            }
        }
```

- **优点**：
  1. **对齐 Unity 语义**：继承的 public 字段可见、可编辑、可序列化
  2. **不需要特判停止点**：链走到 `System.Object` 自然结束（它没有字段）；`Lucky.Entity` 的 `ID` 会被既有的 `INIT_ONLY` 过滤自然挡掉 —— 过滤器一条都不用改
  3. 成本低：外层多套一个 `for`，加一处同名去重
- **缺点**：字段顺序更"不承诺"了（子类在前、基类在后）—— 但顺序本来就不承诺（§6.6）

#### 方案 B：只枚举本类声明的字段

- **优点**：代码最简单
- **缺点**：脚本基类的字段**静默消失**，且与 Unity 行为不一致；等用户撞上再补，改动位置和现在一样，但那时要多解释一次"为什么之前不行"

**结论：方案 A。** 两个实现要点：

1. **同名去重**：子类先枚举，同名字段保留子类的（符合 C# `new` 隐藏语义）；去重发生在**过滤之后、入列之前**（过滤掉的字段不参与遮挡）
2. 默认值读取不受影响：`mono_field_get_value` 拿基类的 `MonoClassField*` 读派生实例是合法的（mono 按字段自身信息寻址）

---

### 4.10 决策点 10：`Entity` 派生类字段（脚本互引）怎么办

#### 背景

表里只登记了 `"Lucky.Entity"`。用户写 `public PlayerController Other;`（引用另一个挂了特定脚本的实体，Unity 里的高频用法），托管全名是 `"Sandbox.PlayerController"`，**匹配不上表** → 按现有规则会被跳过 + 打 WARN。

#### 方案 A：派生自 `Lucky.Entity` 的 class 字段，一律按 `Entity` 引用处理（**推荐 ✅**）

查表失败后再补一刀：

```cpp
        // MONO_TYPE_CLASS 且派生自 Lucky.Entity -> ScriptFieldType::Entity
        mono_class_is_subclass_of(fieldClass, entityClass, false)   // entityClass 从 CoreAssemblyImage 取 "Lucky"."Entity"
```

- **优点**：
  1. **约 10 行换来 Unity 高频用法的支持**；语义与 `Entity` 字段一致：引用一个实体，脚本运行期再通过基类 API 或 `GetComponent` 使用
  2. **读写路径零改动**：`TryGetReferenceHandle` 用 `mono_class_get_field_from_name` 查 `"ID"`（**会搜父类**，派生类实例能找到基类的 `ID`）；`SetReferenceHandle` 按字段声明类型 `mono_object_new` 建对象再写 `ID`，对派生类同样成立
- **缺点 / 已知边界**（写进文档即接受）：
  1. P2.6 的实体槽位只保证"拖入的是有效实体"，**不校验目标实体上是否真的挂了该派生类脚本**（Unity 会做组件级校验，我们留到后续增强）
  2. 只能表达"引用某个实体"，不能表达"引用某个组件"（我们的组件没有独立身份，对齐 Unity 的 `Transform`/`Camera` 组件引用是另一个设计）

#### 方案 B：明确不支持，写进"不做"清单

- **缺点**：用户第一次写脚本互引就撞上"字段消失 + WARN"，而 WARN 文案（"类型不支持"）会把他带向错误方向 —— 类型其实是支持的，只是形态没认出来

**结论：方案 A。** 本 Phase 只做**类型识别**（枚举成 `Entity` 类型、读写走 `Entity` 分支）；槽位 UI 与"是否挂了对应脚本"的校验属 P2.6 及以后。

---

## 5. 实现步骤（按依赖顺序）

每步完成后 `Lucky` 与 `Luck3DApp` 都应能编译通过。

### Step 0：补齐托管侧类型（B 组的前置，可选但推荐）

**文件**：`Lucky-ScriptCore/Source/Lucky/` 下新增

按 §3.6 的形态约定补齐 B 组类型。本步**不阻塞** 2.4 的机制实现与 A 组验收 —— 可以先做 Step 1~7，等要实际使用 `Material` 等字段时再回来补。

**要点**：

- 值类型（`Vector2` / `Vector4` / `Quaternion` / `Color`）**必须与 glm 对应类型布局一致**（照 §3.4 的契约），并在文件里加 `static_assert` 或注释说明
- 引用类型（`Material` / `Mesh` / `Texture2D` / `Script`）**必须暴露 `ulong Handle` 字段**（名字固定为 `Handle`），并在注释里写明"native 侧按此名字读写句柄"
- 补齐后**要重新编译用户脚本工程**（`Assembly-CSharp.dll`），否则程序集里没有这些类型，字段依然匹配不上

### Step 1：新建 `ScriptFieldType.h`

**文件**：`Lucky/Source/Lucky/Scripting/ScriptFieldType.h`

```cpp
#pragma once

#include <cstdint>
#include <cstring>

namespace Lucky
{
    /// <summary>
    /// 脚本字段类型：本引擎支持在 Inspector 中编辑的全部字段类型
    /// None 表示不支持的类型，枚举时会被跳过
    /// 新增类型时只改本文件：枚举加值 + 表里加一行
    /// </summary>
    enum class ScriptFieldType : uint8_t
    {
        None = 0,

        Bool,

        SByte, Byte, Short, UShort, Int, UInt, Long, ULong,

        Float, Double,

        String,

        Vector2, Vector3, Vector4, Quaternion, Color,

        Entity,

        Material, Mesh, Texture2D, Script
    };

    /// <summary>
    /// 字段在 Inspector 里用哪一类控件
    /// 刻意不引用 ImGuiDataType：本头必须零依赖，UI 层再把这个标识映射到具体控件
    /// </summary>
    enum class ScriptFieldWidgetKind : uint8_t
    {
        None = 0,
        Checkbox,
        Int,
        Float,
        Float2,
        Float3,
        Float4,
        Color,
        Text,
        EntityRef,
        AssetRef
    };

    /// <summary>
    /// 字段类型的完整描述：类型集合的唯一来源
    /// </summary>
    struct ScriptFieldTypeInfo
    {
        ScriptFieldType        Type;
        const char*            Name;         // 序列化用的可读名字，如 "Float"
        const char*            ManagedName;  // 托管侧完整类型名，反射匹配用，如 "System.Single"
        ScriptFieldWidgetKind  Widget;       // 控件种类
    };

    /// <summary>
    /// 全部受支持字段类型；新增类型时在此追加一行即可，各处消费点自动生效
    /// </summary>
    inline constexpr ScriptFieldTypeInfo s_ScriptFieldTypeInfos[] =
    {
        { ScriptFieldType::Bool,       "Bool",       "System.Boolean",   ScriptFieldWidgetKind::Checkbox  },
        { ScriptFieldType::SByte,      "SByte",      "System.SByte",     ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::Byte,       "Byte",       "System.Byte",      ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::Short,      "Short",      "System.Int16",     ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::UShort,     "UShort",     "System.UInt16",    ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::Int,        "Int",        "System.Int32",     ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::UInt,       "UInt",       "System.UInt32",    ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::Long,       "Long",       "System.Int64",     ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::ULong,      "ULong",      "System.UInt64",    ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::Float,      "Float",      "System.Single",    ScriptFieldWidgetKind::Float     },
        { ScriptFieldType::Double,     "Double",     "System.Double",    ScriptFieldWidgetKind::Float     },
        { ScriptFieldType::String,     "String",     "System.String",    ScriptFieldWidgetKind::Text      },
        { ScriptFieldType::Vector2,    "Vector2",    "Lucky.Vector2",    ScriptFieldWidgetKind::Float2    },
        { ScriptFieldType::Vector3,    "Vector3",    "Lucky.Vector3",    ScriptFieldWidgetKind::Float3    },
        { ScriptFieldType::Vector4,    "Vector4",    "Lucky.Vector4",    ScriptFieldWidgetKind::Float4    },
        { ScriptFieldType::Quaternion, "Quaternion", "Lucky.Quaternion", ScriptFieldWidgetKind::Float4    },
        { ScriptFieldType::Color,      "Color",      "Lucky.Color",      ScriptFieldWidgetKind::Color     },
        { ScriptFieldType::Entity,     "Entity",     "Lucky.Entity",     ScriptFieldWidgetKind::EntityRef },
        { ScriptFieldType::Material,   "Material",   "Lucky.Material",   ScriptFieldWidgetKind::AssetRef  },
        { ScriptFieldType::Mesh,       "Mesh",       "Lucky.Mesh",       ScriptFieldWidgetKind::AssetRef  },
        { ScriptFieldType::Texture2D,  "Texture2D",  "Lucky.Texture2D",  ScriptFieldWidgetKind::AssetRef  },
        { ScriptFieldType::Script,     "Script",     "Lucky.Script",     ScriptFieldWidgetKind::AssetRef  },
    };

    /// <summary>
    /// 按类型取描述信息；未登记的类型返回一个 Name 为 "None" 的静态条目
    /// </summary>
    inline const ScriptFieldTypeInfo& GetScriptFieldTypeInfo(ScriptFieldType type)
    {
        for (const ScriptFieldTypeInfo& info : s_ScriptFieldTypeInfos)
        {
            if (info.Type == type)
            {
                return info;
            }
        }

        static const ScriptFieldTypeInfo s_UnknownInfo = { ScriptFieldType::None, "None", "", ScriptFieldWidgetKind::None };
        return s_UnknownInfo;
    }

    /// <summary>
    /// 按托管侧完整类型名查类型；查不到返回 false
    /// </summary>
    /// <param name="managedName">如 "System.Single" / "Lucky.Vector3"</param>
    /// <param name="outType">输出：匹配到的字段类型</param>
    inline bool TryGetScriptFieldTypeByManagedName(const char* managedName, ScriptFieldType& outType)
    {
        if (!managedName)
        {
            return false;
        }

        for (const ScriptFieldTypeInfo& info : s_ScriptFieldTypeInfos)
        {
            if (std::strcmp(info.ManagedName, managedName) == 0)
            {
                outType = info.Type;
                return true;
            }
        }

        return false;
    }
}
```

**要点**：

- **header-only**：项目里已有先例（`Asset/AssetType.h` 的 `AssetTypeToString` / `StringToAssetType` 就是头内 inline），所以不需要新建 `.cpp`
- 表用 `inline constexpr`（C++17）避免多编译单元重复定义
- `std::strcmp` 需要 `<cstring>`；`uint8_t` 需要 `<cstdint>`
- **表里只登记受支持类型**。数组 / 枚举 / 自定义类型一律不登记 —— 这正是它们被自动跳过的原因
- `ScriptFieldWidgetKind` 是"控件种类"而不是"控件本身"。`Quaternion` 用 `Float4`（4 个 float 的编辑行）；`Color` 用独立的 `Color` kind —— UI 层已有现成的 `PropertyColor(glm::vec4&)` 颜色选择器（`UI/PropertyGrid.h`），比 4 个 float 的编辑行好用

### Step 2：新建 `ScriptFieldValue.h`

**文件**：`Lucky/Source/Lucky/Scripting/ScriptFieldValue.h`

```cpp
#pragma once

#include "Lucky/Scripting/ScriptFieldType.h"

#include "Lucky/Core/Base.h"            // Ref<T> 的出处；必须显式加，不依赖 PCH 传递包含
#include "Lucky/Core/UUID.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>       // glm::quat 不在 glm/glm.hpp 里，必须显式加（YamlHelpers.h / TransformComponent.h 都是这么做的）
#include <variant>
#include <string>

namespace Lucky
{
    class Asset;    // 前向声明即可：Ref<Asset>（shared_ptr）不需要完整类型；避免拖入 Asset.h（它的 AssetHandle.h 带着 yaml-cpp）

    /// <summary>
    /// 字段值的载荷：受支持类型的并集
    /// Color 与 Vector4 共用 glm::vec4、Entity 落在 UUID，
    /// 因此载荷本身不足以区分类型，必须结合 ScriptFieldValue::Type 使用
    /// </summary>
    using ScriptFieldScalar = std::variant<
        bool,
        int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t,
        float, double,
        std::string,
        glm::vec2, glm::vec3, glm::vec4, glm::quat,
        UUID,
        Ref<Asset>
    >;

    /// <summary>
    /// 脚本字段值：一个字段的取值
    /// 使用约定：Type 是权威 —— 先读 Type 判断有效载荷种类，再用 std::get<T> 取对应的值
    /// 不要依赖"载荷里当前恰好是哪个 alternative"
    /// </summary>
    struct ScriptFieldValue
    {
        ScriptFieldType  Type = ScriptFieldType::None;
        ScriptFieldScalar Data;
    };
}
```

**要点**：

- **不 include 任何 mono 头、不 include `ScriptEngine.h`**，保持零 mono / 零场景依赖
- `Ref<Asset>` 是 `std::shared_ptr<Asset>`（`Ref` 定义在 `Core/Base.h`；`Asset` 只前向声明，要**操作**资产对象的地方（`ScriptEngine.cpp`）再 include `Asset/Asset.h` 及具体资产头）。**资产引用字段的载荷统一用它** —— 一个 alternative 覆盖 `Material` / `Mesh` / `Texture2D` / `Script`，具体是哪种由 `ScriptFieldValue::Type` 区分
- **include 有两个坑，都已按上面代码处理**：① `glm::quat` **不在** `<glm/glm.hpp>` 里（缺了 `<glm/gtc/quaternion.hpp>` 时，走 `ScriptEngine.h` 包含链会被 `TransformComponent.h` 掩盖，但 P2.5 的 `ScriptComponent.h` 直接包含本头时必炸）；② `Ref` 来自 `Core/Base.h`，PCH（`lcpch.h` → `Log.h` → `Base.h`）会掩盖缺失，但头文件必须自洽
- **为什么不是 `AssetHandle`**：`AssetHandle` 只能表达"磁盘上注册过的资产"。引擎里存在**运行时创建**的资产对象（如 `Material::Create(shader)`，`Renderer/Material.cpp:107`），它们没有有效 handle。`Ref<Asset>` 两者都能装，且与 P2.3 已定的 `Ref<Script> ScriptAsset` 语义一致（`Scene::Copy` 时共享而非失效）
- `std::string` 放在 variant 里意味着容器自带堆分配 —— 这是必须的（`String` 是变长类型）
- 各类型的实际载荷类型见下表，**P2.5 序列化与 P2.6 控件都按它取 `std::get`**：

| `ScriptFieldType` | `ScriptFieldScalar` 里的类型 |
|-------------------|------------------------------|
| `Bool` | `bool` |
| `SByte` `Byte` `Short` `UShort` `Int` `UInt` `Long` `ULong` | `int8_t` `uint8_t` `int16_t` `uint16_t` `int32_t` `uint32_t` `int64_t` `uint64_t` |
| `Float` `Double` | `float` `double` |
| `String` | `std::string` |
| `Vector2` `Vector3` `Vector4` | `glm::vec2` `glm::vec3` `glm::vec4` |
| `Quaternion` | `glm::quat` |
| `Color` | `glm::vec4`（r/g/b/a） |
| `Entity` | `UUID` |
| `Material` `Mesh` `Texture2D` `Script` | `Ref<Asset>` |

### Step 3：`ScriptEngine.h` 加前向声明、`ScriptField` 与三个方法

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
#include "Lucky/Scripting/ScriptFieldType.h"
#include "Lucky/Scripting/ScriptFieldValue.h"
```

按规范 §3.3 放在工程内头分组（与 `Lucky/Core/Base.h` 等同组）。

**3) 在 `ScriptClass` 之前定义 `ScriptField`**：

```cpp
    /// <summary>
    /// 脚本字段元信息：字段名、受支持的类型、默认值、以及 mono 侧字段句柄
    /// 仅供 Scripting 层内部使用；组件层只需要 ScriptFieldValue
    /// </summary>
    struct ScriptField
    {
        std::string       Name;
        ScriptFieldType   Type = ScriptFieldType::None;
        ScriptFieldValue  DefaultValue;
        MonoClassField*   Field = nullptr;
    };
```

**4) `ScriptClass` 加三个方法声明**：

```cpp
        /// <summary>
        /// 获取该类的可编辑字段列表（public 实例字段、类型受支持）
        /// 首次调用时枚举并缓存，同时读取每个字段的默认值（会临时实例化一个对象）
        /// 程序集重新加载时随 EntityClasses 一起重建
        /// </summary>
        /// <returns>字段列表的常量引用</returns>
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
        /// <param name="value">要写入的值（按 value.Type 决定写入哪个载荷）</param>
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

- `ScriptField` 定义在**命名空间作用域**（不在 `ScriptClass` 内部）：`GetFields()` 返回 `const std::vector<ScriptField>&`，类型需要在类外可见
- `GetFields()` 是**非 const**（首次调用会改缓存）
- `GetFieldValue` / `SetFieldValue` 不修改 `ScriptClass` 本身 → 标 **`const`**（规范 §13.1）
- `ScriptField` 是 `struct`（纯数据，规范 §6.1），成员 `PascalCase`

### Step 4：`ScriptEngine.cpp` 加 include 与辅助函数

**文件**：`Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`

**1) 补 include**：

```cpp
#include <mono/metadata/attrdefs.h>     // MONO_FIELD_ATTR_*

#include "Lucky/Asset/AssetManager.h"
#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/Mesh.h"
#include "Lucky/Renderer/Texture.h"
#include "Lucky/Asset/Script.h"
```

按规范 §3.3 分组：`attrdefs.h` 放在 mono 组内、与 `class.h` 相邻；其余放在工程内头分组。

> 后四个是**资产引用字段读写**需要的（`AssetManager::GetAsset<T>` 的模板参数必须是完整类型）。如果只做 A 组，可以先只加 `AssetManager.h`，等 Step 5 写到资产分支时再补。
> `std::strchr` 需要 `<cstring>`（PCH 里通常已有，报错再补）。

**2) 在已有的匿名命名空间里**（`GetExceptionStringProperty` / `LogScriptException` 旁边）加以下辅助函数：

```cpp
        /// <summary>
        /// 把 mono 字段类型解析为本引擎支持的 ScriptFieldType；不支持时返回 None
        /// 数组的托管全名带 "[]" 后缀，匹配不上即自动跳过，无需特判
        /// </summary>
        ScriptFieldType ResolveScriptFieldType(MonoType* fieldType)
        {
            if (!fieldType)
            {
                return ScriptFieldType::None;
            }

            // mono_type_get_name 返回堆上新分配的字符串，调用方必须 mono_free（决策点 4.3）
            char* managedName = mono_type_get_name(fieldType);

            ScriptFieldType fieldTypeResult = ScriptFieldType::None;
            const bool found = TryGetScriptFieldTypeByManagedName(managedName, fieldTypeResult);
            mono_free(managedName);

            return found ? fieldTypeResult : ScriptFieldType::None;
        }

        /// <summary>
        /// 字段类型是否为 Lucky.Entity 的派生类（脚本互引，如 public PlayerController Other;）
        /// 这类字段按 ScriptFieldType::Entity 处理（决策点 4.10）
        /// </summary>
        bool IsEntityDerivedFieldType(MonoType* fieldType)
        {
            if (!fieldType || mono_type_get_type(fieldType) != MONO_TYPE_CLASS)
            {
                return false;
            }

            MonoClass* fieldClass = mono_class_from_mono_type(fieldType);
            MonoClass* entityClass = mono_class_from_name(s_Data->CoreAssemblyImage, "Lucky", "Entity");
            if (!fieldClass || !entityClass || fieldClass == entityClass)
            {
                return false;
            }

            return mono_class_is_subclass_of(fieldClass, entityClass, false);
        }

        /// <summary>
        /// 读取托管引用对象里的 ulong 句柄字段（Entity 用 "ID"，资产包装类型用 "Handle"）
        /// </summary>
        /// <returns>是否读取成功</returns>
        bool TryGetReferenceHandle(MonoObject* referenceObject, const char* handleFieldName, uint64_t& outHandle)
        {
            outHandle = 0;
            if (!referenceObject)
            {
                return false;
            }

            MonoClass* referenceClass = mono_object_get_class(referenceObject);
            MonoClassField* handleField = mono_class_get_field_from_name(referenceClass, handleFieldName);
            if (!handleField)
            {
                return false;
            }

            mono_field_get_value(referenceObject, handleField, &outHandle);
            return true;
        }

        /// <summary>
        /// 把 ulong 句柄写进托管引用对象（对象不存在时按字段声明类型新建一个）
        /// </summary>
        /// <returns>是否写入成功</returns>
        bool SetReferenceHandle(MonoObject* ownerInstance, MonoClassField* ownerField, const char* handleFieldName, uint64_t handle)
        {
            if (!ownerInstance || !ownerField)
            {
                return false;
            }

            MonoClass* referenceClass = mono_class_from_mono_type(mono_field_get_type(ownerField));
            MonoClassField* handleField = mono_class_get_field_from_name(referenceClass, handleFieldName);
            if (!handleField)
            {
                return false;
            }

            MonoObject* referenceObject = nullptr;
            mono_field_get_value(ownerInstance, ownerField, &referenceObject);
            if (!referenceObject)
            {
                referenceObject = mono_object_new(mono_domain_get(), referenceClass);
            }

            // handleField 是 ulong（值类型）：传值的地址
            mono_field_set_value(referenceObject, handleField, &handle);

            // ownerField 是引用类型：直接传对象指针（mono_field_set_value 语义不对称，见下方警告）
            mono_field_set_value(ownerInstance, ownerField, referenceObject);
            return true;
        }

        /// <summary>
        /// 读取值类型字段：按字节块直接拷贝到 T 再装进 outValue（布局契约见 §3.4）
        /// </summary>
        template <typename T>
        bool GetScalarField(MonoObject* instance, MonoClassField* field, ScriptFieldValue& outValue)
        {
            T rawValue{};
            mono_field_get_value(instance, field, &rawValue);
            outValue.Data = rawValue;
            return true;
        }

        /// <summary>
        /// 写入值类型字段：取 variant 载荷后按地址交给 mono_field_set_value
        /// </summary>
        template <typename T>
        bool SetScalarField(MonoObject* instance, MonoClassField* field, const ScriptFieldValue& value)
        {
            T rawValue = std::get<T>(value.Data);
            mono_field_set_value(instance, field, &rawValue);
            return true;
        }
```

**要点**：

- `ResolveScriptFieldType` 只做一件事：**查表 + `mono_free`**。加类型不用碰它（决策点 4.3）
- ⚠️ **`mono_field_set_value` 的第三参语义对值类型/引用类型是**不对称**的**：值类型字段传"指向值的缓冲区"（`&value`）；**引用类型字段直接传对象指针本身**（`monoString` / `managedObject`，**不带 `&`**）——mono 内部对引用类型走的是 `mono_gc_wbarrier_set_field(obj, addr, value)`，第三参就是要写入字段的指针值。实证：Godot 引擎 `gd_mono_field.cpp` 的 `MONO_TYPE_STRING` / `MONO_TYPE_CLASS` 分支全部直接传 `mono_string` / `managed`；mono 官方嵌入文档里 `mono_runtime_invoke` 的参数惯例相同（引用类型直接放指针、值类型放地址）。**给引用类型传 `&objectPtr` 会把栈地址写进字段，读回时解引用栈内存直接崩**（本 Phase 验收时真实踩过：`mono_string_to_utf8` 处 abort）。`mono_field_get_value` 则两种类型都传接收缓冲区（`&rawValue`），不要混淆
- `TryGetReferenceHandle` / `SetReferenceHandle` 是**引用类型（`Entity` 与四种资产类型）读写共用的**，把"按名字取句柄字段"这件事收在一处。两个函数都只处理 `ulong` 句柄，**不涉及具体资产类型** —— 具体类型的解析（`GetAsset<T>`）留在 `GetFieldValue` / `SetFieldValue` 的 `switch` 里
- 句柄字段的查找用 `mono_class_get_field_from_name`，它**会沿继承链搜索**（mono 注释原文 "Search the class klass and its parents"）—— 所以 `Entity` 派生类实例（决策点 4.10）也能找到基类 `Lucky.Entity` 的 `ID`
- `GetScalarField<T>` / `SetScalarField<T>` 把"值类型字段按字节块读写"的调用形态收在一处：`GetFieldValue` / `SetFieldValue` 里 14 个标量 case 每个只剩一行（Step 5.2 / 5.3），杜绝手抄 12 遍抄错宽度。`Bool` 是唯一的例外（C# `bool` 为 1 字节，用 `uint8_t` 中转，不依赖 `sizeof(bool) == 1`）

- 新增类型时，如果它是"引用类型"，只需要在 `GetFieldValue` / `SetFieldValue` 里加一个 case 调 `TryGetReferenceHandle` / `SetReferenceHandle`；如果它是值类型，加一个调 `GetScalarField<T>` / `SetScalarField<T>` 的一行 case

### Step 5：实现三个成员函数

**文件**：`Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`

加在 `ScriptClass::InvokeMethod` 实现**之后**（同一组成员函数放在一起）。

#### 5.1 `GetFields()`

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

        // 默认值不是元数据（它被编译进构造函数体），必须在一个实例上读
        // 这里造一个临时实例，读完即丢弃；Awake 不会被调用，只有构造函数会跑
        MonoObject* defaultInstance = Instantiate();

        // mono_class_get_fields 只枚举本类声明的字段、不含父类 -> 沿继承链逐级枚举（决策点 4.9）
        // 到 System.Object 自然结束；Lucky.Entity 的 ID 会被 INIT_ONLY 过滤自然挡掉，无需特判
        for (MonoClass* currentClass = m_MonoClass; currentClass != nullptr; currentClass = mono_class_get_parent(currentClass))
        {
            void* iterator = nullptr;
            MonoClassField* field = nullptr;
            while ((field = mono_class_get_fields(currentClass, &iterator)) != nullptr)
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
                if (fieldName && std::strchr(fieldName, '<') != nullptr)
                {
                    continue;
                }

                // 同名去重：子类先枚举，子类用 new 隐藏基类字段时子类优先；被过滤掉的字段不参与遮挡
                bool isShadowed = false;
                for (const ScriptField& existing : m_Fields)
                {
                    if (existing.Name == fieldName)
                    {
                        isShadowed = true;
                        break;
                    }
                }
                if (isShadowed)
                {
                    continue;
                }

                MonoType* fieldMonoType = mono_field_get_type(field);
                ScriptFieldType fieldType = ResolveScriptFieldType(fieldMonoType);
                if (fieldType == ScriptFieldType::None && IsEntityDerivedFieldType(fieldMonoType))
                {
                    fieldType = ScriptFieldType::Entity;    // 脚本互引：Entity 派生类按实体引用处理（决策点 4.10）
                }
                if (fieldType == ScriptFieldType::None)
                {
                    LF_CORE_WARN("ScriptClass::GetFields - Unsupported type for field '{0}.{1}', ignored", m_ClassName, fieldName);
                    continue;
                }

                ScriptField scriptField;
                scriptField.Name = fieldName;
                scriptField.Type = fieldType;
                scriptField.Field = field;

                if (defaultInstance)
                {
                    GetFieldValue(defaultInstance, scriptField, scriptField.DefaultValue);
                }

                m_Fields.push_back(scriptField);
            }
        }

        return m_Fields;
    }
```

#### 5.2 `GetFieldValue()`

值类型直接按字节块读；引用类型先拿 `MonoObject*`，再从句柄字段取出 `ULong` 还原成 `UUID` / `Ref<Asset>`。

```cpp
    bool ScriptClass::GetFieldValue(MonoObject* instance, const ScriptField& field, ScriptFieldValue& outValue) const
    {
        if (!instance || !field.Field)
        {
            return false;
        }

        outValue.Type = field.Type;

        switch (field.Type)
        {
            case ScriptFieldType::Bool:
            {
                // C# bool 在托管侧是 1 字节，用 uint8_t 中转，避免依赖 sizeof(bool) == 1
                uint8_t rawValue = 0;
                mono_field_get_value(instance, field.Field, &rawValue);
                outValue.Data = (rawValue != 0);
                return true;
            }
            case ScriptFieldType::SByte:  { return GetScalarField<int8_t>(instance, field.Field, outValue); }
            case ScriptFieldType::Byte:   { return GetScalarField<uint8_t>(instance, field.Field, outValue); }
            case ScriptFieldType::Short:  { return GetScalarField<int16_t>(instance, field.Field, outValue); }
            case ScriptFieldType::UShort: { return GetScalarField<uint16_t>(instance, field.Field, outValue); }
            case ScriptFieldType::Int:    { return GetScalarField<int32_t>(instance, field.Field, outValue); }
            case ScriptFieldType::UInt:   { return GetScalarField<uint32_t>(instance, field.Field, outValue); }
            case ScriptFieldType::Long:   { return GetScalarField<int64_t>(instance, field.Field, outValue); }
            case ScriptFieldType::ULong:  { return GetScalarField<uint64_t>(instance, field.Field, outValue); }
            case ScriptFieldType::Float:  { return GetScalarField<float>(instance, field.Field, outValue); }
            case ScriptFieldType::Double: { return GetScalarField<double>(instance, field.Field, outValue); }
            case ScriptFieldType::Vector2:    { return GetScalarField<glm::vec2>(instance, field.Field, outValue); }
            case ScriptFieldType::Vector3:    { return GetScalarField<glm::vec3>(instance, field.Field, outValue); }
            case ScriptFieldType::Vector4:
            case ScriptFieldType::Color:      { return GetScalarField<glm::vec4>(instance, field.Field, outValue); }
            case ScriptFieldType::Quaternion: { return GetScalarField<glm::quat>(instance, field.Field, outValue); }
            case ScriptFieldType::String:
            {
                // System.String 是引用类型：mono_field_get_value 写出的是 MonoString*
                MonoString* rawValue = nullptr;
                mono_field_get_value(instance, field.Field, &rawValue);
                if (!rawValue)
                {
                    outValue.Data = std::string();
                    return true;
                }

                char* utf8 = mono_string_to_utf8(rawValue);
                outValue.Data = utf8 ? std::string(utf8) : std::string();
                if (utf8)
                {
                    mono_free(utf8);
                }
                return true;
            }
            case ScriptFieldType::Entity:
            {
                MonoObject* rawObject = nullptr;
                mono_field_get_value(instance, field.Field, &rawObject);

                uint64_t handle = 0;
                TryGetReferenceHandle(rawObject, "ID", handle);
                outValue.Data = UUID(handle);
                return true;
            }
            case ScriptFieldType::Material:
            case ScriptFieldType::Mesh:
            case ScriptFieldType::Texture2D:
            case ScriptFieldType::Script:
            {
                MonoObject* rawObject = nullptr;
                mono_field_get_value(instance, field.Field, &rawObject);

                uint64_t handle = 0;
                if (!TryGetReferenceHandle(rawObject, "Handle", handle))
                {
                    outValue.Data = Ref<Asset>(nullptr);
                    return true;
                }

                outValue.Data = ResolveAssetByFieldType(field.Type, AssetHandle(handle));
                return true;
            }
            default:
            {
                return false;
            }
        }
    }
```

#### 5.3 `SetFieldValue()`

```cpp
    bool ScriptClass::SetFieldValue(MonoObject* instance, const ScriptField& field, const ScriptFieldValue& value) const
    {
        if (!instance || !field.Field)
        {
            return false;
        }

        if (field.Type != value.Type)
        {
            LF_CORE_ERROR("ScriptClass::SetFieldValue - Type mismatch for field '{0}': field is {1}, value is {2}",
                field.Name, GetScriptFieldTypeInfo(field.Type).Name, GetScriptFieldTypeInfo(value.Type).Name);
            return false;
        }

        switch (value.Type)
        {
            case ScriptFieldType::Bool:
            {
                uint8_t rawValue = std::get<bool>(value.Data) ? 1 : 0;
                mono_field_set_value(instance, field.Field, &rawValue);
                return true;
            }
            case ScriptFieldType::SByte:  { return SetScalarField<int8_t>(instance, field.Field, value); }
            case ScriptFieldType::Byte:   { return SetScalarField<uint8_t>(instance, field.Field, value); }
            case ScriptFieldType::Short:  { return SetScalarField<int16_t>(instance, field.Field, value); }
            case ScriptFieldType::UShort: { return SetScalarField<uint16_t>(instance, field.Field, value); }
            case ScriptFieldType::Int:    { return SetScalarField<int32_t>(instance, field.Field, value); }
            case ScriptFieldType::UInt:   { return SetScalarField<uint32_t>(instance, field.Field, value); }
            case ScriptFieldType::Long:   { return SetScalarField<int64_t>(instance, field.Field, value); }
            case ScriptFieldType::ULong:  { return SetScalarField<uint64_t>(instance, field.Field, value); }
            case ScriptFieldType::Float:  { return SetScalarField<float>(instance, field.Field, value); }
            case ScriptFieldType::Double: { return SetScalarField<double>(instance, field.Field, value); }
            case ScriptFieldType::Vector2:    { return SetScalarField<glm::vec2>(instance, field.Field, value); }
            case ScriptFieldType::Vector3:    { return SetScalarField<glm::vec3>(instance, field.Field, value); }
            case ScriptFieldType::Vector4:
            case ScriptFieldType::Color:      { return SetScalarField<glm::vec4>(instance, field.Field, value); }
            case ScriptFieldType::Quaternion: { return SetScalarField<glm::quat>(instance, field.Field, value); }
            case ScriptFieldType::String:
            {
                const std::string& text = std::get<std::string>(value.Data);
                MonoString* rawValue = mono_string_new(mono_domain_get(), text.c_str());
                // 引用类型字段：直接传对象指针（见 SetReferenceHandle 的警告），传 &rawValue 会把栈地址写进字段
                mono_field_set_value(instance, field.Field, rawValue);
                return true;
            }
            case ScriptFieldType::Entity:
            {
                const UUID id = std::get<UUID>(value.Data);
                SetReferenceHandle(instance, field.Field, "ID", static_cast<uint64_t>(id));
                return true;
            }
            case ScriptFieldType::Material:
            case ScriptFieldType::Mesh:
            case ScriptFieldType::Texture2D:
            case ScriptFieldType::Script:
            {
                const Ref<Asset> asset = std::get<Ref<Asset>>(value.Data);
                const AssetHandle handle = asset ? asset->GetHandle() : AssetHandle{};
                SetReferenceHandle(instance, field.Field, "Handle", static_cast<uint64_t>(handle));
                return true;
            }
            default:
            {
                return false;
            }
        }
    }
```

> 标量 case 每个只有一行，靠的是 Step 4 的 `GetScalarField<T>` / `SetScalarField<T>` 模板。**不要**把模板再合并成"统一取 8 字节"这类写法 —— 宽度不匹配会写坏相邻内存。

#### 5.4 需要一个新辅助函数：`ResolveAssetByFieldType`（`ScriptEngine` 公开静态方法）

`GetFieldValue` 里那句 `ResolveAssetByFieldType(field.Type, handle)` 是**必须的**，因为 `AssetManager::GetAsset<T>` 的模板参数必须在编译期确定。它**不放匿名命名空间**：P2.5 反序列化资产引用字段时也要按类型解析 handle（`ComponentSerializers.cpp` 是另一个编译单元），所以声明在 `ScriptEngine` 公开区、定义在类外：

```cpp
        // ScriptEngine.h 公开区（Asset 前向声明即可，返回类型是 Ref<Asset>）：
        /// <summary>
        /// 按字段类型从资产句柄解析资产对象；类型不匹配或句柄无效时返回空引用
        /// </summary>
        static Ref<Asset> ResolveAssetByFieldType(ScriptFieldType type, AssetHandle handle);

        // ScriptEngine.cpp：
        Ref<Asset> ScriptEngine::ResolveAssetByFieldType(ScriptFieldType type, AssetHandle handle)
        {
            if (!handle.IsValid())
            {
                return nullptr;
            }

            switch (type)
            {
                case ScriptFieldType::Material:
                {
                    return AssetManager::GetAsset<Material>(handle);
                }
                case ScriptFieldType::Mesh:
                {
                    return AssetManager::GetAsset<Mesh>(handle);
                }
                case ScriptFieldType::Texture2D:
                {
                    return AssetManager::GetAsset<Texture2D>(handle);
                }
                case ScriptFieldType::Script:
                {
                    return AssetManager::GetAsset<Script>(handle);
                }
                default:
                {
                    return nullptr;
                }
            }
        }
```

**要点**：

- **这是"表覆盖不了的那一处"**（决策点 4.1 的补充说明）：`GetAsset<T>` 是模板，只能按类型写 `switch`。但注意它**只有 4 个资产类型**，而且加资产类型时的改动**局限在这一个函数里**
- `Ref<T>` 隐式转 `Ref<Asset>`（`shared_ptr` 派生到基类）✓
- `ScriptEngine.h` 里 `Asset` 前向声明就够（`Ref<Asset>` 返回类型不需要完整类型）；`AssetHandle` 经 `ScriptFieldValue.h` → `Core/UUID.h` 链不可达，需在 `ScriptEngine.h` 补 `#include "Lucky/Asset/AssetHandle.h"`（`AssetHandle` 是值类型参数，必须完整类型）
- 若某个资产类型还没被 `AssetManager` 实例化（`GetExpectedAssetType` 特化 + 显式实例化），会**链接期报错** —— P2.1 已经为 `Script` 补过一次（`LNK2019 ... GetAsset<class Lucky::Script>`）

**其它要点**：

- **`SetFieldValue` 的 `switch` 判的是 `value.Type`，且开头做了 `field.Type != value.Type` 的校验** —— 这是 `ScriptFieldValue` 方案 A 那个"`Type` 与载荷可能不一致"缺点的兜底。类型不匹配时拒写并报错，**不静默**。P2.6 灌值时若出现这条日志，说明 `FieldMap` 里存了脏数据
- `Bool` 用 `uint8_t` 中转而不是 `bool`：C# 的 `bool` 在 mono 里是 1 字节，而 C++ 的 `bool` 大小是实现定义的。显式写出来避免隐含假设
- `Vector3` / `Vector4` / `Quaternion` 直接传 glm 指针：依据是 §3.4 的布局契约
- **`String` 是唯一需要手动管理内存的类型**：读用 `mono_string_to_utf8` + `mono_free`；写用 `mono_string_new`。**不要漏掉 `mono_free`**（每次 Inspector 重画都会读一遍字段，泄漏会累积）
- **`Entity` 字段写入的已知风险**：`Lucky.Entity.ID` 是 `readonly`，`mono_field_set_value` 对 `initonly` 字段**能否写入需要实测**。若写不进：
  - 优先方案：在托管侧给 `Entity` 加一个 internal 的构造/设置入口，native 侧改为调用它
  - 次选方案：本 Phase **对 `Entity` 字段只支持读**，写入留到有真实需求时再解决
  - **不要**为了让写入工作而把 `ID` 的 `readonly` 去掉 —— 那会破坏 P1 定下的托管 API 契约

### Step 6：编译验证

```bash
Scripts/Setup-Windows.bat
```

- 确认 `Lucky.vcxproj` 里出现 `ScriptFieldType.h` 与 `ScriptFieldValue.h`
- Debug 编译 `Lucky` → 通过
- Debug 编译 `Luck3DApp` → 通过
- **若报 `MONO_FIELD_ATTR_PUBLIC` 未定义**：Step 4-1 的 `#include <mono/metadata/attrdefs.h>` 没加
- **若报 `std::strchr` / `std::strcmp` 未定义**：补 `#include <cstring>`
- **若报 `glm::quat` 不完整类型或 `Ref` 未定义**：`ScriptFieldValue.h` 的 include 缺了 `<glm/gtc/quaternion.hpp>` 或 `Lucky/Core/Base.h`（PCH 会掩盖这两个缺失——本机能编过不代表头文件自洽，别依赖 PCH）
- **若报 `LNK2019 ... GetAsset<class Lucky::XXX>`**：`AssetManager` 缺该类型的显式实例化（见 §5.4 要点）

### Step 7（一次性自证，验收后删除）：给示例脚本加字段并打印枚举结果

**前置**：给 `Luck3DApp/Project/Assets/Scripts/PlayerController.cs` **临时**加几个字段（验证完**建议保留**，P2.5 / P2.6 正好需要它们当测试样本）：

```csharp
    public class PlayerController : Entity
    {
        public float Speed = 3.0f;
        public bool LogPosition = false;
        public string Title = "player";
        public Vector3 Offset = new Vector3(0.0f, 0.0f, 0.0f);
        public int Score = 42;

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
            LF_CORE_INFO("ScriptEngine: field test - {0} (type {1})",
                field.Name, GetScriptFieldTypeInfo(field.Type).Name);
        }
    }
```

**预期**（顺序不保证，`mono_class_get_fields` 的返回顺序不承诺）：

```
ScriptEngine: field test - Speed (type Float)
ScriptEngine: field test - LogPosition (type Bool)
ScriptEngine: field test - Title (type String)
ScriptEngine: field test - Offset (type Vector3)
ScriptEngine: field test - Score (type Int)
```

**必须验证的负向结果**：

- **没有** `ID`（被 `MONO_FIELD_ATTR_INIT_ONLY` 挡掉）
- **没有** `<Speed>k__BackingField`（前提是你没改成自动属性；想验这条就把 `Speed` 临时改成 `public float Speed { get; set; } = 3.0f;` 再看一次）

**⚠️ 验收完成后必须删除 `Init` 里这段临时代码**（`.cs` 里的字段可以保留）。

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
- `GetFields()`：**Inspector 每帧都会调**（要画控件），每次都重新 `mono_class_get_fields` + 分配 `vector` + **造临时实例读默认值**是纯浪费

而缓存的失效问题在 `GetFields()` 这里是**免费解决**的：`ScriptClass` 对象活在 `EntityClasses` 里，程序集重载时 `LoadAssemblyClasses()` 第一行 `EntityClasses.clear()` 会把所有 `ScriptClass` **连同缓存一起丢掉**。

### 6.3 `ID` 字段为什么用 `INIT_ONLY` 就能挡住，而不必判名字？

`readonly` 字段在 IL 里带 `InitOnly` 标志，`mono_field_get_flags` 会如实返回。`Lucky.Entity.ID` 声明为 `public readonly ulong ID;`，所以 `MONO_FIELD_ATTR_INIT_ONLY` 必然命中。

一个前提要说清楚：`ID` 声明在基类 `Lucky.Entity` 上，而 `mono_class_get_fields` 不枚举父类字段 —— 如果只枚举本类，`ID` 根本不会出现。它会冒出来是因为我们沿继承链枚举（决策点 4.9），这时 `INIT_ONLY` 过滤就是**必需**的，不是多余的保险。

好处是**规则是通用的**：以后用户自己写 `public readonly int SomeId;`（无论写在本类还是基类）也一样会被挡掉。

### 6.4 如果用户写了 `public string Name = "abc";` 会怎样？

**会正常工作。** `System.String` 在表里（`ScriptFieldType::String`），所以字段会被枚举出来，Inspector 里是一个文本输入框（P2.6）。

但要注意读写路径的特殊性：

- 读：`mono_field_get_value` 对引用类型写出的是 `MonoString*`，要 `mono_string_to_utf8` 转成 `std::string`，**用完 `mono_free`**
- 写：要用 `mono_string_new(mono_domain_get(), ...)` 造一个托管字符串

如果用户写的是 `public int[] Scores;`（数组），则托管全名是 `System.Int32[]`，**匹配不上表** → 被跳过 + 打 WARN。这是预期行为（§1.3）。

### 6.5 `GetFieldValue` / `SetFieldValue` 传进来的 `field` 必须是本类的字段吗？

**是的，这是前置契约**（XML 注释里写了"必须来自本类的 `GetFields()`"）。如果传了一个别的类的 `MonoClassField*`，`mono_field_get_value` 会按错误的字段布局去读实例内存 —— 轻则读到垃圾，重则越界。

**不打算在函数里校验**（校验需要比较 `mono_field_get_parent(field) == m_MonoClass`，该 API 在 `class.h:244`）—— 调用方（P2.6）本来就是从 `GetFields()` 拿的字段再用的，属于内部契约。若你希望更稳，可以加一行 `LF_CORE_ASSERT`。

### 6.6 字段的声明顺序在 Inspector 里能保证吗？

**不能，也不该依赖**。`mono_class_get_fields` 走的是元数据表的顺序，**mono 不承诺它等于源码声明顺序**。

如果需要"在源码里控制 Inspector 顺序"，唯一可靠的办法是加一个显式的排序特性。**本 Phase 不做，也不建议在后续阶段做** —— 字段少的时候顺序无所谓，字段多了本来就该用分组/自定义 Inspector。

### 6.7 继承链上的字段按什么顺序出现？同名（`new` 隐藏）怎么办？

`GetFields()` 从本类往基类逐级枚举：**子类字段在前，基类字段在后**。同名字段按"先枚举到的赢"去重，即**子类优先** —— 正好符合 C# `new` 隐藏的语义。去重发生在过滤之后，所以"子类写了个 private 字段、基类有个同名 public 字段"时，基类那个**会**出现（private 的被过滤，不参与遮挡）—— 这也和 Unity 的行为一致。

字段在 Inspector 里的顺序本来就不承诺（§6.6），继承只是让它更不承诺，不需要为此做什么。

### 6.8 这一步能不能不做，直接在 P2.5 里连字段一起搞？

技术上可以，但**不推荐**：

- 字段反射是**最容易踩 mono API 细节**的一步（flags 常量、遍历方式、类型名匹配、bool 宽度、引用类型的句柄提取、glm 布局契约）
- 把它独立出来，可以**先用日志把"枚举结果 + 默认值对不对"验干净**（Step 7 的自证），再往上叠"存进组件 + 序列化 + 画控件"
- 混在一起做，一旦 Inspector 显示不对，你分不清是"反射错了"还是"存储错了"还是"控件错了"

### 6.9 为什么 `ScriptFieldValue` 不直接用 `AssetHandle` 存资产引用？

因为 `AssetHandle` **只能表达"磁盘上注册过的资产"**。引擎里存在运行时创建的资产对象 —— `Renderer/Material.cpp:107` 的 `Material::Create(shader)` 就是一个（它没有文件、没有 handle）。

`Ref<Asset>` 两者都能装，而且和 P2.3 已定的 `Ref<Script> ScriptAsset` 语义一致（`Scene::Copy` 时共享而非失效）。

代价要说清楚：**运行时创建的资产不会被存进场景** —— 序列化时写出的 handle 是 0，读回来就是空引用。这与 Unity 的行为一致（编辑器里 `new` 出来的 material 存不住，必须先 `AssetDatabase.CreateAsset` 落盘）。

---

## 7. 验收标准

1. **编译通过**：`Lucky` 与 `Luck3DApp` 的 Debug / Release / Dist 三个 configuration 全通过
2. **枚举正确（A 组）**：`PlayerController` 的 `GetFields()` 返回 `Speed`(Float)、`LogPosition`(Bool)、`Title`(String)、`Offset`(Vector3)、`Score`(Int)；**不含** `ID`、**不含**编译器生成字段
3. **顺序不承诺**：第 2 条的字段顺序不要求与源码一致
4. **默认值正确**：`Speed` 的 `DefaultValue` 是 `Float` 且等于 `3.0f`；`Title` 是 `String` 且等于 `"player"`（**这一条验的是"枚举时读默认值"这条路真的通了**）
5. **不支持类型被跳过**：临时加 `public int[] Scores;` → 被跳过，日志有 `Unsupported type`；**不崩**
6. **读值正确**：对 `Speed` 调 `GetFieldValue`，返回 `Type == Float` 且值为 `3.0f`
7. **写值生效**：对 `Speed` 调 `SetFieldValue(Float=7.0f)`，再 `GetFieldValue` 读回是 `7.0f`（**必须在托管实例上真读真写**）
8. **字符串往返**：写 `Title = "changed"` → 读回 `"changed"`；连续读写 100 次，**内存不持续增长**（验 `mono_free` 没漏）
9. **类型不匹配被拒**：`SetFieldValue` 传入 `field.Type != value.Type` 的值 → 返回 `false`，日志有 `Type mismatch`，**目标字段未被改写**
10. **缓存生效且无副作用**：连续调 `GetFields()` 10 次，返回的 `vector` 地址不变、`size()` 不变、内容一致
11. **B 组按托管类型就绪情况验证**：补齐了 `Lucky.Material` 等托管类型后，`public Material Mat;` 能被枚举出来，拖入资产后读写往返正确（**未补齐托管类型时跳过本条，不算失败**）
12. **不破坏既有行为**：Play / Stop / 脚本 `Awake` / `Update` / `OnDestroy` 全部与改动前一致
13. **临时代码已删除**：Step 7 在 `Init` 里的自证代码确认已移除
14. **代码规范**：通过人工 checklist —— `enum class`（§8.1）；纯数据用 `struct`（§6.1）；`switch` 各 case 带花括号（§5.2）；不修改对象的方法标 `const`（§13.1）；`while` / `for` 不带 `else`；公有接口有 `/// <summary>` 中文注释（§4.1）；**日志与提示全英文、不含中文字符**（§9.2）；无"为对齐而对齐"的空格（§5.4）；无引用外部文档的注释、无"P2.5 会用到"这类阶段性注释
15. **继承字段被枚举**（验决策点 4.9）：临时给 `PlayerController` 加中间基类（`class CharacterBase : Entity { public float Health = 100.0f; }`，让 `PlayerController : CharacterBase`）→ `GetFields()` 含 `Health`(Float) 且 `DefaultValue` 为 `100.0f`；仍**不含** `ID`。验完还原
16. **Entity 派生类引用**（验决策点 4.10）：临时加 `public PlayerController Other;` → 被枚举为 `Entity` 类型，**不打** `Unsupported type` 警告。验完还原

> ⚠️ **第 7 条和第 4 条是这一整步的"生死线"**：
> - 第 7 条（写值）不通，P2.6 的"Inspector 改值 → 脚本读到"整条链就是空的
> - 第 4 条（默认值）不通，P2.5 的 `FieldMap` 初始值就全是 0
>
> **两条都必须在托管对象上真读真写**，不能只看编译过。

---

## 8. 对下一 Phase 的接线点

| 位置 | 后续 Phase | 打开方式 |
|------|-----------|---------|
| `ScriptFieldType.h` 的类型表 | **所有后续 Phase** | 序列化查 `Name`、反序列化遍历找 `Name`、UI 查 `Widget`。**加类型只改这一个文件** |
| `ScriptFieldValue`（`ScriptFieldValue.h`） | **P2.5** | 直接作为 `ScriptComponent::FieldMap` 的 value 类型；组件头只依赖这个轻头，不碰 `ScriptEngine.h` |
| `ScriptField::DefaultValue` | **P2.5** | `FieldMap` 初始化时直接用它，**不需要再造临时实例**（本 Phase 已经读好） |
| `ScriptClass::GetFields()` | **P2.5 / P2.6** | P2.5 用它在"首次挂上脚本"时填 `FieldMap`；P2.6 用它画控件 |
| `ScriptClass::SetFieldValue` | **P2.6** | 在 `ScriptInstance::InvokeAwake()` **之前**把所有 `FieldMap` 值灌进去 |
| `ScriptFieldWidgetKind` | **P2.6** | UI 层把它映射到具体控件。已核实**现成**（`UI/PropertyGrid.h`）：`PropertyFloat` / `PropertyInt` / `PropertyCheckbox` / `PropertyFloat2` / `PropertyFloat3` / `PropertyFloat4` / `PropertyColor(vec4)` / `PropertyString(char*, size_t)` / `PropertyAsset<T>`；**需新写**：64 位整数控件（`PropertyLong`，给 `UInt`/`Long`/`ULong`）与实体引用槽（`PropertyEntityRef`，拖放源用场景树的 `DragDrop::EntityHierarchy`） |
| variant 载荷类型 | **P2.6** | 读写 `Data` 必须按 `Type` 对应的**实际载荷类型** `std::get`：8 种整数宽度不同、`Double` 是 `double`。`UI::PropertyInt`（int32）装不下 `long`/`ulong`、`PropertyFloat`（float）装不下 `double` —— **显示可以截断，写回必须保住原值**，不能 int32 读出来再写回去 |
| `ResolveAssetByFieldType` | 加资产类型时 | `GetAsset<T>` 是模板，只能在这里加 case（决策点 4.1 里"表覆盖不了"的那一处） |
| `[HideInInspector]` | 后续增强 | 托管侧加特性类；native 侧用 `reflection.h` 的 `mono_custom_attrs_from_field` + `mono_custom_attrs_has_attr` 在 `GetFields()` 里判一次 |
| 数组 / 枚举 / 嵌套类 | 后续增强 | ① 类型表加行（数组还需要"元素类型 + 长度"的表达）② `ScriptFieldValue` 加载荷 ③ 读写加 case ④ 序列化加分支 ⑤ 控件加分支 |

---

## 9. 变更清单速览

- **新增文件（2 个）**
  - `Lucky/Source/Lucky/Scripting/ScriptFieldType.h`（类型枚举 + 类型表 + 查表函数，header-only）
  - `Lucky/Source/Lucky/Scripting/ScriptFieldValue.h`（载荷 variant + 值容器）
- **修改文件（2 个）**
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.h`：mono 前向声明加 `MonoClassField`；include 两个新头与 `Asset/AssetHandle.h`；新增 `ScriptField` 结构；`ScriptClass` 加 `GetFields` / `GetFieldValue` / `SetFieldValue` 与两个私有成员；`ScriptEngine` 加 `ResolveAssetByFieldType` 公开静态声明
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`：补 `attrdefs.h` 与四个资产类型的 include；匿名命名空间加 `ResolveScriptFieldType` / `IsEntityDerivedFieldType` / `TryGetReferenceHandle` / `SetReferenceHandle` / `GetScalarField<T>` / `SetScalarField<T>`；`ResolveAssetByFieldType` 实现为 `ScriptEngine` 公开静态方法；实现三个成员函数
- **删除**：无
- **不改动**：`Scene/Components/ScriptComponent.h`、`ScriptGlue.cpp`、premake 脚本（新头文件在同目录，但**仍需重跑一次 premake**）
- **托管侧（B 组前置，可选）**：`Lucky-ScriptCore` 下新增 `Vector2.cs` / `Vector4.cs` / `Quaternion.cs` / `Color.cs` / `Material.cs` / `Mesh.cs` / `Texture2D.cs` / `Script.cs`，形态见 §3.6
- **测试样本会改动**：`Luck3DApp/Project/Assets/Scripts/PlayerController.cs` 临时加 `Speed` / `LogPosition` / `Title` / `Offset` / `Score` 五个字段（**建议保留**，P2.5 / P2.6 需要）

---

## 10. 参考

- 编码规范 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：§2.1 命名、§3.3 include 顺序、§4.1 XML 注释、§5.2 强制花括号、§5.4 对齐规则、§6.1 class/struct、§8.1 enum class、§9.2 日志必须英文、§13.1 const 正确性、§13.9 auto 使用规范
- 前置详设 [Phase2.3_ScriptComponent_AssetRef.md](Phase2.3_ScriptComponent_AssetRef.md)（`ScriptComponent` 改造；本 Phase 不依赖但顺序上在其后）
- 布局契约 [Phase1.5_ScriptGlue.md](Phase1.5_ScriptGlue.md) 决策点 9（`glm::vec3` 与 C# `Lucky.Vector3` 的内存布局对齐）
- 托管类型 [Phase1.2_ScriptCore_Assembly.md](Phase1.2_ScriptCore_Assembly.md)（`Lucky.Vector3` / `Lucky.Entity` 的定义）
- 脚本系统路线图 [ScriptSystem_Roadmap.md](ScriptSystem_Roadmap.md) 第 4 节 Phase 2「`ScriptEngine` 缓存 `ScriptClass` 元信息（字段名 / 类型 / 默认值）」
- **Unity 官方字段类型规则**：[Script serialization rules](https://docs.unity3d.com/6000.5/Documentation/Manual//script-serialization-rules.html)（本 Phase 的类型集合就是照它裁的：基本类型 / 枚举 ≤32 位 / 内置结构 / `[Serializable]` struct / `UnityEngine.Object` 派生引用 / 上述类型的数组或 List；明确不支持多维数组、交错数组、嵌套容器）
- **Unity 的引用序列化机制**：[Direct reference asset management](https://docs.unity3d.com/6000.0/Documentation/Manual/assets-direct-reference.html)（字段引用 `UnityEngine.Object` 派生对象时，存档写的是 `{fileID, guid}` 指向**磁盘资产文件**，运行时再解析成 `InstanceID` 跟踪活对象 —— 这正是 §6.8 那条"运行时创建的资产不会被存进场景"的依据）
- mono 字段 API（行号已核）：`mono_class_get_fields`(class.h:212) / `mono_field_get_name`(238) / `mono_field_get_type`(241) / `mono_field_get_flags`(247) / `mono_class_get_field_from_name`(94) / `mono_class_from_mono_type`(127) / `mono_class_get_parent`(170) / `mono_class_is_subclass_of`(130) / `mono_type_get_name`(144) / `mono_field_get_parent`(244)、`mono_type_get_type`(metadata.h:344) / `mono_type_get_class`(352)、`mono_field_get_value`(object.h:342) / `mono_field_set_value`(336) / `mono_object_new`(78) / `mono_object_get_class`(209) / `mono_string_new`(148) / `mono_string_to_utf8`(163)、`mono_domain_get`(appdomain.h:74)、`mono_free`(mono-publib.h:104)
- mono 行为实证（GitHub mono/mono `mono/metadata/class.c`）：`mono_class_get_fields` 只枚举本类声明字段（父类需 `mono_class_get_parent` 上溯）；`mono_class_get_field_from_name` 会搜父类（"Search the class klass and its parents"）；`mono_type_get_name` 返回 `g_string_new` 分配的堆字符串、调用方必须释放
