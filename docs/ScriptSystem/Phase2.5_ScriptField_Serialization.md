# Phase 2.5：ScriptComponent 存字段值 + 序列化

## 1. 概述

把 P2.4 产出的 `ScriptFieldValue` 装进 `ScriptComponent`，并且：

1. **脚本首次挂上时，从托管类读出字段的初始值填进 `FieldMap`** —— 这样 Inspector 里看到的就是脚本里写的 `= 3.0f`，而不是 0
2. **脚本字段列表变化时自动同步**（新增字段补默认值、删掉的字段丢弃）
3. **随场景序列化** —— 存盘重开值还在

本 Phase 让 `ScriptComponent` 从"只有一个资产引用"变成"资产引用 + 一份字段值表"。**仍然不画控件**（P2.6 才画），但会在 Inspector 里加**一行**"拖入脚本后同步 FieldMap"的调用，好让本 Phase 能独立验收。

### 1.1 关键约束

- **字段的初始值只能从"真实实例"读，不能用 `mono_class_get_field_default_value`。** 原因很关键：C# 里
  ```csharp
  public float Speed = 3.0f;
  ```
  这个 `= 3.0f` 被编译进**构造函数体**（IL 里的 `stfld`），**不是**字段元数据里的默认值。所以 `mono_class_get_field_default_value` 只会给你 `0`。**必须造一个实例去读。**
- **造临时实例的副作用必须被约定住**：读默认值需要 `ScriptClass::Instantiate()`（它会跑一遍 C# 构造函数），所以：
  - 临时实例**不注册**进 `EntityInstances`
  - 临时实例**不调用** `Awake`
  - 读完立刻让它 `Ref` 析构
  - 这一点必须写在代码注释里 —— 否则以后有人会"顺手"把它注册进去
- **`FieldMap` 的键是字段名（`std::string`），不是索引。** 脚本里插入/删除/重排字段后，旧存档仍能按名字对上；代价是**字段改名会丢值**（Unity 同样如此）。
- **`ScriptFieldType` 必须同时提供 `ToString` 和 `FromString`**，和 P2.1 给 `AssetType` 加两处映射是同一个教训 —— 只写一侧会导致存盘能读、重开丢类型。
- **反序列化时不依赖 `ScriptEngine` 已加载程序集**：读出来只是"一串有类型的值"，不需要解析脚本类。补默认值/清理多余字段交给 `SyncScriptFieldMap`（它才需要程序集，且可能在 Inspector 拖入时或进 Play 前才被调用）。
- **存档里的字段类型与脚本当前类型不符时，以脚本为准**（用默认值覆盖 + 打 WARN）—— 脚本从 `float` 改成 `int` 属于用户改了签名，旧值已经语义无效。
- 代码风格遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：组件是 `struct`（§13.3）、`enum class`（§8.1）、控制语句强制花括号（§5.2）、范围 for 元素用 `auto`（§13.9）。

### 1.2 前置条件

| 依赖 | 说明 |
|------|------|
| P2.3 | `ScriptComponent::ScriptAsset`（`Ref<Script>`）已就位 |
| P2.4 | `ScriptFieldValue.h`、`ScriptClass::GetFields()` / `GetFieldValue` / `SetFieldValue`、`ScriptClass::Instantiate()` 已就位 |
| P2.2 | `ScriptEngine::ResolveScriptClass` 已就位 |
| 序列化 | `ComponentSerializers.cpp` 里 `ScriptComponent` 已有 `Serialize_Script` / `Deserialize_Script`（P2.3 改成 `AssetHandle`） |
| YAML | `YamlHelpers.h` 提供 vec3 等类型的 YAML 转换 |

### 1.3 本 Phase **不做**的事

- 不做 Inspector 的字段控件（P2.6）—— 本 Phase 只在拖入脚本时触发一次同步
- 不做"进 Play 时把值灌进托管对象"（P2.6）
- 不做字段类型扩展（仍是 float / int / bool / Vector3）
- 不做 `[HideInInspector]`

---

## 2. 涉及的文件

### 2.1 新建

无。

### 2.2 修改

| 文件 | 改动 |
|------|------|
| `Lucky/Source/Lucky/Scripting/ScriptFieldValue.h` | 加 `ScriptFieldMap` 别名；加 `ScriptFieldTypeToString` / `StringToScriptFieldType` |
| `Lucky/Source/Lucky/Scene/Components/ScriptComponent.h` | 加 `ScriptFieldMap Fields` 成员 |
| `Lucky/Source/Lucky/Scripting/ScriptEngine.h` | 加 `SyncScriptFieldMap` 声明 |
| `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp` | 加 `SyncScriptFieldMap` 实现 |
| `Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp` | `Serialize_Script` / `Deserialize_Script` 加 `Fields` 段 |
| `Lucky/Source/Lucky/Editor/ComponentInspectors.cpp` | `Draw_Script` 里在脚本被赋值后调用一次同步 |

### 2.3 不修改

- `Asset/*`：不涉及
- `ScriptGlue.cpp`：不涉及
- 托管 C# 代码：不涉及
- `Scene/Scene.cpp`：运行态灌值属 P2.6
- premake：无新增文件

---

## 3. 现状回顾

### 3.1 改动后的 `ScriptComponent`（P2.3 产出）

```cpp
    struct ScriptComponent
    {
        Ref<Script> ScriptAsset;

        ScriptComponent() = default;
        ScriptComponent(const ScriptComponent& other) = default;
        ScriptComponent(const Ref<Script>& scriptAsset)
            : ScriptAsset(scriptAsset) {}
    };
```

### 3.2 `ScriptFieldValue` 与相关 API（P2.4 产出）

```cpp
    enum class ScriptFieldType : uint8_t
    {
        None = 0,
        Float,
        Int,
        Bool,
        Vector3
    };

    struct ScriptFieldValue
    {
        ScriptFieldType Type = ScriptFieldType::None;
        float FloatValue = 0.0f;
        int IntValue = 0;
        bool BoolValue = false;
        glm::vec3 Vector3Value = glm::vec3(0.0f);
    };
```

```cpp
        const std::vector<ScriptField>& GetFields();

        bool GetFieldValue(MonoObject* instance, const ScriptField& field, ScriptFieldValue& outValue) const;

        bool SetFieldValue(MonoObject* instance, const ScriptField& field, const ScriptFieldValue& value) const;
```

### 3.3 `Instantiate()` 会跑到哪个构造函数

```cpp
    MonoObject* ScriptClass::Instantiate()
    {
        return ScriptEngine::InstantiateClass(m_MonoClass);
    }

    MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass)
    {
        MonoObject* instance = mono_object_new(s_Data->AppDomain, monoClass);
        mono_runtime_object_init(instance);
        return instance;
    }
```

`mono_runtime_object_init` 调的是**无参构造**。对 `PlayerController : Entity`，链上会调到 `Lucky.Entity()`（`protected Entity() { ID = 0; }`）—— **不需要实体 UUID**，所以"造临时实例读默认值"这条路是通的。

`ScriptInstance` 的构造里额外做的事（调 `Entity(ulong)` 把 UUID 写进 `ID`、注册实例、调 `Awake`）**都不在本 Phase 的临时实例路径上**。

### 3.4 `ScriptComponent` 的序列化现状（P2.3 产出）

```cpp
        void Serialize_Script(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<ScriptComponent>())
            {
                return;
            }
            const ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

            out << YAML::Key << "ScriptComponent";
            out << YAML::BeginMap;
            if (sc.ScriptAsset)
            {
                out << YAML::Key << "AssetHandle" << YAML::Value << sc.ScriptAsset->GetHandle();
            }
            else
            {
                out << YAML::Key << "AssetHandle" << YAML::Value << static_cast<uint64_t>(0);
            }
            out << YAML::EndMap;
        }
```

### 3.5 `AssetType` 的 `ToString` / `FromString` 范式（本 Phase 要照抄给 `ScriptFieldType`）

```cpp
    inline const char* AssetTypeToString(AssetType type)
    {
        switch (type)
        {
            case AssetType::Material:   return "Material";
            // ...
            default:                    return "None";
        }
    }

    inline AssetType StringToAssetType(const std::string& str)
    {
        if (str == "Material")  return AssetType::Material;
        // ...
        return AssetType::None;
    }
```

### 3.6 场景里 `ScriptComponent` 段落的目标形态（本 Phase 之后）

```yaml
    ScriptComponent:
      AssetHandle: 12345678901234567890
      Fields:
        Speed:
          Type: Float
          FloatValue: 3.0
        LogPosition:
          Type: Bool
          BoolValue: false
```

**刻意做成"可读的键值对"**，方便你直接手改存档调试（也符合项目"YAML 存档 Git 友好"的既定取向）。

---

## 4. 关键设计决策（多方案对比）

### 4.1 决策点 1：字段初始值怎么取得

#### 方案 A：脚本被赋值的瞬间，造一个临时实例读全部字段（**推荐 ✅**）

```cpp
MonoObject* tempInstance = scriptClass->Instantiate();
// 对每个字段调 GetFieldValue(tempInstance, field, outValue)
// tempInstance 随 Ref 析构被回收
```

- **优点**：
  1. **Inspector 里立刻显示脚本里写的初值**（`Speed = 3.0`），和 Unity 的行为一致
  2. 一次遍历把所有字段读完，代价可控（只在"拖入脚本"和"脚本字段变了"时发生）
  3. 不需要在运行时另开一条"回填"路径
- **缺点**：
  1. **会执行脚本的 C# 构造函数** —— 如果用户在构造函数里写了副作用（罕见且不推荐），会被执行一次。**必须用注释把"临时实例不注册、不 Awake、读完即弃"的约定写死**
  2. 依赖 mono 已初始化（`ResolveScriptClass` 已经依赖这一条，无额外新增）

#### 方案 B：用 `mono_class_get_field_default_value` 读元数据默认值

- **优点**：不造实例，零副作用
- **缺点**：**根本读不到 C# 字段初始化器的值**。`public float Speed = 3.0f;` 编译进构造函数，元数据默认值是 `0`。用户会看到 Inspector 里显示 `0` 而脚本里明明写着 `3.0f` —— 典型的"看起来对了其实全错"。**否决。**

#### 方案 C：`FieldMap` 只存"用户改过的值"，缺省时 Inspector 显示类型零值

- **优点**：零副作用，实现最简
- **缺点**：
  1. 编辑态看到的是 `0`，而不是脚本里的初值 —— 用户无法判断"到底生效了没有"
  2. 存档里没有"脚本初值"这个基准，一旦字段被改过就无法回到初值
  3. 与 Unity 的体验背离，违背"对齐 Unity 语义"的既定方向
- **次优**：如果实测发现"临时实例副作用"是真实痛点，可以退到这里，但要接受上面三条代价

#### 方案 D：首次进 Play 时读真实实例的初值再回填到 `FieldMap`

- **优点**：用的是真实例，无"临时"概念
- **缺点**：
  1. **编辑态永远看不到值**（只有进过一次 Play 才有），而编辑态恰恰是最需要看到默认值的时候
  2. 进 Play 时 `Scene::Copy` 已经发生，回填要写到哪一份场景上？（运行态副本丢弃后回填就没了）
  3. 引入了"编辑态/运行态数据不一致"的时序陷阱
- **否决。**

**结论：方案 A。** 把副作用约定写进注释和验收标准。

---

### 4.2 决策点 2：`FieldMap` 的键用什么

#### 方案 A：字段名 `std::string`（**推荐 ✅**）

- **优点**：
  1. 脚本里插入/删除/重排字段后，旧存档仍能按名字对上正确字段
  2. 存档文本可读（`Speed: { Type: Float, ... }`），能手改、能 diff
  3. Unity 就是这么做的，用户预期一致
- **缺点**：**字段改名 = 丢值**（`Speed` → `MoveSpeed` 后存档里的 `Speed` 变成"多余字段"被清理，`MoveSpeed` 用默认值）。可接受，且 `SyncScriptFieldMap` 会打日志说明

#### 方案 B：字段索引 `size_t`

- **优点**：内存小、查得快
- **缺点**：**脚本里插一个字段，后面所有字段的值全部错位** —— 这是灾难性的静默数据损坏。**否决。**

#### 方案 C：`MonoClassField*` 指针

- **优点**：唯一、精确
- **缺点**：指针在程序集重载后会失效，**存档根本存不下来**（没法序列化一个进程内的指针）。**否决。**

**结论：方案 A。**

---

### 4.3 决策点 3：`ScriptFieldMap` 类型别名放哪

`FieldMap` 的类型是 `std::unordered_map<std::string, ScriptFieldValue>`。别名定义在哪，决定了谁要依赖谁。

#### 方案 A：放在 `Scripting/ScriptFieldValue.h`（**推荐 ✅**）

```cpp
using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldValue>;
```

- **优点**：
  1. `ScriptEngine.h`（要用它当 `SyncScriptFieldMap` 的参数）和 `ScriptComponent.h`（要用它当成员）都**只依赖这个轻头**，不需要互相包含
  2. 类型别名跟它的元素类型放在一起，概念内聚
  3. `ScriptFieldValue.h` 因此要新增 `#include <string>` 与 `#include <unordered_map>` —— 代价很小（本来就是数据头）
- **缺点**：`ScriptFieldValue.h` 从"只有两个定义"变成"有两个定义 + 两个转换函数 + 一个别名"，稍胖

#### 方案 B：放在 `ScriptComponent.h`

- **优点**：用的人就在这儿
- **缺点**：`ScriptEngine.h` 要用它，就得 include `Scene/Components/ScriptComponent.h` —— `Scripting → Scene/Components` 这个方向虽然技术上可行（`ScriptEngine.h` 已经 include 了 `Scene/Entity.h`），但**为了一个类型别名去依赖一个组件头**，耦合得不值。**次优。**

#### 方案 C：在 `ScriptEngine.h` 和 `ScriptComponent.h` 各写一份别名

- **缺点**：两份定义必须逐字一致，改名时容易漏一处；两个不同的别名指向同一类型，读代码的人会以为是两种东西。**否决。**

**结论：方案 A。**

---

### 4.4 决策点 4：同步逻辑做成几个接口

"同步"要做三件事：① 缺失字段补默认值 ② 类型不符的字段用默认值覆盖 ③ 多余的键删掉。

#### 方案 A：一个入口 `SyncScriptFieldMap`，内部按需实例化（**推荐 ✅**）

```cpp
static void SyncScriptFieldMap(const Ref<Script>& scriptAsset, ScriptFieldMap& fieldMap);
```

内部逻辑：先解析类拿字段列表 → 检查是否真的缺字段 → **只有需要读默认值时才造临时实例** → 补齐/覆盖/清理。

- **优点**：
  1. 调用方只有一句 `SyncScriptFieldMap(sc.ScriptAsset, sc.Fields);`，不需要知道内部要不要实例化
  2. "按需实例化"这个优化藏在里面，调用方无需关心
  3. 一个入口 = 一处维护，避免"两个接口调用顺序错了"的问题
- **缺点**：函数内部有分支（"要不要造实例"），比"直接造"稍复杂 —— 但换来"字段没变时不跑构造函数"，值得

#### 方案 B：拆成 `FillFieldMapDefaults` + `PruneFieldMap` 两个接口

- **优点**：各自单一职责
- **缺点**：**调用方必须两个都调，且顺序有讲究**（先 prune 再 fill，否则刚补的默认值会被 prune 误删？不会，但顺序依赖本身就是坑）。**次优。**

#### 方案 C：同步逻辑放到 `ScriptComponent` 自己的方法里

- **优点**：`sc.Sync();` 读起来最顺
- **缺点**：`ScriptComponent` 就得 include `ScriptEngine.h`（要用 `ResolveScriptClass` / `GetFields` / `Instantiate`），把脚本运行时整条链拖进组件头 —— **直接破坏 P2.4 决策点 4.3 的努力**。**否决。**

**结论：方案 A。**

---

### 4.5 决策点 5：`Fields` 段在 YAML 里怎么写

#### 方案 A：可读的键值对（**推荐 ✅**）

```yaml
      Fields:
        Speed:
          Type: Float
          FloatValue: 3.0
        LogPosition:
          Type: Bool
          BoolValue: false
```

- **优点**：
  1. 直接手改存档就能试数值，不用开编辑器
  2. `Type` 显式写出来 —— 反序列化时**不依赖程序集**也能正确构造 `ScriptFieldValue`
  3. Git diff 可读（改一个值只动一行）
- **缺点**：比紧凑格式啰嗦（每个字段 3 行）

#### 方案 B：紧凑的序列形式

```yaml
      Fields:
        - Name: Speed
          Type: Float
          FloatValue: 3.0
```

- **优点**：顺序稳定（按写入顺序）
- **缺点**：**反序列化时要遍历查找**，而且"键是名字"这个设计的信息被藏起来了（读存档的人看不出这是按名字索引的）。**不如 A。**

#### 方案 C：只存"值"，类型靠 `ScriptFieldType` 推断

```yaml
      Fields:
        Speed: 3.0
```

- **优点**：最简洁
- **缺点**：**类型信息丢了** —— `3` 是 `Int` 还是 `Float`？`[0,0,0]` 是 `Vector3` 还是别的？反序列化时必须依赖程序集去查脚本的字段类型，这会让"反序列化"和"脚本能不能解析"绑死（约束 1.1 明确要避免）。**否决。**

**结论：方案 A。**

---

### 4.6 决策点 6：存档里的类型与脚本当前类型不符时怎么办

场景：脚本里 `public float Speed = 3.0f;` 改成了 `public int Speed = 3;`，存档里还是 `Type: Float, FloatValue: 3.0`。

#### 方案 A：以脚本为准，用字段的当前默认值覆盖 + 打 WARN（**推荐 ✅**）

- **优点**：
  1. 结果确定：Inspector 显示的就是脚本当前的真实初值
  2. 一条 WARN 交代清楚"这个字段的值因为类型变了被重置了"
  3. 不会把 `FloatValue=3.0` 硬塞进 `int` 字段（那是未定义行为级别的错配）
- **缺点**：用户丢了一次手调的值。但**类型都改了，旧值本来就没有意义了**

#### 方案 B：以存档为准，尝试做类型转换

- **优点**：尽量不丢数据
- **缺点**：需要写 `float↔int↔bool↔Vector3` 的转换矩阵（12 条规则），且"`Vector3` 转 `int`"这种事没有合理语义。**收益远小于复杂度，否决。**

#### 方案 C：不处理，直接按存档的类型写进去

- **优点**：零代码
- **缺点**：`SetFieldValue` 会按 `value.Type` 去写 mono 字段 —— 而那个字段的实际类型是 `int`，**写入 `float` 的 4 字节虽然宽度一样，但位模式完全不同**（`3.0f` 的位模式被解释成 `1077936128`）。这是**静默的数据损坏**。**必须否决。**

**结论：方案 A**，由 `SyncScriptFieldMap` 负责这件事。

---

### 4.7 决策点 7：什么时候调用 `SyncScriptFieldMap`

#### 方案 A：Inspector 里"脚本资产被赋值"的那一帧调用一次（**推荐 ✅**）

```cpp
if (UI::PropertyAsset("Script", sc.ScriptAsset))
{
    ScriptEngine::SyncScriptFieldMap(sc.ScriptAsset, sc.Fields);
}
```

- **优点**：
  1. `UI::PropertyAsset` 的返回值正好是"被修改了"，天然就是触发条件，不需要额外的脏标记
  2. 拖入脚本的**那一刻**就填好默认值，用户立刻能看到
  3. 本 Phase 就能独立验收（拖入 → 存档 → 重开，值还在），不用等 P2.6
- **缺点**：如果脚本字段在外部改了但没重新拖入，FieldMap 不会自动更新 —— 这种情况由"下次拖入/下次 Play 前"的同步补上（P2.6 会在进 Play 前再同步一次）

#### 方案 B：只在进 Play 前同步

- **优点**：逻辑集中在一处
- **缺点**：**编辑态看不到值**，本 Phase 无法独立验收，且用户拖入后拖了个寂寞。**次优。**

#### 方案 C：每帧在 `Draw_Script` 里都同步

- **优点**：永远最新
- **缺点**：每帧都要解析类 + 遍历字段，**且一旦有字段缺失就会每帧造一个临时实例**（跑一遍 C# 构造函数）—— 这是不可接受的浪费。**否决。**

**结论：方案 A。** 并在 §8 接线点注明 P2.6 要在"进 Play 前"再同步一次作为兜底。

---

## 5. 实现步骤（按依赖顺序）

每步完成后 `Lucky` 与 `Luck3DApp` 都应能编译通过。

### Step 1：`ScriptFieldValue.h` 加别名与类型转换

**文件**：`Lucky/Source/Lucky/Scripting/ScriptFieldValue.h`

**1) 补 include**：

```cpp
#include <glm/glm.hpp>

#include <string>
#include <unordered_map>
```

**2) 文件末尾（`namespace Lucky` 内、结构体之后）加设备别名与两个转换函数**：

```cpp
    /// <summary>
    /// 脚本字段值表：键为字段名
    /// 用名字而不是索引，脚本里插入/删除/重排字段后旧数据仍能对上；代价是字段改名会丢值
    /// </summary>
    using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldValue>;

    /// <summary>
    /// ScriptFieldType 转字符串（序列化用）
    /// </summary>
    inline const char* ScriptFieldTypeToString(ScriptFieldType type)
    {
        switch (type)
        {
            case ScriptFieldType::Float:    return "Float";
            case ScriptFieldType::Int:      return "Int";
            case ScriptFieldType::Bool:     return "Bool";
            case ScriptFieldType::Vector3:  return "Vector3";
            default:                        return "None";
        }
    }

    /// <summary>
    /// 字符串转 ScriptFieldType（反序列化用）
    /// </summary>
    inline ScriptFieldType StringToScriptFieldType(const std::string& str)
    {
        if (str == "Float")     return ScriptFieldType::Float;
        if (str == "Int")       return ScriptFieldType::Int;
        if (str == "Bool")      return ScriptFieldType::Bool;
        if (str == "Vector3")   return ScriptFieldType::Vector3;
        return ScriptFieldType::None;
    }
```

**要点**：

- 完全照抄 `AssetType.h` 里 `AssetTypeToString` / `StringToAssetType` 的形态（§3.5）—— `inline` 自由函数、`switch` 带 `default`、字符串精确匹配
- **两个函数必须同时存在**。只写 `ToString` 的话，存盘正常、重开时类型全变 `None` —— 这个坑 P2.1 已经踩过一次（`AssetType`）
- 别名 `ScriptFieldMap` 与结构体放在同一个头，让 `ScriptComponent.h` 和 `ScriptEngine.h` 都能只依赖这个轻头（决策点 4.3）

### Step 2：`ScriptComponent.h` 加 `Fields`

**文件**：`Lucky/Source/Lucky/Scene/Components/ScriptComponent.h`

```cpp
#pragma once

#include "Lucky/Scripting/ScriptFieldValue.h"

#include "Lucky/Asset/Script.h"
#include "Lucky/Core/Base.h"

namespace Lucky
{
    /// <summary>
    /// 脚本组件：把用户 C# 脚本挂到实体上
    /// 保存脚本资产引用与该脚本各字段的取值；托管对象本身由 ScriptEngine 统一管理
    /// </summary>
    struct ScriptComponent
    {
        Ref<Script> ScriptAsset;

        ScriptFieldMap Fields;

        ScriptComponent() = default;
        ScriptComponent(const ScriptComponent& other) = default;
        ScriptComponent(const Ref<Script>& scriptAsset)
            : ScriptAsset(scriptAsset) {}
    };
}
```

**要点**：

- `ScriptFieldMap` 的 `unordered_map` 值语义完整，`= default` 拷贝构造天然正确 —— 不需要写自定义拷贝逻辑
- **不要**把 `Fields` 做成 `ScriptFieldMap*` 之类的间接层：组件是值语义（`Scene::Copy` 按值拷贝），间接层会破坏"副本与源不共享组件存储"这条契约
- 成员顺序：把"引用"和"数据"分开成两块、中间留空行，比挤在一起可读

### Step 3：`ScriptEngine` 加 `SyncScriptFieldMap`

**文件 A**：`Lucky/Source/Lucky/Scripting/ScriptEngine.h`

在 `ResolveScriptClass` 声明**之后**加：

```cpp
        /// <summary>
        /// 把脚本字段表与脚本类的当前字段列表对齐
        /// - 类里新增的字段：读取其初始值补进 fieldMap
        /// - 已存在但类型与脚本不符的字段：用初始值覆盖（脚本改了字段类型时的必然结果）
        /// - fieldMap 里多出来的字段（脚本已删）：移除
        /// 只有在真的需要读初始值时才创建临时托管对象；该对象不注册、不调用 Awake
        /// </summary>
        /// <param name="scriptAsset">脚本资产（为空时清空 fieldMap）</param>
        /// <param name="fieldMap">待对齐的字段表（原地修改）</param>
        static void SyncScriptFieldMap(const Ref<Script>& scriptAsset, ScriptFieldMap& fieldMap);
```

**文件 B**：`Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`

加在 `ResolveScriptClass` 实现**之后**：

```cpp
    void ScriptEngine::SyncScriptFieldMap(const Ref<Script>& scriptAsset, ScriptFieldMap& fieldMap)
    {
        if (!scriptAsset)
        {
            fieldMap.clear();
            return;
        }

        Ref<ScriptClass> scriptClass = ResolveScriptClass(scriptAsset->GetClassName());
        if (!scriptClass)
        {
            // ResolveScriptClass 已经报过错；保留 fieldMap 现有内容，避免用户在脚本没编译时丢掉已调的值
            return;
        }

        const std::vector<ScriptField>& fields = scriptClass->GetFields();

        // ---- 第一遍：判断是否需要读初始值 ----
        bool needsDefaults = false;
        for (const ScriptField& field : fields)
        {
            auto it = fieldMap.find(field.Name);
            if (it == fieldMap.end() || it->second.Type != field.Type)
            {
                needsDefaults = true;
                break;
            }
        }

        if (needsDefaults)
        {
            // 临时实例：仅用于读取字段初始值，不注册进 EntityInstances、不调用 Awake、读完即弃
            MonoObject* tempInstance = scriptClass->Instantiate();
            if (tempInstance)
            {
                for (const ScriptField& field : fields)
                {
                    auto it = fieldMap.find(field.Name);
                    const bool typeMismatch = (it != fieldMap.end() && it->second.Type != field.Type);

                    if (typeMismatch)
                    {
                        LF_CORE_WARN("ScriptEngine::SyncScriptFieldMap - 字段 '{0}.{1}' 的类型已从存档类型变为脚本类型，值被重置为脚本初始值", scriptClass->GetName(), field.Name);
                    }

                    if (it == fieldMap.end() || typeMismatch)
                    {
                        ScriptFieldValue value;
                        if (scriptClass->GetFieldValue(tempInstance, field, value))
                        {
                            fieldMap[field.Name] = value;
                        }
                    }
                }
            }
        }

        // ---- 第二遍：清掉脚本里已经不存在的字段 ----
        for (auto it = fieldMap.begin(); it != fieldMap.end();)
        {
            const bool stillExists = std::any_of(fields.begin(), fields.end(),
                [&it](const ScriptField& field) { return field.Name == it->first; });

            if (stillExists)
            {
                ++it;
            }
            else
            {
                LF_CORE_WARN("ScriptEngine::SyncScriptFieldMap - 字段 '{0}' 在脚本中已不存在，项目值被移除", it->first);
                it = fieldMap.erase(it);
            }
        }
    }
```

**要点**：

- **两遍扫描**：第一遍只判断"要不要读初始值"（不实例化），第二遍清理多余字段。**这样"字段没变"这个常见情况下完全不造实例**（决策点 4.4-A 的核心收益）
- **`ResolveScriptClass` 失败时保留 `fieldMap` 不动**：用户在脚本没编译/改名期间，之前手调过的值不该被抹掉。这是刻意与"scriptAsset 为空就 clear"区别对待的 —— 前者是"脚本没了"，后者是"暂时解析不出来"
- **临时实例没有用 `Ref` 管起来**：`Instantiate()` 返回的是裸 `MonoObject*`，它的生命周期由 mono 的 GC 管；我们只是不再引用它。**不要**为了"看起来干净"去包一个智能指针 —— mono 对象不是 C++ `new` 出来的，析构语义完全不同
- **`needsDefaults` 时 `tempInstance` 可能为 nullptr**（mono 侧分配失败）：已经是"读不到默认值就跳过"，不会崩
- `std::any_of` 需要 `<algorithm>`（PCH 里通常已有）；若报未定义就补
- **清理阶段打 WARN 而不是静默**：用户把字段改了名，值丢了这件事要有痕迹

### Step 4：序列化 `Fields`

**文件**：`Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp`

**1) `Serialize_Script` 在 `AssetHandle` 之后加 `Fields` 段**：

```cpp
        void Serialize_Script(YAML::Emitter& out, Entity entity)
        {
            if (!entity.HasComponent<ScriptComponent>())
            {
                return;
            }
            const ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

            out << YAML::Key << "ScriptComponent";
            out << YAML::BeginMap;
            if (sc.ScriptAsset)
            {
                out << YAML::Key << "AssetHandle" << YAML::Value << sc.ScriptAsset->GetHandle();
            }
            else
            {
                out << YAML::Key << "AssetHandle" << YAML::Value << static_cast<uint64_t>(0);
            }

            out << YAML::Key << "Fields" << YAML::Value << YAML::BeginMap;
            for (const auto& [fieldName, fieldValue] : sc.Fields)
            {
                out << YAML::Key << fieldName;
                out << YAML::BeginMap;
                out << YAML::Key << "Type" << YAML::Value << ScriptFieldTypeToString(fieldValue.Type);

                switch (fieldValue.Type)
                {
                    case ScriptFieldType::Float:
                    {
                        out << YAML::Key << "FloatValue" << YAML::Value << fieldValue.FloatValue;
                        break;
                    }
                    case ScriptFieldType::Int:
                    {
                        out << YAML::Key << "IntValue" << YAML::Value << fieldValue.IntValue;
                        break;
                    }
                    case ScriptFieldType::Bool:
                    {
                        out << YAML::Key << "BoolValue" << YAML::Value << fieldValue.BoolValue;
                        break;
                    }
                    case ScriptFieldType::Vector3:
                    {
                        out << YAML::Key << "Vector3Value" << YAML::Value << fieldValue.Vector3Value;
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }

                out << YAML::EndMap;
            }
            out << YAML::EndMap;

            out << YAML::EndMap;
        }
```

**2) `Deserialize_Script` 在读完 `AssetHandle` 之后加 `Fields` 段**：

```cpp
            YAML::Node fieldsNode = node["Fields"];
            if (fieldsNode && fieldsNode.IsMap())
            {
                for (auto fieldNode : fieldsNode)
                {
                    const std::string fieldName = fieldNode.first.as<std::string>();
                    YAML::Node valueNode = fieldNode.second;
                    if (!valueNode || !valueNode["Type"])
                    {
                        continue;
                    }

                    ScriptFieldValue fieldValue;
                    fieldValue.Type = StringToScriptFieldType(valueNode["Type"].as<std::string>(""));

                    switch (fieldValue.Type)
                    {
                        case ScriptFieldType::Float:
                        {
                            fieldValue.FloatValue = valueNode["FloatValue"].as<float>(0.0f);
                            break;
                        }
                        case ScriptFieldType::Int:
                        {
                            fieldValue.IntValue = valueNode["IntValue"].as<int>(0);
                            break;
                        }
                        case ScriptFieldType::Bool:
                        {
                            fieldValue.BoolValue = valueNode["BoolValue"].as<bool>(false);
                            break;
                        }
                        case ScriptFieldType::Vector3:
                        {
                            fieldValue.Vector3Value = valueNode["Vector3Value"].as<glm::vec3>(glm::vec3(0.0f));
                            break;
                        }
                        default:
                        {
                            // 未知类型：跳过该项，保留 Type 为 None 的占位
                            continue;
                        }
                    }

                    sc.Fields[fieldName] = fieldValue;
                }
            }
```

**要点**：

- **两个 `switch` 必须覆盖同一组类型**。以后加类型时，`ScriptFieldValue.h` 的枚举、`Serialize_Script`、`Deserialize_Script`、P2.6 的控件**四处都要加** —— 这是这套扁平结构（P2.4 决策点 4.1-A）的固定代价，**值得在文档里点明**
- `valueNode["..."].as<T>(默认值)` **一律给默认值**：存档可能被手改坏、可能是旧版本写的，缺失键不该崩
- `glm::vec3` 的 YAML 转换来自 `Serialization/YamlHelpers.h`（材质/Transform 已在用）。若该头没被 `ComponentSerializers.cpp` 包含，需要补 include
- `fieldNode.first.as<std::string>()` 是 yaml-cpp 遍历 map 的标准写法
- **反序列化不解析脚本类**（约束 1.1）：这里只是把"一串有类型的值"读出来。字段是否还在、类型是否还对，交给 `SyncScriptFieldMap`

### Step 5：Inspector 里在脚本被赋值后同步

**文件**：`Lucky/Source/Lucky/Editor/ComponentInspectors.cpp`

```cpp
        void Draw_Script(Entity entity)
        {
            ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

            if (UI::PropertyAsset("Script", sc.ScriptAsset))
            {
                // 脚本被赋值/更换：按脚本当前的字段列表重建字段表（含初始值）
                ScriptEngine::SyncScriptFieldMap(sc.ScriptAsset, sc.Fields);
            }

            if (sc.ScriptAsset && !ScriptEngine::ResolveScriptClass(sc.ScriptAsset->GetClassName()))
            {
                ImGui::TextColored({0.9f, 0.35f, 0.35f, 1.0f}, "脚本类未找到：请确认该脚本已参与编译，且类名与文件名一致");
            }
        }
```

**要点**：

- `UI::PropertyAsset` 的 `bool` 返回值在这里**必须用上**（P2.3 里是忽略的）—— 它正好表达"脚本引用被改动"，是同步的天然触发条件（决策点 4.7-A）
- 同步放在**赋值分支内**，不要放在 `if` 外面 —— 否则每帧都同步（决策点 4.7-C 的教训）
- 拖入空（把脚本清掉）时 `scriptAsset` 为空 → `SyncScriptFieldMap` 内部会 `fieldMap.clear()`，符合语义
- 本 Phase 仍不需要新增字段控件（P2.6 做），所以 `Draw_Script` 到此为止

### Step 6：编译验证

- Debug 编译 `Lucky` → 通过
- Debug 编译 `Luck3DApp` → 通过
- 不需要重跑 premake（无新增文件）
- **若报 `ScriptFieldMap` 未定义**：`ScriptComponent.h` / `ScriptEngine.h` 没有 include `Lucky/Scripting/ScriptFieldValue.h`
- **若报 `std::any_of` 未定义**：`ScriptEngine.cpp` 补 `#include <algorithm>`

---

## 6. 疑点问答

### 6.1 为什么"临时实例"不用智能指针管起来？

因为 mono 对象不是 `new` 出来的，是 `mono_object_new` 在 GC 堆上分配的。它的生命周期由 mono 的 GC 决定 —— 我们在 native 侧不再持有引用之后，GC 会在合适时机回收。用一个 C++ 智能指针去"析构"它会直接崩。

所以 `SyncScriptFieldMap` 里 `tempInstance` 就是一个裸指针，函数返回后自然失效。**不要**为了"看起来 RAII"去包装它。

### 6.2 为什么 `ResolveScriptClass` 失败时保留 `fieldMap`，而 `scriptAsset` 为空时清空？

两者的语义完全不同：

| 情况 | 语义 | 处理 |
|------|------|------|
| `scriptAsset` 为空 | 用户**主动**把脚本摘掉了，这个实体不再有脚本 | 清空 —— 留着无主的值没有意义 |
| `ResolveScriptClass` 失败 | 脚本资产还在，只是**暂时**解析不出类（没编译 / 改了名） | 保留 —— 用户手调过的值不该因为"忘了编译"被抹掉 |

第二条在实操里很常见：拖入脚本、调数值、改脚本、**忘了重新编译**、回到编辑器 —— 如果这时清空，用户会莫名其妙丢一堆调好的数。

### 6.3 脚本改字段类型后，我在 Inspector 调过的值去哪了？

被脚本的初始值覆盖了（决策点 4.6-A），并且日志里有：

```
ScriptEngine::SyncScriptFieldMap - 字段 'PlayerController.Speed' 的类型已从存档类型变为脚本类型，值被重置为脚本初始值
```

这是**有意的**：类型都改了，旧值不可能还有意义（`float 3.0` 变成 `int` 该是多少？）。而且不覆盖的话就只能"按存档类型硬写"，那会把 `3.0f` 的位模式塞进 `int` 字段，得到 `1077936128` —— 静默的数据损坏，比丢值糟得多。

### 6.4 同名字段在两个脚本之间切换会串值吗？

**不会串到错误的字段，但会保留同名值**。场景：从脚本 A（有 `Speed`）换成脚本 B（也有 `Speed`，但语义不同）。

`SyncScriptFieldMap` 的逻辑是"名字匹配且类型一致 → 保留"。所以 `Speed: Float` 会被保留下来，不会重置为 B 的初值。

**这是有意还是无意？** 当前实现是"有意保留"，理由：

- 用户换脚本时，同名同类型的字段**大概率是同一个意图**（比如把 `PlayerController` 换成 `PlayerControllerV2`）
- 如果强行重置，用户换脚本就得重新调一遍所有数值

**但要接受的风险**：如果 B 的 `Speed` 语义完全不同（比如 A 是像素/秒、B 是米/秒），用户会看到"一个看起来像自己调过的、其实语义不对的值"。这个取舍 Unity 也是保留的。**如果实测觉得危险，改动很小**：把 `SyncScriptFieldMap` 的触发条件从"赋值时同步"改成"赋值时先 `fieldMap.clear()` 再同步"（即换脚本即重置）—— 一行的事，但会牺牲"换脚本保留数值"的便利。

### 6.5 `Fields` 为空时会不会写出多余的 YAML？

会写出一个空的 `Fields: {}`。这是可接受的：

- 空 map 在 yaml-cpp 里输出成 `{}`，读回时 `IsMap()` 为真但遍历为零次
- 相比"判断非空才写"，少一个分支、少一处可能写错的地方
- 存档里多 1 行，代价可以忽略

### 6.6 每帧 Inspector 都会画，`SyncScriptFieldMap` 会不会被重复调用？

**不会**。它只在 `UI::PropertyAsset` 返回 `true`（即"这一帧脚本引用被改动"）时调用。拖拽只在鼠标释放的那一帧返回 `true`，所以一次拖拽只同步一次。

### 6.7 反序列化时 `fieldsNode` 里的类型是未知字符串（比如手改存档写错了）会怎样？

`StringToScriptFieldType` 返回 `ScriptFieldType::None` → `switch` 落到 `default` → `continue` 跳过该项。**不会崩**，该字段会保持"不存在"状态，随后 `SyncScriptFieldMap` 会用脚本初始值把它补上。**这正是"反序列化不依赖程序集、同步负责兜底"这套分工的好处。**

---

## 7. 验收标准

1. **编译通过**：`Lucky` 与 `Luck3DApp` 的 Debug / Release / Dist 三个 configuration 全通过
2. **初始值正确填充**：`PlayerController.cs` 里 `public float Speed = 3.0f;`，拖入 Script 字段后，存档里出现 `Speed: { Type: Float, FloatValue: 3.0 }` —— **不是 0**（这是决策点 4.1-A 的验证点）
3. **不入实例表**：拖入脚本后立刻 Play/Stop，行为与 P2.4 之前一致；`EntityInstances` 里**没有**因为"临时实例"而多出条目（可在 `OnCreateEntityScript` 断点核对数量）
4. **存盘重开保留**：保存场景 → 关闭编辑器 → 重开 → 打开场景 → `Speed` 仍是 `3.0`
5. **手改存档生效**：直接编辑 `.luck3d` 把 `FloatValue: 3.0` 改成 `7.5` → 重开场景 → 拖一次脚本或（P2.6 之后）进 Play，脚本读到的应是新值
6. **新字段自动补齐**：给脚本加 `public bool LogPosition = false;` → 重新编译 → 重开编辑器 → 打开场景 → 存档里出现 `LogPosition`
7. **删字段自动清理**：把 `LogPosition` 从脚本删掉 → 重新编译 → 重开 → 打开场景 → 存档里 `LogPosition` 消失，且日志有 `在脚本中已不存在` 的 WARN
8. **类型变更被重置**：把 `Speed` 改成 `public int Speed = 3;` → 重新编译 → 重开 → 打开场景 → 该字段变成 `Type: Int, IntValue: 3`，日志有 `类型已从存档类型变为脚本类型`
9. **不编译时不丢值**：把 `PlayerController.cs` 改名（不编译）→ 重开编辑器 → 打开场景 → `Fields` 里的值**仍然保留**（验证 6.2 的分支），Inspector 显示红色提示
10. **摘掉脚本即清空**：把 Script 字段拖成 `None (Script)` → 存档里 `AssetHandle: 0` 且 `Fields` 为空
11. **无崩溃无 ERROR**：全流程日志里没有 ERROR，没有崩溃
12. **代码规范**：通过人工 checklist —— 组件是 `struct`（§13.3）；控制语句全带花括号（§5.2）；`enum class`（§8.1）；范围 for 用 `auto`、`switch` 各 case 带花括号；公有接口有 `/// <summary>` 中文注释（§4.1）；无"为对齐而对齐"的空格（§5.4）；无引用外部文档的注释、无"P2.6 会用到"这类阶段性注释

> ⚠️ **第 2 条是这一整步的"生死线"**：如果 `Speed` 填出来是 `0`，说明默认值读的是元数据而不是实例（决策点 4.1-B 的陷阱），或者 `GetFieldValue` 传错了实例。
>
> ⚠️ **第 7、8、9 条是最容易被漏测的三条**，但它们恰好覆盖了 `SyncScriptFieldMap` 的三个分支（清理 / 类型覆盖 / 解析失败保留）。只测第 2、4 条的话，这两个分支的代码等于没验过。
>
> ⚠️ **第 5 条要等 P2.6 才能完整验证**（"脚本读到的值"需要运行态灌值）。本 Phase 只要确认"重开场景后 Inspector / 存档里的值变了"即可，把"脚本真的读到"留到 P2.6。

---

## 8. 对下一 Phase 的接线点

| 位置 | 后续 Phase | 打开方式 |
|------|-----------|---------|
| `ScriptComponent::Fields` | **P2.6** | 在旁边画字段控件（每个字段一个控件，写回 `Fields`） |
| `ScriptEngine::SyncScriptFieldMap` | **P2.6** | 进 Play 前（`Scene::OnRuntimeStart` 里）再同步一次作为兜底：脚本字段在外部改过、但用户没重新拖入的情况 |
| `ScriptClass::SetFieldValue` | **P2.6** | 在 `ScriptInstance` 构造完成后、`InvokeAwake()` **之前**，遍历 `Fields` 逐个灌入 |
| `ScriptFieldTypeToString` / `StringToScriptFieldType` | 类型扩展 | 加新字段类型时，这两处 + 枚举 + 序列化 switch + P2.6 控件，共五处 |
| `Fields` 为空时写出 `{}` 的行为 | 存档格式演进 | 若将来要兼容"没有 Fields 段的旧存档"，只需在 `Deserialize_Script` 里 `if (!fieldsNode) return;` 之前不要报错即可（现在的写法天然兼容） |

---

## 9. 变更清单速览

- **新增文件**：无
- **修改文件（6 个）**
  - `Lucky/Source/Lucky/Scripting/ScriptFieldValue.h`：加 `ScriptFieldMap` 别名、`ScriptFieldTypeToString`、`StringToScriptFieldType`；补 `<string>` / `<unordered_map>`
  - `Lucky/Source/Lucky/Scene/Components/ScriptComponent.h`：加 `ScriptFieldMap Fields` 成员
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.h`：加 `SyncScriptFieldMap` 声明
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`：加 `SyncScriptFieldMap` 实现（可能补 `<algorithm>`）
  - `Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp`：`Serialize_Script` / `Deserialize_Script` 加 `Fields` 段（可能补 `YamlHelpers.h` 的 include）
  - `Lucky/Source/Lucky/Editor/ComponentInspectors.cpp`：`Draw_Script` 里用上 `PropertyAsset` 的返回值，触发一次同步
- **删除**：无
- **不改动**：`Asset/*`、`ScriptGlue.cpp`、`Scene/Scene.cpp`（运行态灌值属 P2.6）、托管 C# 代码、premake
- **存档格式变更**：`ScriptComponent` 段新增 `Fields` 映射；**没有 `Fields` 段的旧存档会得到空字段表**，随后被 `SyncScriptFieldMap` 补上默认值

---

## 10. 参考

- 编码规范 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：§3.3 include 顺序、§4.1 XML 注释、§5.2 强制花括号、§5.4 对齐规则、§6.1 class/struct、§8.1 enum class、§13.3 ECS 组件、§13.9 auto 使用规范
- 前置详设 [Phase2.3_ScriptComponent_AssetRef.md](Phase2.3_ScriptComponent_AssetRef.md)（`ScriptAsset` 与 `AssetHandle` 序列化范式）
- 前置详设 [Phase2.4_ScriptField_Metadata.md](Phase2.4_ScriptField_Metadata.md)（`ScriptFieldValue` / `GetFields` / `GetFieldValue` / `Instantiate`）
- 类型转换范式 [Phase2.1_Script_As_Asset.md](Phase2.1_Script_As_Asset.md) 决策点 4.1（`AssetType` 的 `ToString` / `FromString` 必须成对）
- 序列化范式 [SceneSerialization_Enhancement.md](../Serialization/SceneSerialization_Enhancement.md)、`Serialization/YamlHelpers.h`
- 脚本系统路线图 [ScriptSystem_Roadmap.md](ScriptSystem_Roadmap.md) 第 4 节 Phase 2「`ScriptComponent` 增加 `FieldMap`」「`SceneSerializer` 写入/读取 `FieldMap`」
