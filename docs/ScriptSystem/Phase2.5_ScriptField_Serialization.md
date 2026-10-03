# Phase 2.5：ScriptComponent 存字段值 + 序列化

## 1. 概述

把 P2.4 产出的 `ScriptFieldValue` 装进 `ScriptComponent`，并且：

1. **脚本首次挂上时，把字段的初始值填进 `FieldMap`** —— 初始值直接取 P2.4 已读好的 `ScriptField::DefaultValue`，Inspector 里看到的就是脚本里写的 `= 3.0f`，而不是 0
2. **脚本字段列表变化时自动同步**（新增字段补默认值、删掉的字段丢弃）
3. **随场景序列化** —— 存盘重开值还在

本 Phase 让 `ScriptComponent` 从"只有一个资产引用"变成"资产引用 + 一份字段值表"。**仍然不画控件**（P2.6 才画），但会在 Inspector 里加**一行**"拖入脚本后同步 FieldMap"的调用，好让本 Phase 能独立验收。

### 1.1 关键约束

- **字段的初始值直接取 `ScriptField::DefaultValue`，本 Phase 不造任何实例。** P2.4（决策点 4.6）已经在 `GetFields()` 里从真实实例把默认值读好存进 `ScriptField`（C# 字段初始化器编译进构造函数体，元数据里只有 0 —— 所以必须在实例上读，这一步 P2.4 已经做了）。`SyncScriptFieldMap` 只是把它抄进 `FieldMap`，是纯数据操作。
- **`FieldMap` 的键是字段名（`std::string`），不是索引。** 脚本里插入/删除/重排字段后，旧存档仍能按名字对上；代价是**字段改名会丢值**（Unity 同样如此）。
- **类型名与类型的互查全部走 P2.4 的类型表**：写盘用 `GetScriptFieldTypeInfo(type).Name`，读盘用本 Phase 在 `ScriptFieldType.h` 补的 `TryGetScriptFieldTypeByName` —— 不在别处再写一份字符串清单（P2.1 `AssetType` 漏 `StringToAssetType` 分支的教训）。
- **反序列化时不依赖 `ScriptEngine` 已加载程序集**：读出来只是"一串有类型的值"，不需要解析脚本类。补默认值/清理多余字段交给 `SyncScriptFieldMap`（它才需要程序集，且可能在 Inspector 拖入时或进 Play 前才被调用）。
- **存档里的字段类型与脚本当前类型不符时，以脚本为准**（用默认值覆盖 + 打 WARN）—— 脚本从 `float` 改成 `int` 属于用户改了签名，旧值已经语义无效。
- 代码风格遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：组件是 `struct`（§13.3）、`enum class`（§8.1）、控制语句强制花括号（§5.2）、范围 for 元素用 `auto`（§13.9）。

### 1.2 前置条件

| 依赖 | 说明 |
|------|------|
| P2.3 | `ScriptComponent::ScriptAsset`（`Ref<Script>`）已就位 |
| P2.4 | `ScriptFieldType.h`（类型表）、`ScriptFieldValue.h`（variant 容器）、`ScriptClass::GetFields()`（含 `ScriptField::DefaultValue`，默认值已读好）已就位 |
| P2.2 | `ScriptEngine::ResolveScriptClass` 已就位 |
| 序列化 | `ComponentSerializers.cpp` 里 `ScriptComponent` 已有 `Serialize_Script` / `Deserialize_Script`（P2.3 改成 `AssetHandle`） |
| YAML | `YamlHelpers.h` 提供 vec3 等类型的 YAML 转换 |

### 1.3 本 Phase **不做**的事

- 不做 Inspector 的字段控件（P2.6）—— 本 Phase 只在拖入脚本时触发一次同步
- 不做"进 Play 时把值灌进托管对象"（P2.6）
- 不新增字段类型（类型集合以 P2.4 登记的 22 种为准；本 Phase 做的是让它们**全部**可序列化）
- 不做 `[HideInInspector]`

---

## 2. 涉及的文件

### 2.1 新建

无。

### 2.2 修改

| 文件 | 改动 |
|------|------|
| `Lucky/Source/Lucky/Scripting/ScriptFieldType.h` | 加 `TryGetScriptFieldTypeByName`（序列化反查） |
| `Lucky/Source/Lucky/Scripting/ScriptFieldValue.h` | 加 `ScriptFieldMap` 别名；补 `<unordered_map>` |
| `Lucky/Source/Lucky/Scene/Components/ScriptComponent.h` | 加 `ScriptFieldMap Fields` 成员 |
| `Lucky/Source/Lucky/Scripting/ScriptEngine.h` | 加 `SyncScriptFieldMap` 声明；`ResolveAssetByFieldType` 提升为公开静态 |
| `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp` | 加 `SyncScriptFieldMap` 实现；`ResolveAssetByFieldType` 定义挪出匿名命名空间 |
| `Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp` | `Serialize_Script` / `Deserialize_Script` 加 `Fields` 段；补 `ScriptEngine.h` include |
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

`ScriptFieldValue` 是 variant 容器（22 种类型，详见 P2.4 决策点 4.1 / 4.2）：

```cpp
    struct ScriptFieldValue
    {
        ScriptFieldType  Type = ScriptFieldType::None;   // 权威：先看 Type，再 std::get 取载荷
        ScriptFieldScalar Data;                          // variant：bool / 8 种整数 / float / double / string
                                                         // / vec2-4 / quat / UUID(Entity) / Ref<Asset>(资产)
    };
```

与本 Phase 直接相关的另两件 P2.4 产出：

```cpp
    struct ScriptField
    {
        std::string       Name;
        ScriptFieldType   Type;
        ScriptFieldValue  DefaultValue;   // ★ P2.4 已从真实实例读好，本 Phase 直接用
        MonoClassField*   Field;
    };

    const std::vector<ScriptField>& ScriptClass::GetFields();   // 含继承链上的字段（P2.4 决策点 4.9）
```

### 3.3 默认值已在 P2.4 读好，本 Phase 不再实例化

P2.4 的 `ScriptClass::GetFields()` 首次调用时会造一个临时实例读默认值（`Instantiate()` 只跑无参构造，不需要实体 UUID），存进每条 `ScriptField::DefaultValue`。因此本 Phase 的 `SyncScriptFieldMap` **完全不需要碰 mono 实例** —— 它操作的是纯数据（`ScriptField` 元信息 + `FieldMap`）。这让本 Phase 比最初设想少了一整节"临时实例副作用约定"。

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

### 3.5 类型名互查走 P2.4 的类型表（不照抄 `AssetType` 的手写清单）

P2.1 给 `AssetType` 手写了 `AssetTypeToString` / `StringToAssetType` 两份清单，结果漏了 `StringToAssetType` 的一个分支。P2.4 的类型表（`s_ScriptFieldTypeInfos[]`）自带序列化名 `Name`，所以：

- 写盘：`GetScriptFieldTypeInfo(type).Name`（P2.4 已就位）
- 读盘：本 Phase 在 `ScriptFieldType.h` 补一个 `TryGetScriptFieldTypeByName`（遍历表按 `Name` 匹配，见 Step 1）

**不手写任何字符串清单** —— 加类型时表里加一行，两个方向同时生效。

### 3.6 场景里 `ScriptComponent` 段落的目标形态（本 Phase 之后）

```yaml
    ScriptComponent:
      AssetHandle: 12345678901234567890
      Fields:
        Speed:
          Type: Float
          Value: 3.0
        LogPosition:
          Type: Bool
          Value: false
        Title:
          Type: String
          Value: player
        Offset:
          Type: Vector3
          Value: [0, 0, 0]
        Target:
          Type: Entity
          Value: 9876543210987654321
        Mat:
          Type: Material
          Value: 12345678901234567891
```

**刻意做成"可读的键值对"**，方便你直接手改存档调试（也符合项目"YAML 存档 Git 友好"的既定取向）。值统一用 `Value` 键 —— 类型已由 `Type` 显式给出，读法按 `Type` 分发（决策点 4.5），不需要 `FloatValue` / `BoolValue` 这类"22 种类型 22 个键名"。

---

## 4. 关键设计决策（多方案对比）

### 4.1 决策点 1：字段初始值从哪拿

> 背景：这个问题在 P2.4 已经解决了一半 —— `GetFields()` 枚举时会从真实实例把每个字段的默认值读进 `ScriptField::DefaultValue`（P2.4 决策点 4.6）。本 Phase 要决定的是 `SyncScriptFieldMap` 怎么用这些默认值。

#### 方案 A：直接用 `ScriptField::DefaultValue`（**推荐 ✅**）

```cpp
// 缺失字段 / 类型不符字段：直接抄 P2.4 读好的默认值
fieldMap[field.Name] = field.DefaultValue;
```

- **优点**：
  1. **本 Phase 完全不碰 mono 实例** —— `SyncScriptFieldMap` 是纯数据操作，没有"临时实例注册/不注册、调不调 Awake"的副作用问题
  2. 默认值只读一次（P2.4 缓存），这里只是拷贝
  3. "同步"的触发时机（拖入脚本、进 Play 前兜底）随便调，没有任何构造副作用
- **缺点**：若脚本字段变了但程序集没重编，`GetFields()` 的缓存不会更新 —— 这与"脚本必须重编才能生效"的既定工作流一致，不算新增约束

#### 方案 B：同步时再造临时实例现读

- **优点**：读到的是"当下"的实例值
- **缺点**：
  1. **重复劳动**：P2.4 已经读过一遍，这里再造实例读一遍，同一件事做两次
  2. 重新引入 P2.4 已经吸收掉的副作用问题（C# 构造函数会在编辑期再跑一遍）
  3. "按需实例化"的分支让同步逻辑复杂一倍
- **否决**（P2.4 决策点 4.6 的目的就是把这件事提前做掉）

#### 方案 C：`FieldMap` 只存"用户改过的值"，缺省时 Inspector 显示类型零值

- **优点**：实现最简
- **缺点**：
  1. 编辑态看到的是 `0`，而不是脚本里的初值 —— 用户无法判断"到底生效了没有"
  2. 存档里没有"脚本初值"这个基准，一旦字段被改过就无法回到初值
  3. 与 Unity 的体验背离
- **次优**：如果实测发现默认值机制有真实痛点，可以退到这里，但要接受上面三条代价

**结论：方案 A。**

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

#### 方案 A：一个入口 `SyncScriptFieldMap`（**推荐 ✅**）

```cpp
static void SyncScriptFieldMap(const Ref<Script>& scriptAsset, ScriptFieldMap& fieldMap);
```

内部逻辑：解析类拿字段列表（`GetFields()`，含 `DefaultValue`）→ 缺失字段补 `DefaultValue`、类型不符的用 `DefaultValue` 覆盖 → 清掉脚本里已不存在的键。纯数据操作，不碰 mono 实例。

- **优点**：
  1. 调用方只有一句 `SyncScriptFieldMap(sc.ScriptAsset, sc.Fields);`
  2. 一个入口 = 一处维护，避免"两个接口调用顺序错了"的问题
  3. 无实例化 → 无触发时机顾虑（拖入时调、进 Play 前兜底调都安全）
- **缺点**：函数内部仍是"补齐/覆盖 + 清理"两段逻辑 —— 但都是直白的 map 操作

#### 方案 B：拆成 `FillFieldMapDefaults` + `PruneFieldMap` 两个接口

- **优点**：各自单一职责
- **缺点**：**调用方必须两个都调，且顺序有讲究**（先 prune 再 fill，否则刚补的默认值会被 prune 误删？不会，但顺序依赖本身就是坑）。**次优。**

#### 方案 C：同步逻辑放到 `ScriptComponent` 自己的方法里

- **优点**：`sc.Sync();` 读起来最顺
- **缺点**：`ScriptComponent` 就得 include `ScriptEngine.h`（要用 `ResolveScriptClass` / `GetFields`），把脚本运行时整条链拖进组件头 —— **直接破坏 P2.4 决策点 4.4 的努力**。**否决。**

**结论：方案 A。**

---

### 4.5 决策点 5：`Fields` 段在 YAML 里怎么写

#### 方案 A：可读的键值对 + 统一 `Value` 键（**推荐 ✅**）

```yaml
      Fields:
        Speed:
          Type: Float
          Value: 3.0
        LogPosition:
          Type: Bool
          Value: false
```

- **优点**：
  1. 直接手改存档就能试数值，不用开编辑器
  2. `Type` 显式写出来 —— 反序列化时**不依赖程序集**也能正确构造 `ScriptFieldValue`
  3. Git diff 可读（改一个值只动一行）
  4. **`Value` 一个键通吃 22 种类型** —— 读法由 `Type` 分发；若按"每类型一个键名"（`FloatValue` / `BoolValue` / …）需要 22 个键名，且与 variant 容器（P2.4 决策点 4.2）的形态不符
- **缺点**：比紧凑格式啰嗦（每个字段 3 行）

#### 方案 B：紧凑的序列形式

```yaml
      Fields:
        - Name: Speed
          Type: Float
          Value: 3.0
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

场景：脚本里 `public float Speed = 3.0f;` 改成了 `public int Speed = 3;`，存档里还是 `Type: Float, Value: 3.0`。

#### 方案 A：以脚本为准，用字段的当前默认值覆盖 + 打 WARN（**推荐 ✅**）

- **优点**：
  1. 结果确定：Inspector 显示的就是脚本当前的真实初值
  2. 一条 WARN 交代清楚"这个字段的值因为类型变了被重置了"
  3. 不会把 `Type: Float, Value=3.0` 硬塞进 `int` 字段（那是未定义行为级别的错配）
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
- **缺点**：每帧都要解析类 + 遍历字段 + 两轮 map 扫描 —— 纯浪费（字段表又不是每帧变）。而且 `UI::PropertyAsset` 的返回值已经提供了精确的触发时机，没有必要每帧轮询。**否决。**

**结论：方案 A。** 并在 §8 接线点注明 P2.6 要在"进 Play 前"再同步一次作为兜底。

---

## 5. 实现步骤（按依赖顺序）

每步完成后 `Lucky` 与 `Luck3DApp` 都应能编译通过。

### Step 1：类型表加反查函数 + `ScriptFieldValue.h` 加别名

**文件 A**：`Lucky/Source/Lucky/Scripting/ScriptFieldType.h`

在 `TryGetScriptFieldTypeByManagedName` **之后**加（反序列化按 `Name` 反查，与写盘的 `GetScriptFieldTypeInfo(type).Name` 配对）：

```cpp
    /// <summary>
    /// 按序列化名查类型（反序列化用）；查不到返回 false
    /// </summary>
    /// <param name="name">存档里的 Type 字符串，如 "Float" / "Vector3"</param>
    /// <param name="outType">输出：匹配到的字段类型</param>
    inline bool TryGetScriptFieldTypeByName(const char* name, ScriptFieldType& outType)
    {
        if (!name)
        {
            return false;
        }

        for (const ScriptFieldTypeInfo& info : s_ScriptFieldTypeInfos)
        {
            if (std::strcmp(info.Name, name) == 0)
            {
                outType = info.Type;
                return true;
            }
        }

        return false;
    }
```

**文件 B**：`Lucky/Source/Lucky/Scripting/ScriptFieldValue.h`

**1) 补 include**：

```cpp
#include <unordered_map>
```

**2) 文件末尾（`namespace Lucky` 内、结构体之后）加别名**：

```cpp
    /// <summary>
    /// 脚本字段值表：键为字段名
    /// 用名字而不是索引，脚本里插入/删除/重排字段后旧数据仍能对上；代价是字段改名会丢值
    /// </summary>
    using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldValue>;
```

**要点**：

- 写盘不需要新函数：`GetScriptFieldTypeInfo(type).Name`（P2.4 已有）；读盘用 `TryGetScriptFieldTypeByName`。**两个方向都从同一张表取**（§3.5），不存在"只写一侧"的可能
- **不要**手写 `ScriptFieldTypeToString` / `StringToScriptFieldType` 的字符串清单 —— 那正是 P2.1 `AssetType` 漏分支的同款结构
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
        /// - 类里新增的字段：用 ScriptField::DefaultValue 补进 fieldMap
        /// - 已存在但类型与脚本不符的字段：用 DefaultValue 覆盖（脚本改了字段类型时的必然结果）
        /// - fieldMap 里多出来的字段（脚本已删）：移除
        /// 默认值取 GetFields() 已读好的 ScriptField::DefaultValue，纯数据操作，不创建托管对象
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

        // ---- 第一遍：缺失字段补默认值，类型不符的用默认值覆盖 ----
        for (const ScriptField& field : fields)
        {
            auto it = fieldMap.find(field.Name);
            if (it == fieldMap.end())
            {
                fieldMap[field.Name] = field.DefaultValue;
                continue;
            }

            if (it->second.Type != field.Type)
            {
                LF_CORE_WARN("ScriptEngine::SyncScriptFieldMap - Type of field '{0}.{1}' changed from the saved type to the script type, value reset to the script default", scriptClass->GetName(), field.Name);
                it->second = field.DefaultValue;
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
                LF_CORE_WARN("ScriptEngine::SyncScriptFieldMap - Field '{0}' no longer exists in the script, stored value removed", it->first);
                it = fieldMap.erase(it);
            }
        }
    }
```

**要点**：

- **纯数据操作**：默认值全部来自 `ScriptField::DefaultValue`（P2.4 决策点 4.6 已读好），本函数不实例化、不调 `GetFieldValue` —— 在 Inspector 拖入时调、进 Play 前兜底调都没有副作用
- **`ResolveScriptClass` 失败时保留 `fieldMap` 不动**：用户在脚本没编译/改名期间，之前手调过的值不该被抹掉。这是刻意与"scriptAsset 为空就 clear"区别对待的 —— 前者是"脚本没了"，后者是"暂时解析不出来"
- `GetFields()` 返回的字段**含继承链上的字段**（P2.4 决策点 4.9），所以基类字段的默认值同步是自动覆盖的，不需要额外处理
- `std::any_of` 需要 `<algorithm>`（PCH 里通常已有）；若报未定义就补
- **清理阶段打 WARN 而不是静默**：用户把字段改了名，值丢了这件事要有痕迹

### Step 4：序列化 `Fields`

**文件 A**：`Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp`

**0) 前置：`ResolveAssetByFieldType` 从 P2.4 的匿名命名空间提升为 `ScriptEngine` 的公开静态方法**

反序列化资产引用字段时需要"按字段类型把 handle 解析成 `Ref<Asset>`"，而这个分发函数在 P2.4 里放在 `ScriptEngine.cpp` 的匿名命名空间，外部用不了。把它提升：

- `ScriptEngine.h` 公开区加声明：
```cpp
        /// <summary>
        /// 按字段类型从资产句柄解析资产对象；类型不匹配或句柄无效时返回空引用
        /// </summary>
        static Ref<Asset> ResolveAssetByFieldType(ScriptFieldType type, AssetHandle handle);
```
- `ScriptEngine.cpp`：定义原样挪出匿名命名空间，改成 `Ref<Asset> ScriptEngine::ResolveAssetByFieldType(...)`。`GetFieldValue` 里的调用点不用改（同类成员直接调）
- `Asset` 在 `ScriptEngine.h` 里前向声明即可（返回类型是 `Ref<Asset>`）

**1) `Serialize_Script` 在 `AssetHandle` 之后加 `Fields` 段**：

```cpp
            out << YAML::Key << "Fields" << YAML::Value << YAML::BeginMap;
            for (const auto& [fieldName, fieldValue] : sc.Fields)
            {
                out << YAML::Key << fieldName;
                out << YAML::BeginMap;
                out << YAML::Key << "Type" << YAML::Value << GetScriptFieldTypeInfo(fieldValue.Type).Name;

                switch (fieldValue.Type)
                {
                    case ScriptFieldType::Bool:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<bool>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::SByte:
                    {
                        // yaml-cpp 会把 int8_t 当字符输出，先提升为 int32
                        out << YAML::Key << "Value" << YAML::Value << static_cast<int32_t>(std::get<int8_t>(fieldValue.Data));
                        break;
                    }
                    case ScriptFieldType::Byte:
                    {
                        out << YAML::Key << "Value" << YAML::Value << static_cast<uint32_t>(std::get<uint8_t>(fieldValue.Data));
                        break;
                    }
                    case ScriptFieldType::Short:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<int16_t>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::UShort:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<uint16_t>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Int:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<int32_t>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::UInt:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<uint32_t>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Long:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<int64_t>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::ULong:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<uint64_t>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Float:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<float>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Double:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<double>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::String:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<std::string>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Vector2:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<glm::vec2>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Vector3:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<glm::vec3>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Vector4:
                    case ScriptFieldType::Color:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<glm::vec4>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Quaternion:
                    {
                        out << YAML::Key << "Value" << YAML::Value << std::get<glm::quat>(fieldValue.Data);
                        break;
                    }
                    case ScriptFieldType::Entity:
                    {
                        out << YAML::Key << "Value" << YAML::Value << static_cast<uint64_t>(std::get<UUID>(fieldValue.Data));
                        break;
                    }
                    case ScriptFieldType::Material:
                    case ScriptFieldType::Mesh:
                    case ScriptFieldType::Texture2D:
                    case ScriptFieldType::Script:
                    {
                        // 运行时创建的资产没有有效 handle，写出 0（读回是空引用，与 Unity 一致，见 P2.4 §6.9）
                        const Ref<Asset>& asset = std::get<Ref<Asset>>(fieldValue.Data);
                        out << YAML::Key << "Value" << YAML::Value << static_cast<uint64_t>(asset ? asset->GetHandle() : AssetHandle{});
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
                    if (!TryGetScriptFieldTypeByName(valueNode["Type"].as<std::string>("").c_str(), fieldValue.Type))
                    {
                        continue;   // 未知类型：跳过，由 SyncScriptFieldMap 用脚本默认值补齐
                    }

                    YAML::Node scalarNode = valueNode["Value"];
                    switch (fieldValue.Type)
                    {
                        case ScriptFieldType::Bool:   { fieldValue.Data = scalarNode.as<bool>(false); break; }
                        case ScriptFieldType::SByte:  { fieldValue.Data = static_cast<int8_t>(scalarNode.as<int32_t>(0)); break; }
                        case ScriptFieldType::Byte:   { fieldValue.Data = static_cast<uint8_t>(scalarNode.as<uint32_t>(0)); break; }
                        case ScriptFieldType::Short:  { fieldValue.Data = scalarNode.as<int16_t>(0); break; }
                        case ScriptFieldType::UShort: { fieldValue.Data = scalarNode.as<uint16_t>(0); break; }
                        case ScriptFieldType::Int:    { fieldValue.Data = scalarNode.as<int32_t>(0); break; }
                        case ScriptFieldType::UInt:   { fieldValue.Data = scalarNode.as<uint32_t>(0); break; }
                        case ScriptFieldType::Long:   { fieldValue.Data = scalarNode.as<int64_t>(0); break; }
                        case ScriptFieldType::ULong:  { fieldValue.Data = scalarNode.as<uint64_t>(0); break; }
                        case ScriptFieldType::Float:  { fieldValue.Data = scalarNode.as<float>(0.0f); break; }
                        case ScriptFieldType::Double: { fieldValue.Data = scalarNode.as<double>(0.0); break; }
                        case ScriptFieldType::String: { fieldValue.Data = scalarNode.as<std::string>(""); break; }
                        case ScriptFieldType::Vector2: { fieldValue.Data = scalarNode.as<glm::vec2>(glm::vec2(0.0f)); break; }
                        case ScriptFieldType::Vector3: { fieldValue.Data = scalarNode.as<glm::vec3>(glm::vec3(0.0f)); break; }
                        case ScriptFieldType::Vector4:
                        case ScriptFieldType::Color:   { fieldValue.Data = scalarNode.as<glm::vec4>(glm::vec4(0.0f)); break; }
                        case ScriptFieldType::Quaternion: { fieldValue.Data = scalarNode.as<glm::quat>(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)); break; }
                        case ScriptFieldType::Entity: { fieldValue.Data = UUID(scalarNode.as<uint64_t>(0)); break; }
                        case ScriptFieldType::Material:
                        case ScriptFieldType::Mesh:
                        case ScriptFieldType::Texture2D:
                        case ScriptFieldType::Script:
                        {
                            const AssetHandle handle(scalarNode.as<uint64_t>(0));
                            fieldValue.Data = ScriptEngine::ResolveAssetByFieldType(fieldValue.Type, handle);
                            break;
                        }
                        default:
                        {
                            continue;
                        }
                    }

                    sc.Fields[fieldName] = fieldValue;
                }
            }
```

**3) 补 include**：`ComponentSerializers.cpp` 目前没有 include `ScriptEngine.h`（调 `ResolveAssetByFieldType` 需要），按规范 §3.3 补进工程内头分组。`YamlHelpers.h`（glm 的 YAML 转换）与 `AssetManager.h` 已经在。

**要点**：

- **两个 `switch` 必须覆盖同一组类型（22 种）**。加类型时：P2.4 的类型表加一行 + 这里的读/写各加一个 case + P2.6 的控件加分支 —— 表之外的这三处是 P2.4 决策点 4.1 写明"表覆盖不了"的固定代价
- **`SByte` / `Byte` 必须提升后写盘**：yaml-cpp 把 8 位整型当字符处理，直接 `out << int8_t` 会写出不可读的字符而不是数字；读回时对应地用 `as<int32_t>` 再 `static_cast` 收窄
- `as<T>(默认值)` **一律给默认值**：存档可能被手改坏、可能是旧版本写的，缺失键（包括缺 `Value`）不该崩 —— yaml-cpp 对 undefined node 调 `as<T>(fallback)` 会返回 fallback
- `glm::vec2/3/4/quat` 的 YAML 转换来自 `Serialization/YamlHelpers.h`（四种都有 `convert` 特化，Transform/材质已在用）
- **运行时创建的资产写出来是 0**：`GetHandle()` 无效 → 读回空引用。这与 Unity"编辑器里 new 出来的 material 存不进场景"一致（P2.4 §6.9），不是 bug
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
- **若报 `TryGetScriptFieldTypeByName` 未定义**：Step 1 的函数没加进 `ScriptFieldType.h`（注意不是 `ScriptFieldValue.h`）
- **若报 `ResolveAssetByFieldType` 未定义或不可访问**：Step 4-0 的提升没做（它必须从匿名命名空间挪成 `ScriptEngine` 公开静态方法）
- **若报 `std::any_of` 未定义**：`ScriptEngine.cpp` 补 `#include <algorithm>`

---

## 6. 疑点问答

### 6.1 为什么 `SyncScriptFieldMap` 不需要造临时实例读默认值？

因为 P2.4 已经读好了。`ScriptClass::GetFields()` 首次调用时会造一个临时实例、把每个字段的默认值读进 `ScriptField::DefaultValue` 并缓存（P2.4 决策点 4.6）——"C# 字段初始化器编译进构造函数体、必须在实例上读"这个问题在那里解决。本 Phase 只是抄这份缓存，同一件事不做两遍。

这也回答了"临时实例怎么管"的问题：**本 Phase 没有临时实例**。P2.4 那边的临时实例是裸 `MonoObject*`（GC 管生命周期），同样不包装、不注册、不调 Awake。

### 6.2 为什么 `ResolveScriptClass` 失败时保留 `fieldMap`，而 `scriptAsset` 为空时清空？

两者的语义完全不同：

| 情况 | 语义 | 处理 |
|------|------|------|
| `scriptAsset` 为空 | 用户**主动**把脚本摘掉了，这个实体不再有脚本 | 清空 —— 留着无主的值没有意义 |
| `ResolveScriptClass` 失败 | 脚本资产还在，只是**暂时**解析不出类（没编译 / 改了名） | 保留 —— 用户手调过的值不该因为"忘了编译"被抹掉 |

第二条在实操里很常见：拖入脚本、调数值、改脚本、**忘了重新编译**、回到编辑器 —— 如果这时清空，用户会莫名其妙丢一堆调好的数。

### 6.3 脚本改字段类型后，我在 Inspector 调过的值去哪了？

被脚本的初始值覆盖了（决策点 4.6-A），并且日志里有（日志全英文，规范 §9.2）：

```
ScriptEngine::SyncScriptFieldMap - Type of field 'PlayerController.Speed' changed from the saved type to the script type, value reset to the script default
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

`TryGetScriptFieldTypeByName` 返回 `false` → `continue` 跳过该项。**不会崩**，该字段会保持"不存在"状态，随后 `SyncScriptFieldMap` 会用脚本初始值把它补上。**这正是"反序列化不依赖程序集、同步负责兜底"这套分工的好处。**

---

## 7. 验收标准

1. **编译通过**：`Lucky` 与 `Luck3DApp` 的 Debug / Release / Dist 三个 configuration 全通过
2. **初始值正确填充**：`PlayerController.cs` 里 `public float Speed = 3.0f;`，拖入 Script 字段后，存档里出现 `Speed: { Type: Float, Value: 3.0 }` —— **不是 0**（验的是默认值真的从 `ScriptField::DefaultValue` 抄过来了）
3. **字符串字段序列化往返**：`public string Title = "player";` → 存档里出现 `Title: { Type: String, Value: player }` → 存盘重开后值正确（验 22 种类型读写分发里的引用类型分支）
4. **存盘重开保留**：保存场景 → 关闭编辑器 → 重开 → 打开场景 → `Speed` 仍是 `3.0`
5. **手改存档生效**：直接编辑 `.luck3d` 把 `Value: 3.0` 改成 `7.5` → 重开场景 → 拖一次脚本或（P2.6 之后）进 Play，脚本读到的应是新值
6. **新字段自动补齐**：给脚本加 `public bool LogPosition = false;` → 重新编译 → 重开编辑器 → 打开场景 → 存档里出现 `LogPosition`
7. **删字段自动清理**：把 `LogPosition` 从脚本删掉 → 重新编译 → 重开 → 打开场景 → 存档里 `LogPosition` 消失，且日志有 `no longer exists in the script` 的 WARN
8. **类型变更被重置**：把 `Speed` 改成 `public int Speed = 3;` → 重新编译 → 重开 → 打开场景 → 该字段变成 `Type: Int, Value: 3`，日志有 `changed from the saved type to the script type` 的 WARN
13. **8 位整数写盘是数字不是字符**：给脚本加 `public sbyte Small = -5;` → 存档里是 `Value: -5`（**不是乱码字符**，验 Step 4 的 int8 提升）
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
| `ScriptComponent::Fields` | **P2.6** | 在旁边画字段控件（每个字段一个控件，写回 `Fields`）。**读写 `Data` 必须按 `Type` 的实际载荷类型 `std::get`**（P2.4 §8：8 种整数宽度不同、`Double` 是 `double`，显示可截断、写回必须保值） |
| `ScriptEngine::SyncScriptFieldMap` | **P2.6** | 进 Play 前（`Scene::OnRuntimeStart` 里）再同步一次作为兜底：脚本字段在外部改过、但用户没重新拖入的情况 |
| `ScriptClass::SetFieldValue` | **P2.6** | 在 `ScriptInstance` 构造完成后、`InvokeAwake()` **之前**，遍历 `Fields` 逐个灌入 |
| 序列化读/写两个 `switch` | 类型扩展 | 加新字段类型时：P2.4 类型表加一行 + 这里读/写各加一个 case + P2.6 控件加分支（类型名互查不用动，走表） |
| `Fields` 为空时写出 `{}` 的行为 | 存档格式演进 | 若将来要兼容"没有 Fields 段的旧存档"，只需在 `Deserialize_Script` 里 `if (!fieldsNode) return;` 之前不要报错即可（现在的写法天然兼容） |

---

## 9. 变更清单速览

- **新增文件**：无
- **修改文件（7 个）**
  - `Lucky/Source/Lucky/Scripting/ScriptFieldType.h`：加 `TryGetScriptFieldTypeByName`（序列化反查，与 `GetScriptFieldTypeInfo(type).Name` 配对）
  - `Lucky/Source/Lucky/Scripting/ScriptFieldValue.h`：加 `ScriptFieldMap` 别名；补 `<unordered_map>`
  - `Lucky/Source/Lucky/Scene/Components/ScriptComponent.h`：加 `ScriptFieldMap Fields` 成员
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.h`：加 `SyncScriptFieldMap` 声明；`ResolveAssetByFieldType` 提升为公开静态方法的声明（Step 4-0）
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`：加 `SyncScriptFieldMap` 实现；`ResolveAssetByFieldType` 定义挪出匿名命名空间（可能补 `<algorithm>`）
  - `Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp`：`Serialize_Script` / `Deserialize_Script` 加 `Fields` 段；补 `ScriptEngine.h` 的 include
  - `Lucky/Source/Lucky/Editor/ComponentInspectors.cpp`：`Draw_Script` 里用上 `PropertyAsset` 的返回值，触发一次同步
- **删除**：无
- **不改动**：`Asset/*`、`ScriptGlue.cpp`、`Scene/Scene.cpp`（运行态灌值属 P2.6）、托管 C# 代码、premake
- **存档格式变更**：`ScriptComponent` 段新增 `Fields` 映射（`Type` + 统一 `Value` 键）；**没有 `Fields` 段的旧存档会得到空字段表**，随后被 `SyncScriptFieldMap` 补上默认值

---

## 10. 参考

- 编码规范 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：§3.3 include 顺序、§4.1 XML 注释、§5.2 强制花括号、§5.4 对齐规则、§6.1 class/struct、§8.1 enum class、§13.3 ECS 组件、§13.9 auto 使用规范
- 前置详设 [Phase2.3_ScriptComponent_AssetRef.md](Phase2.3_ScriptComponent_AssetRef.md)（`ScriptAsset` 与 `AssetHandle` 序列化范式）
- 前置详设 [Phase2.4_ScriptField_Metadata.md](Phase2.4_ScriptField_Metadata.md)（决策点 4.1 类型表 / 4.2 variant 容器 / 4.6 默认值在枚举时读好 / 4.9 继承链枚举）
- 类型转换范式 [Phase2.1_Script_As_Asset.md](Phase2.1_Script_As_Asset.md) 决策点 4.1（`AssetType` 漏 `StringToAssetType` 的教训 —— 本 Phase 用"两个方向都查同一张表"规避）
- 序列化范式 [SceneSerialization_Enhancement.md](../Serialization/SceneSerialization_Enhancement.md)、`Serialization/YamlHelpers.h`
- 脚本系统路线图 [ScriptSystem_Roadmap.md](ScriptSystem_Roadmap.md) 第 4 节 Phase 2「`ScriptComponent` 增加 `FieldMap`」「`SceneSerializer` 写入/读取 `FieldMap`」
