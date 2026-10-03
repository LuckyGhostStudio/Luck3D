# Phase 2.6：Inspector 字段控件 + 运行态灌值

## 1. 概述

Phase 2 的收口。两件事：

1. **Inspector 按字段类型画控件** —— 拖动条 / 输入框 / 勾选框 / 三轴，改动直接写回 `ScriptComponent::Fields`
2. **运行态灌值** —— 托管对象实例化后、`Awake` 之前，把 `Fields` 里的值写进托管对象

外加两处收尾：

3. **加一个静默版的类解析接口**，供 Inspector 做错误提示用（否则 P2.3 那个红色提示会**逐帧刷 ERROR 日志**）
4. **反序列化之后同步一次字段表**，解决"脚本加了新字段 → 重新编译 → 重启编辑器 → 打开场景，新字段在 Inspector 里看不到"的问题

做完这四件，Phase 2 的出口标准达成：**改脚本里的 `public float Speed = 3.0f;` → Inspector 出现 Speed 拖动条 → 调了值 → 存盘重开还在 → Play 后脚本读到的是你调的值。**

### 1.1 关键约束

- **灌值必须在 `Awake` 之前。** 脚本的 `Awake` 极可能直接读字段（`m_Speed = Speed;` 这类缓存）。顺序反了 → 脚本读到脚本初值而不是你调的值，**而且完全不报错** —— 最难查的一类 bug。灌值点就在 `ScriptEngine::OnCreateEntityScript` 里、`instance->InvokeAwake()` 的**上一行**。
- **Inspector 里必须换用静默版解析。** `ResolveScriptClass`（P2.2）在失败时会打 `LF_CORE_ERROR`，而 Inspector **每帧**都要画那个红色提示 —— 直接调它会 60 次/秒刷日志。所以要加一个不写日志的 `TryResolveScriptClass`，**只在用户可见的 UI 上呈现错误，日志交给真正的动作路径打**。
- **Inspector 不要每帧调用 `SyncScriptFieldMap`。** 正确做法是"**真正会改变字段表的事件**"触发同步，共三处：Inspector 拖入脚本（P2.5 已接）、**场景反序列化之后**（本 Phase 新增）、以及（可选）脚本资产被替换。每帧同步会让"解析失败"这种情况持续空转。
  > **纠正 P2.5 文档 §8 的一处设想**：那里写"P2.6 要在进 Play 前再同步一次作为兜底"。**本 Phase 分析后决定不做。** 理由：`ScriptInstance::SetFieldValues` 对"字段表里没有的字段"本来就是跳过（保留托管对象自己的初值），所以缺字段时运行态天然正确，不需要在 `OnRuntimeStart` 里补同步；而且 `OnRuntimeStart` 操作的是 `Scene::Copy` 出来的**运行态副本**，同步结果在 Stop 时就丢了。**真正需要同步的时机是"反序列化之后"**（见约束 4 与 Step 4）。
- **控件优先复用 `UI::` 层现成的；缺的两类按现有形态补**。已核实（`UI/PropertyGrid.h`）：
  - 现成：`PropertyFloat`（`:67`）、`PropertyFloat2`（`:72`）、`PropertyFloat3`（`:77`）、`PropertyFloat4`（`:82`）、`PropertyInt`（`:95`）、`PropertyColor(vec4)`（`:107`）、`PropertyString(char*, size_t)`（`:118`）、`PropertyCheckbox`（`:147`）、`PropertyAsset<T>`（`:172`）
  - **需新写两个**：`PropertyLong`（int64 拖拽，给 `UInt`/`Long`/`ULong`）与 `PropertyEntityRef`（实体引用槽，拖放源是场景树的 `DragDrop::EntityHierarchy`，payload 为 `UUID`，`SceneHierarchyPanel.cpp:518` 已有）
  - 默认参数 `min = 0, max = 0` 表示**不夹取范围**（ImGui `DragFloat` 的惯例），只传 label + value 即可
- **控件映射按 `ScriptFieldWidgetKind` 分发，不按类型逐一映射**：22 种类型 → 10 种控件行（P2.4 类型表已登记每种类型的 Widget）。以后加类型时：P2.4 类型表加一行 + 序列化读/写各一个 case + 这里对应 Widget 的分支补载荷存取。
- **载荷存取分两档**（P2.4 §8 的接线要求）：能精确装载荷的控件直接传 `std::get<T>(fieldValue.Data)` 的**引用**原地改（`bool` / `int32` / `float` / `vec2-4` / `UUID`）；装不下的（8/16 位整数、`UInt`/`Long`/`ULong`、`Double`、`Quaternion`、`string` 的 char 缓冲、`Ref<T>` 向下转换）用**临时变量中转、控件返回 true 才写回** —— 显示可截断，写回必须保值，用户没碰就一字节不动。
- 代码风格遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：控制语句强制花括号（§5.2）、`switch` 各 case 带花括号、范围 for 用 `auto`（§13.9）、智能指针引用显式声明类型（§13.9）。

### 1.2 前置条件

| 依赖 | 说明 |
|------|------|
| P2.3 | `ScriptComponent::ScriptAsset` + `Draw_Script` 里的 `PropertyAsset<Script>` |
| P2.4 | `ScriptField` / `ScriptFieldType` / `ScriptClass::GetFields()` / `SetFieldValue` |
| P2.5 | `ScriptComponent::Fields`（`ScriptFieldMap`）、`SyncScriptFieldMap`、序列化 |
| `UI::` 属性控件 | 9 个现成 + 本 Phase 新写 2 个（`PropertyLong` / `PropertyEntityRef`，见 1.1） |
| `ScriptEngine::OnCreateEntityScript` | 已就位（P2.3 改过签名） |

### 1.3 本 Phase **不做**的事

- 不做字段分组、折叠、排序、拖拽排序
- 不做 `[HideInInspector]`
- 不做枚举 / 数组 / 嵌套类 / `Dictionary` 字段（与 P2.4 的不做清单一致；`string` / `Entity` / 资产引用 / 全部标量与向量类型本 Phase **都做**）
- Entity 引用槽只做"拖入赋值 + 显示名字 + 清空"，**不校验目标实体上是否挂了对应脚本**（P2.4 决策点 4.10 的既定边界）
- 不做"改脚本自动重编译"（P3）
- 不做 Console 面板（独立项）

---

## 2. 涉及的文件

### 2.1 新建

无。

### 2.2 修改

| 文件 | 改动 |
|------|------|
| `Lucky/Source/Lucky/Scripting/ScriptEngine.h` | `ScriptEngine` 加 `TryResolveScriptClass`；`ScriptInstance` 加 `SetFieldValues` |
| `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp` | 加两个实现；`OnCreateEntityScript` 里插入灌值调用 |
| `Lucky/Source/Lucky/Editor/ComponentInspectors.cpp` | `Draw_Script` 改用静默解析 + 按 `ScriptFieldWidgetKind` 画字段控件 |
| `Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp` | `Deserialize_Script` 末尾同步一次字段表 |
| `Lucky/Source/Lucky/UI/PropertyGrid.h` + 对应 `.cpp` | 新写 `PropertyLong`（int64 拖拽）与 `PropertyEntityRef`（实体引用槽） |
| `Lucky/Source/Lucky/Scene/Entity.h` | 加 `GetScene()` 访问器（一行） |

### 2.3 不修改

- `Scene/Scene.cpp`：**不改**（见约束 3 的说明：`OnRuntimeStart` 不需要补同步；灌值发生在 `OnCreateEntityScript` 内部）
- `Scene/Components/ScriptComponent.h`：P2.5 已完成
- `Scripting/ScriptFieldValue.h`：P2.4 / P2.5 已完成
- 托管 C# 代码：不涉及
- premake：无新增文件（`PropertyGrid` 是已有文件的修改）

---

## 3. 现状回顾

### 3.1 `OnCreateEntityScript` 当前实现（要插入灌值的地方）

```cpp
    void ScriptEngine::OnCreateEntityScript(Entity entity, const Ref<ScriptClass>& scriptClass)
    {
        LF_CORE_ASSERT(scriptClass, "ScriptEngine::OnCreateEntityScript - scriptClass must not be null");

        Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(scriptClass, entity);
        s_Data->EntityInstances[entity.GetUUID()] = instance;

        instance->InvokeAwake();
    }
```

**灌值要插在 `CreateRef` 之后、`InvokeAwake()` 之前。**

### 3.2 `ScriptInstance` 当前形态

```cpp
    class ScriptInstance
    {
    public:
        ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity);

        void InvokeAwake();
        void InvokeUpdate(float dt);
        void InvokeDestroy();

        Ref<ScriptClass> GetScriptClass() const { return m_ScriptClass; }
    private:
        Ref<ScriptClass> m_ScriptClass;
        MonoObject* m_Instance = nullptr;
        MonoMethod* m_Constructor = nullptr;
        MonoMethod* m_AwakeMethod = nullptr;
        MonoMethod* m_UpdateMethod = nullptr;
        MonoMethod* m_DestroyMethod = nullptr;
        bool m_DestroyCalled = false;
    };
```

`m_Instance` 是托管对象，`m_ScriptClass` 有字段元信息 —— 灌值需要的东西都在。

### 3.3 `Draw_Script` 当前实现（P2.5 之后）

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
                ImGui::TextColored({0.9f, 0.35f, 0.35f, 1.0f}, "Script class not found.\nMake sure the script is compiled and the class name matches the file name.");
            }
        }
```

**这里的 `ResolveScriptClass` 就是每帧刷 ERROR 的那个点。**

### 3.4 `ResolveScriptClass` 的日志行为（P2.2 产出）

```cpp
        if (hasMultipleMatches)
        {
            LF_CORE_ERROR("ScriptEngine::ResolveScriptClass - Class name '{0}' matches multiple script classes: {1}. Please rename the file or adjust its namespace to make it unique.", className, matchedFullNames);
            return nullptr;
        }

        if (!matchedClass)
        {
            LF_CORE_ERROR("ScriptEngine::ResolveScriptClass - Script class '{0}' not found. Please make sure the script has been compiled and its class name matches the file name.", className);
            return nullptr;
        }
```

### 3.5 `Deserialize_Script` 当前实现（P2.5 之后，要加同步的地方）

```cpp
        void Deserialize_Script(Entity entity, const YAML::Node& entityNode)
        {
            YAML::Node node = entityNode["ScriptComponent"];
            if (!node)
            {
                return;
            }
            ScriptComponent& sc = entity.AddComponent<ScriptComponent>();

            YAML::Node handleNode = node["AssetHandle"];
            if (!handleNode)
            {
                // 旧格式（ClassName）：不做迁移，脚本引用留空
                LF_CORE_WARN("SceneSerializer: ScriptComponent uses legacy 'ClassName' format, script reference is dropped. Reassign the script asset in the Inspector.");
                return;
            }

            uint64_t handleValue = handleNode.as<uint64_t>();
            AssetHandle handle(handleValue);
            if (handle.IsValid())
            {
                sc.ScriptAsset = AssetManager::GetAsset<Script>(handle);
            }

            if (!sc.ScriptAsset && handle.IsValid())
            {
                LF_CORE_ERROR("SceneSerializer: Failed to load script asset [{0}]", handleValue);
            }

            // ← Fields 段的读取（P2.5 加的）在这里
            // ← 本 Phase 要在函数末尾加一次同步
        }
```

### 3.6 `ScriptFieldWidgetKind` → 控件映射（22 种类型 → 10 种控件行）

| WidgetKind | 覆盖的类型 | 控件（`UI/PropertyGrid.h`） | 载荷存取 |
|-----------|-----------|------------------------------|---------|
| `Checkbox` | `Bool` | `PropertyCheckbox`（现成） | **直接**：`std::get<bool>(Data)` 的引用 |
| `Int` | `SByte` `Byte` `Short` `UShort` `Int` | `PropertyInt`（现成） | int32 临时中转，改才写回 |
| `Int` | `UInt` `Long` `ULong` | **`PropertyLong`（本 Phase 新写）** | int64 临时中转，改才写回（`ULong` 超 `int64` 上限的值显示截断，不写回就不损坏） |
| `Float` | `Float` | `PropertyFloat`（现成） | **直接** |
| `Float` | `Double` | `PropertyFloat`（现成） | float 临时中转，改才写回（精度损失只发生在用户主动编辑时） |
| `Float2` | `Vector2` | `PropertyFloat2`（现成） | **直接** |
| `Float3` | `Vector3` | `PropertyFloat3`（现成） | **直接** |
| `Float4` | `Vector4` | `PropertyFloat4`（现成） | **直接** |
| `Float4` | `Quaternion` | `PropertyFloat4`（现成） | `vec4(w,x,y,z)` 临时中转，改才写回 |
| `Color` | `Color` | `PropertyColor(vec4)`（现成） | **直接**（颜色选择器，比 4 个 float 行好用） |
| `Text` | `String` | `PropertyString(char*, size_t)`（现成） | char 缓冲中转，改才写回 |
| `EntityRef` | `Entity` | **`PropertyEntityRef`（本 Phase 新写）** | **直接**：`std::get<UUID>(Data)` 的引用 |
| `AssetRef` | `Material` `Mesh` `Texture2D` `Script` | `PropertyAsset<T>`（现成模板） | `Ref<T>` 临时中转（`static_pointer_cast`），改才写回 |

所有控件都返回 `bool`（是否被修改），且内部自带 `BeginPropertyGrid` / `PropertyLabel` / `PropertyValueBegin` / `EndPropertyGrid`，**一行就是一个完整的属性行**。"直接"档把控件绑到 `std::get<T>` 返回的引用上原地改；"中转"档只有控件返回 `true` 才把临时变量写回 variant（决策点 4.5）。

### 3.7 打开场景的时序（支撑约束 4）

`Application::Init()` 中的顺序：

```
Project::Load(...)          // 加载 .lcproj
AssetManager::Init()        // 资产系统就绪
Renderer::Init()
ScriptEngine::Init()        // 加载 Core / App 程序集，EntityClasses 就绪  ← 关键
PushOverlay(m_ImGuiLayer)
```

`EditorLayer` 里"根据命令行参数打开场景"发生在**这之后**（在 `Run()` 循环里）。所以**反序列化场景时 `ScriptEngine` 已经就绪**，可以在 `Deserialize_Script` 里安全调用 `SyncScriptFieldMap`。

---

## 4. 关键设计决策（多方案对比）

### 4.1 决策点 1：灌值代码放在哪

#### 方案 A：`ScriptInstance::SetFieldValues(const ScriptFieldMap&)`（**推荐 ✅**）

```cpp
void ScriptInstance::SetFieldValues(const ScriptFieldMap& fieldMap);
```

在 `ScriptEngine::OnCreateEntityScript` 里、`InvokeAwake()` 之前调一次。

- **优点**：
  1. **职责内聚**：托管对象的生命周期归 `ScriptInstance`，往它身上写值也应该由它负责
  2. `m_Instance` + `m_ScriptClass` 都是它的私有成员，函数内部直接取用，**不需要任何参数传递**
  3. 与现有的 `InvokeAwake` / `InvokeUpdate` / `InvokeDestroy` 形态完全一致（都是"对托管对象做一件生命周期相关的事"）
- **缺点**：`ScriptEngine.h` 要 include `ScriptFieldValue.h`（P2.4 已经加了，零额外成本）

#### 方案 B：在 `OnCreateEntityScript` 里内联循环

```cpp
        Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(scriptClass, entity);

        const ScriptComponent& sc = entity.GetComponent<ScriptComponent>();
        for (const ScriptField& field : scriptClass->GetFields())
        {
            auto it = sc.Fields.find(field.Name);
            if (it != sc.Fields.end())
            {
                scriptClass->SetFieldValue(instance->GetMonoObject(), field, it->second);
            }
        }
```

- **优点**：不加新方法
- **缺点**：
  1. `instance->GetMonoObject()` 需要**新开一个 getter 把内部托管对象暴露出去** —— 等于为了绕开封装而破坏封装
  2. 这段逻辑将来别处也要用（比如 P3 热重载后重新灌值），内联在 `OnCreateEntityScript` 里没法复用
  3. 它属于 `entity` / 组件层的关注点，塞进 `ScriptEngine` 的实例化函数里会让那个函数变长、职责变杂
- **次优。**

#### 方案 C：把 `FieldMap` 传进 `ScriptInstance` 的构造函数

- **优点**：一次构造搞定
- **缺点**：`ScriptInstance` 的构造目前只收 `ScriptClass` + `Entity`，"字段值"是**场景数据**而不是**实例身份**的一部分；而且 `ScriptComponent` 可能在实例化之后被改（P3 要支持运行时改值），构造期注入没法复用。**否决。**

**结论：方案 A。**

---

### 4.2 决策点 2：Inspector 的错误提示怎么避免刷日志

#### 方案 A：加 `TryResolveScriptClass`（静默版），Inspector 用它（**推荐 ✅**）

```cpp
static Ref<ScriptClass> TryResolveScriptClass(const std::string& className);   // 无日志
static Ref<ScriptClass> ResolveScriptClass(const std::string& className);      // 有日志（现有）
```

两个接口共用同一段匹配实现，只是"是否打日志"不同。内部抽一个私有辅助：

```cpp
        static Ref<ScriptClass> ResolveScriptClassImpl(const std::string& className, bool logDiagnostics);
```

- **优点**：
  1. **日志只在真正需要的地方产生** —— 动作路径（进 Play、反序列化同步）打日志，纯展示路径（Inspector）不打
  2. Inspector 的红色提示本身就是给用户的反馈，**不需要日志再来一遍**
  3. 两个公开接口 + 一个私有实现，结构清晰
- **缺点**：多一个公开接口

#### 方案 B：把 `ResolveScriptClass` 的日志级别从 ERROR 降到 TRACE

- **优点**：改动最小（一行）
- **缺点**：**动作路径的诊断能力被一起削弱** —— 进 Play 时脚本解析失败会变得不显眼，而那是必须让用户看见的错误。**否决。**

#### 方案 C：Inspector 自己缓存"上一次的解析结果"，只在变化时打日志

- **优点**：不动 `ScriptEngine` 接口
- **缺点**：要往 `Draw_Script` 里塞静态状态（哪个实体？哪个脚本？），**状态管理比问题本身复杂**；而且日志仍会打，只是被缓存挡了一层。**否决。**

#### 方案 D：Inspector 干脆不做错误提示（回到只打日志）

- **优点**：零改动
- **缺点**：P2.3 特意做的错误可见性被撤掉，用户拖了个没编译的脚本会一头雾水。**否决。**

**结论：方案 A。**

---

### 4.3 决策点 3：字段控件的 label 用什么

#### 方案 A：直接用字段名（**推荐 ✅**）

```cpp
UI::PropertyFloat(field.Name.c_str(), std::get<float>(fieldValue.Data));
```

- **优点**：
  1. 与脚本里的字段名**逐字对应** —— 看到 `Speed` 就知道去脚本里改哪个
  2. Unity 也是直接显示字段名（驼峰/帕斯卡原样）
  3. 零额外信息，不用维护"字段名 → 显示名"的映射
- **缺点**：字段名是英文标识符，界面上不会有中文说明。**这是符合预期的** —— 脚本作者自己起的名字，他自己认得

#### 方案 B：加个前缀（`Script.Speed`）

- **优点**：和组件自身的属性区分开
- **缺点**：`ScriptComponent` 里除了 `Script`（资产引用）就只有字段了，不会混淆；加前缀只是噪音。**否决。**

#### 方案 C：根据字段名做"驼峰拆词"美化（`MoveSpeed` → `Move Speed`）

- **优点**：视觉更舒服
- **缺点**：引入一层"名字转换"，用户看到 `Move Speed` 却要在脚本里找 `MoveSpeed`，**反而增加了一次心智翻译**。**否决。**

#### 方案 D（必须注意的配套）：ImGui ID 冲突

字段名在**同一个脚本内**唯一（`ScriptClass::GetFields()` 不会重复），但不同组件的属性 label 理论上可能重名（比如某个组件也有叫 `Speed` 的）。

- 先按方案 A 直接写。
- **如果实测出现"点了 A 的控件 B 的值变了"这类 ID 冲突**，在 `Draw_Script` 的字段循环外用 `ImGui::PushID(sc.ScriptAsset ? sc.ScriptAsset->GetHandle() : 0)` / `ImGui::PopID()` 包一层。
- `UI::PropertyAsset` 内部用的是 `GenerateID()`，项目里的属性控件大概率已经处理了 ID 唯一性；**先不预防，等实测**。

**结论：方案 A（含方案 D 的备用手段）。**

---

### 4.4 决策点 4："脚本加了新字段但编辑器里的字段表没有"怎么解决

场景很具体（而且必然发生，因为没有热重载）：

1. 你给 `PlayerController` 加 `public bool LogPosition = false;`
2. 重新编译 `Assembly-CSharp`
3. **重启编辑器**（程序集只在启动时加载）
4. 打开之前存过的场景 → `Fields` 里没有 `LogPosition` → **Inspector 里看不到它**

#### 方案 A：`Deserialize_Script` 末尾同步一次（**推荐 ✅**）

```cpp
            // 读完 Fields 之后
            if (sc.ScriptAsset)
            {
                ScriptEngine::SyncScriptFieldMap(sc.ScriptAsset, sc.Fields);
            }
```

- **优点**：
  1. **时机天然正确**：打开场景时 `ScriptEngine` 已就绪（见 3.7 的时序）
  2. 一次搞定，不刷屏、不每帧空转
  3. 顺带把"删掉的字段"清理掉、"类型变了的字段"重置掉 —— 三个分支一次覆盖
  4. 用户视角完全无感：重启编辑器打开场景，新字段就在那里了
- **缺点**：
  1. `ComponentSerializers.cpp` 要 include `ScriptEngine.h`（`Serialization → Scripting` 依赖）。**但该文件已经在依赖 `AssetManager`、`Renderer3D`、`MaterialSerializer`，多一个 `ScriptEngine` 不改变它的层次**
  2. 打开一个脚本解析失败的场景时，日志里会出现那条 ERROR（**这是好事** —— 用户确实需要知道）

#### 方案 B：Inspector 每帧调 `SyncScriptFieldMap`

- **优点**：不需要动序列化
- **缺点**：
  1. 每帧都要 `ResolveScriptClass`（遍历 `EntityClasses` 做字符串比较）—— 虽然单次很便宜，但**每帧白做功**
  2. `SyncScriptFieldMap` 内部用的是**会打日志**的 `ResolveScriptClass`，脚本解析失败时会逐帧刷 ERROR
  3. 要修第 2 点就得让 `SyncScriptFieldMap` 也静默，那"同步失败"就彻底没有痕迹了
- **否决。**

#### 方案 C：Inspector 上给个"重新同步"按钮，手动点

- **优点**：完全由用户控制，零自动行为
- **缺点**：**把一件必然要做的事推给用户**。用户加个字段就得记着点一下按钮，忘了就会以为"字段没生效"。**次优**（可以作为兜底手段保留，但不作为主方案）

#### 方案 D：`Scene::OnRuntimeStart` 里同步（P2.5 §8 的原始设想）

- **优点**：运行态一定能拿到最新字段
- **缺点（这是本 Phase 纠正 P2.5 的地方）**：
  1. **运行态根本不需要** —— `SetFieldValues` 对"字段表里没有的字段"是跳过，托管对象保留自己的 C# 初值，行为已经正确
  2. `OnRuntimeStart` 操作的是 `Scene::Copy` 出来的**副本**，同步结果在 Stop 时丢弃，**写了个寂寞**
  3. 用户在**编辑态**看不到新字段（而编辑态才是需要看的时候）
- **否决。**

**结论：方案 A。**

---

### 4.5 决策点 5：字段值写回 `Fields` 的策略（直接档 vs 中转档）

`ScriptFieldValue` 是 variant（P2.4 决策点 4.2），控件能绑定什么类型是固定的 —— 所以写回策略分**两档**：

#### 直接档：控件类型与载荷类型一致，传引用原地改（**首选**）

```cpp
case ScriptFieldWidgetKind::Float:      // Float 类型
{
    // std::get 返回载荷的引用，控件原地改 -> 一行收工，返回值可以忽略
    UI::PropertyFloat(field.Name.c_str(), std::get<float>(fieldValue.Data));
    break;
}
```

适用：`Bool`、`Int`（int32）、`Float`、`Vector2` / `Vector3` / `Vector4`、`Color`、`Entity`（`UUID`）。

#### 中转档：控件装不下载荷，临时变量 + 返回 true 才写回（**必须**）

```cpp
case ScriptFieldWidgetKind::Float:      // Double 类型
{
    // PropertyFloat 是 float&，装不下 double：临时中转，只有用户真的改了才写回
    float temp = static_cast<float>(std::get<double>(fieldValue.Data));
    if (UI::PropertyFloat(field.Name.c_str(), temp))
    {
        fieldValue.Data = static_cast<double>(temp);
    }
    break;
}
```

适用：8/16 位整数（int32 中转）、`UInt` / `Long` / `ULong`（int64 中转）、`Double`（float 中转）、`Quaternion`（`vec4(w,x,y,z)` 中转）、`String`（char 缓冲中转）、资产引用（`Ref<T>` 向下转换中转）。

- **为什么"改才写回"是必须的而不是可选的**：中转本身是有损的（`double → float`、`uint64 → int64`、`Ref<Asset> → Ref<Material>`）。**用户没碰控件时把截断值写回去 = 每帧静默磨损数据**（`double` 的精度、`ulong` 的高位会在 Inspector 挂着的几十帧里被悄悄吃掉）。只有控件返回 `true`（用户真的编辑了）才允许有损写回 —— 这时损失是用户动作的一部分，可接受。
- **直接档为什么可以忽略返回值**：载荷与控件同型，改不改都是精确读写，没有磨损问题。

#### 被否决的写法

- **全部走中转档**：直接档也先拷出来再判断写回 —— 多一层拷贝、多一个忘写回的可能，没有任何收益。
- **收集返回值做"场景标脏"**：`Draw_Script` 的返回类型是 `void`（组件 Inspector 的既定契约），收集了也传不出去；将来要做（E-TODO-08）得改整条 Inspector 接口。**本 Phase 不做。**

> ⚠️ **实现要点**：循环里必须用**引用**取出 map 元素：
> ```cpp
> ScriptFieldValue& fieldValue = it->second;      // 必须引用，否则改动落在副本上
> ```
> 用 `auto` 会拷贝，就是"拖了没反应"的经典 bug。规范 §13.9 里"涉及所有权/引用语义应显式声明"正好适用于这里 —— 这里**不要**用 `auto`。

---

## 5. 实现步骤（按依赖顺序）

每步完成后 `Lucky` 与 `Luck3DApp` 都应能编译通过。

### Step 1：加静默解析接口

**文件 A**：`Lucky/Source/Lucky/Scripting/ScriptEngine.h`

在 `ResolveScriptClass` 声明**之后**加：

```cpp
        /// <summary>
        /// 按简单类名解析脚本类，与 ResolveScriptClass 行为相同但不写日志
        /// 供每帧调用的展示路径使用（Inspector 的可用性提示），避免逐帧刷日志
        /// </summary>
        /// <param name="className">简单类名（通常是 .cs 的文件名词干）</param>
        /// <returns>脚本类；未找到或有多个同名类时返回 nullptr</returns>
        static Ref<ScriptClass> TryResolveScriptClass(const std::string& className);
```

在**私有区**加内部实现声明：

```cpp
        /// <summary>
        /// 解析脚本类的统一实现
        /// </summary>
        /// <param name="className">简单类名</param>
        /// <param name="logDiagnostics">是否输出诊断日志</param>
        static Ref<ScriptClass> ResolveScriptClassImpl(const std::string& className, bool logDiagnostics);
```

**要点**：`ResolveScriptClassImpl` 放**私有区**（调用方只有同类的两个公开方法）；`ScriptClass` 已是 `ScriptEngine` 的嵌套友元类型，不需要额外友元声明。

**文件 B**：`Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`

把现有的 `ResolveScriptClass` 实现**改造成** `ResolveScriptClassImpl`，再加两个薄壳：

```cpp
    Ref<ScriptClass> ScriptEngine::ResolveScriptClass(const std::string& className)
    {
        return ResolveScriptClassImpl(className, true);
    }

    Ref<ScriptClass> ScriptEngine::TryResolveScriptClass(const std::string& className)
    {
        return ResolveScriptClassImpl(className, false);
    }

    Ref<ScriptClass> ScriptEngine::ResolveScriptClassImpl(const std::string& className, bool logDiagnostics)
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
                hasMultipleMatches = true;
                matchedFullNames += ", " + fullName;
                continue;
            }

            matchedClass = scriptClass;
            matchedFullNames = fullName;
        }

        if (hasMultipleMatches)
        {
            if (logDiagnostics)
            {
                LF_CORE_ERROR("ScriptEngine::ResolveScriptClass - Class name '{0}' matches multiple script classes: {1}. Please rename the file or adjust its namespace to make it unique.", className, matchedFullNames);
            }
            return nullptr;
        }

        if (!matchedClass && logDiagnostics)
        {
            LF_CORE_ERROR("ScriptEngine::ResolveScriptClass - Script class '{0}' not found. Please make sure the script has been compiled and its class name matches the file name.", className);
        }

        return matchedClass;
    }
```

**要点**：

- **`hasMultipleMatches` 分支里的日志现在被 `logDiagnostics` 包住**；但注意 `matchedFullNames` 的收集**不能**被包住（它同时用于匹配逻辑本身），所以只包 return 之前的那一段日志
- **未找到的分支要重构成"先判 `!matchedClass` 再在内部判 `logDiagnostics`"**：原写法是 `if (!matchedClass) { log; return nullptr; }`，现在必须拆开，否则静默路径会提前 return 掉（结果一样，但代码重复）。上面给的是合并写法
- 两个薄壳方法**放在 `Impl` 之前或之后都可以**，但建议按声明顺序（`ResolveScriptClass` → `TryResolveScriptClass` → `Impl`）保持可读
- **`Impl` 不放在匿名命名空间**：它要访问 `s_Data`（文件作用域静态），且是类的私有静态成员，放类里最自然

### Step 2：加 `ScriptInstance::SetFieldValues`

**文件 A**：`Lucky/Source/Lucky/Scripting/ScriptEngine.h`

在 `ScriptInstance` 的 public 区，`InvokeAwake` **之前**加：

```cpp
        /// <summary>
        /// 把字段表的值写入托管对象
        /// 必须在 InvokeAwake 之前调用：脚本的 Awake 通常会直接读取字段
        /// 字段表里没有的字段会被跳过，托管对象保留其脚本内初始值
        /// </summary>
        /// <param name="fieldMap">字段表（键为字段名）</param>
        void SetFieldValues(const ScriptFieldMap& fieldMap);
```

**文件 B**：`Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`

加在 `ScriptInstance::InvokeAwake` 实现**之前**（这样阅读顺序就是"先灌值再 Awake"）：

```cpp
    void ScriptInstance::SetFieldValues(const ScriptFieldMap& fieldMap)
    {
        if (!m_Instance || !m_ScriptClass)
        {
            return;
        }

        const std::vector<ScriptField>& fields = m_ScriptClass->GetFields();
        for (const ScriptField& field : fields)
        {
            auto it = fieldMap.find(field.Name);
            if (it == fieldMap.end())
            {
                // 字段表里没有这一项：保留托管对象的脚本内初始值
                continue;
            }

            if (it->second.Type != field.Type)
            {
                // 类型不匹配：写下去会按错误的位宽/位模式覆盖内存，必须拦下
                LF_CORE_WARN("ScriptInstance::SetFieldValues - Value type of field '{0}.{1}' does not match the script type, skipped", m_ScriptClass->GetName(), field.Name);
                continue;
            }

            m_ScriptClass->SetFieldValue(m_Instance, field, it->second);
        }
    }
```

**要点**：

- **类型校验是必须的**（不是可选）：`SetFieldValue` 是按 `value.Type` 决定写什么宽度的数据。若 `value.Type` 是 `Float` 而字段实际是 `int`，写入的 4 字节位模式会被解释成一个巨大的整数 —— **静默的数据损坏**。P2.5 的 `SyncScriptFieldMap` 已经在源头修掉了类型不符，这里是第二道保险（因为 `Fields` 也可能来自手改的存档）
- `const ScriptFieldMap&` 参数：只读，符合规范 §13.1
- 日志里带上类名和字段名，便于定位

### Step 3：在 `OnCreateEntityScript` 里插入灌值

**文件**：`Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`

```cpp
    void ScriptEngine::OnCreateEntityScript(Entity entity, const Ref<ScriptClass>& scriptClass)
    {
        LF_CORE_ASSERT(scriptClass, "ScriptEngine::OnCreateEntityScript - scriptClass must not be null");

        Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(scriptClass, entity);

        // 灌值必须在 Awake 之前：脚本的 Awake 通常会直接读取字段
        if (entity.HasComponent<ScriptComponent>())
        {
            instance->SetFieldValues(entity.GetComponent<ScriptComponent>().Fields);
        }

        s_Data->EntityInstances[entity.GetUUID()] = instance;

        instance->InvokeAwake();
    }
```

**要点**：

- **`HasComponent<ScriptComponent>()` 要判**：理论上调用方（`Scene::OnRuntimeStart`）就是从 `ScriptComponent` 遍历过来的，一定存在。但 `OnCreateEntityScript` 是公开接口，判一下属于廉价防御，**并且**避免 `GetComponent` 在组件缺失时触发断言
- **注册进 `EntityInstances` 的时机保持在灌值之后**：`SetFieldValues` 不需要实例表；把注册放在紧挨 `InvokeAwake` 之前，保持"注册完成即可被 Awake 里的逻辑观察到"这个既有语义
- **`ScriptEngine.cpp` 可能需要补 `#include "Lucky/Scene/Components/ScriptComponent.h"`**：目前它通过 `ScriptEngine.h → Scene/Entity.h` 能拿到 `Entity`，但 `ScriptComponent` 的完整定义不一定可见。**编译报"未定义类型 ScriptComponent"时补这个 include**（按规范 §3.3 放工程内头分组）

### Step 4：反序列化后同步字段表

**文件**：`Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp`

在 `Deserialize_Script` 的**末尾**（读完 `Fields` 段之后）加：

```cpp
            // 与脚本当前的字段列表对齐：
            // 场景可能是"脚本加过字段之后"才打开的，此时 Fields 里缺少新字段的初始值
            if (sc.ScriptAsset)
            {
                ScriptEngine::SyncScriptFieldMap(sc.ScriptAsset, sc.Fields);
            }
```

**要点**：

- **放在 `Fields` 段读取之后**：先读存档，再对齐（对齐会补齐缺失项、清理多余项、修正类型不符项）
- **必须判 `sc.ScriptAsset`**：脚本引用为空时不需要同步（`SyncScriptFieldMap` 内部也判了，但这里判一下能省一次函数调用，且语义更清楚）
- 该文件需要 `#include "Lucky/Scripting/ScriptEngine.h"`（若尚未包含）。**注意 include 顺序**（规范 §3.3）
- **不要把同步放在函数开头的早退分支之后就算完** —— 有两个早退分支（`!node`、`!handleNode`），它们早退时没有脚本引用，本来就不需要同步，所以放在末尾是正确且安全的

### Step 5：`UI/` 补两个控件（`PropertyLong` / `PropertyEntityRef`）

**文件**：`Lucky/Source/Lucky/UI/PropertyGrid.h`（声明）+ `Lucky/Source/Lucky/UI/PropertyGrid.cpp`（实现）

**1) `PropertyLong`** —— 64 位整数拖拽，给 `UInt` / `Long` / `ULong` 字段用：

```cpp
    /// <summary>
    /// 64 位整数拖拽属性（给 UInt / Long / ULong 字段用）
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="value">64 位整数值引用</param>
    /// <param name="delta">拖拽速度（默认 1.0）</param>
    /// <param name="min">最小值（min == max 时不夹取）</param>
    /// <param name="max">最大值</param>
    /// <returns>值是否被修改</returns>
    bool PropertyLong(const char* label, int64_t& value, float delta = 1.0f, int64_t min = 0, int64_t max = 0);
```

实现**整段照抄 `PropertyInt`**（`PropertyGrid.cpp:147`），只把 `DragInt(...)` 换成对 `ImGui::DragScalar(GenerateID(), ImGuiDataType_S64, &value, delta, ...)` 的调用（若 UI 层已有 `DragScalar` 封装则优先用封装）；`min == max` 时不夹取的语义与 `PropertyInt` 保持一致。

**2) `PropertyEntityRef`** —— 实体引用槽：

```cpp
    /// <summary>
    /// 实体引用属性：显示目标实体名，支持从场景树拖入实体赋值，可一键清空
    /// 不校验目标实体上挂的是什么脚本（见 P2.4 决策点 4.10 的既定边界）
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="entityID">实体 UUID（0 表示空引用）</param>
    /// <param name="scene">当前场景（用于把 UUID 解析成实体名）</param>
    /// <returns>值是否被修改</returns>
    bool PropertyEntityRef(const char* label, UUID& entityID, Scene* scene);
```

实现要点：

- **声明在 `PropertyGrid.h`，`Scene` 前向声明即可**（`class Scene;`，指针参数不需要完整类型）；`PropertyGrid.cpp` 里 include `Lucky/Scene/Scene.h` 与 `Lucky/Scene/Components/NameComponent.h`
- **显示名**：`entityID != 0 && scene` 时用 `scene->TryGetEntityWithUUID(entityID)` 取 `NameComponent` 的名字；取不到（空引用 / 实体已删）显示 `"None (Entity)"`
- **拖放目标**：`ImGui::AcceptDragDropPayload(DragDrop::EntityHierarchy)`（payload 是 `UUID`，场景树节点已是拖放源，`SceneHierarchyPanel.cpp:518`），`IsDelivery()` 时写入 `entityID`；`DragDrop::EntityHierarchy` 常量在 `Lucky/Editor/DragDropPayloads.h`
- **清空按钮**：值列右侧放一个 `ImGui::SmallButton("X")`，点击把 `entityID` 置 0 并返回 true —— **不能省**，否则引用没法解除
- 形态（`BeginPropertyGrid` / `PropertyLabel` / `PropertyValueBegin` / `EndPropertyGrid`、悬停高亮）照 `PropertyAsset<T>`（`PropertyGrid.h:172`）的骨架

**3) `Entity` 补一个场景访问器**（`PropertyEntityRef` 与 `Draw_Script` 需要把实体 UUID 解析成名字，而 `Entity::m_Scene` 是私有、没有公开 getter）：

`Lucky/Source/Lucky/Scene/Entity.h` 公开区加一行：

```cpp
        /// <summary>
        /// 获取实体所属场景
        /// </summary>
        Scene* GetScene() const { return m_Scene; }
```

### Step 6：Inspector 画字段控件

**文件**：`Lucky/Source/Lucky/Editor/ComponentInspectors.cpp`

**1) 文件顶部匿名命名空间加两个整数中转辅助**（把 8 种整数宽度的存取收在一处，避免 `Draw_Script` 里嵌套 switch）：

```cpp
namespace
{
    // 整数字段载荷统一读成 int64（显示用；ULong 超 int64 上限时截断，不写回就不损坏）
    int64_t GetFieldValueAsInt64(const Lucky::ScriptFieldValue& value)
    {
        switch (value.Type)
        {
            case Lucky::ScriptFieldType::SByte:  return std::get<int8_t>(value.Data);
            case Lucky::ScriptFieldType::Byte:   return std::get<uint8_t>(value.Data);
            case Lucky::ScriptFieldType::Short:  return std::get<int16_t>(value.Data);
            case Lucky::ScriptFieldType::UShort: return std::get<uint16_t>(value.Data);
            case Lucky::ScriptFieldType::Int:    return std::get<int32_t>(value.Data);
            case Lucky::ScriptFieldType::UInt:   return std::get<uint32_t>(value.Data);
            case Lucky::ScriptFieldType::Long:   return std::get<int64_t>(value.Data);
            case Lucky::ScriptFieldType::ULong:  return static_cast<int64_t>(std::get<uint64_t>(value.Data));
            default:                             return 0;
        }
    }

    // 把控件编辑结果按字段实际类型写回载荷
    void SetFieldValueFromInt64(Lucky::ScriptFieldValue& value, int64_t newValue)
    {
        switch (value.Type)
        {
            case Lucky::ScriptFieldType::SByte:  value.Data = static_cast<int8_t>(newValue); break;
            case Lucky::ScriptFieldType::Byte:   value.Data = static_cast<uint8_t>(newValue); break;
            case Lucky::ScriptFieldType::Short:  value.Data = static_cast<int16_t>(newValue); break;
            case Lucky::ScriptFieldType::UShort: value.Data = static_cast<uint16_t>(newValue); break;
            case Lucky::ScriptFieldType::Int:    value.Data = static_cast<int32_t>(newValue); break;
            case Lucky::ScriptFieldType::UInt:   value.Data = static_cast<uint32_t>(newValue); break;
            case Lucky::ScriptFieldType::Long:   value.Data = newValue; break;
            case Lucky::ScriptFieldType::ULong:  value.Data = static_cast<uint64_t>(newValue); break;
            default: break;
        }
    }
}
```

**2) `Draw_Script` 重写为按 `ScriptFieldWidgetKind` 分发**：

```cpp
        void Draw_Script(Entity entity)
        {
            ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

            if (UI::PropertyAsset("Script", sc.ScriptAsset))
            {
                // 脚本被赋值/更换：按脚本当前的字段列表重建字段表（含初始值）
                ScriptEngine::SyncScriptFieldMap(sc.ScriptAsset, sc.Fields);
            }

            if (!sc.ScriptAsset)
            {
                return;
            }

            // 展示路径用静默解析：错误提示本身就是给用户的反馈，不需要再刷日志
            Ref<ScriptClass> scriptClass = ScriptEngine::TryResolveScriptClass(sc.ScriptAsset->GetClassName());
            if (!scriptClass)
            {
                ImGui::TextColored({0.9f, 0.35f, 0.35f, 1.0f}, "Script class not found.\nMake sure the script is compiled and the class name matches the file name.");
                return;
            }

            for (const ScriptField& field : scriptClass->GetFields())
            {
                auto it = sc.Fields.find(field.Name);
                if (it == sc.Fields.end())
                {
                    continue;
                }

                // 必须用引用：控件是原地修改，取副本会导致"拖了没反应"
                ScriptFieldValue& fieldValue = it->second;
                const char* label = field.Name.c_str();

                // 按控件种类分发：直接档传 std::get 的引用原地改；中转档临时变量、返回 true 才写回（决策点 4.5）
                switch (GetScriptFieldTypeInfo(fieldValue.Type).Widget)
                {
                    case ScriptFieldWidgetKind::Checkbox:
                    {
                        UI::PropertyCheckbox(label, std::get<bool>(fieldValue.Data));
                        break;
                    }
                    case ScriptFieldWidgetKind::Int:
                    {
                        const bool isSmall = (fieldValue.Type != ScriptFieldType::UInt &&
                                              fieldValue.Type != ScriptFieldType::Long &&
                                              fieldValue.Type != ScriptFieldType::ULong);
                        int64_t temp = GetFieldValueAsInt64(fieldValue);
                        bool modified = false;
                        if (isSmall)
                        {
                            int temp32 = static_cast<int>(temp);
                            modified = UI::PropertyInt(label, temp32);
                            temp = temp32;
                        }
                        else
                        {
                            modified = UI::PropertyLong(label, temp);
                        }
                        if (modified)
                        {
                            SetFieldValueFromInt64(fieldValue, temp);
                        }
                        break;
                    }
                    case ScriptFieldWidgetKind::Float:
                    {
                        if (fieldValue.Type == ScriptFieldType::Double)
                        {
                            float temp = static_cast<float>(std::get<double>(fieldValue.Data));
                            if (UI::PropertyFloat(label, temp))
                            {
                                fieldValue.Data = static_cast<double>(temp);
                            }
                        }
                        else
                        {
                            UI::PropertyFloat(label, std::get<float>(fieldValue.Data));
                        }
                        break;
                    }
                    case ScriptFieldWidgetKind::Float2:
                    {
                        UI::PropertyFloat2(label, std::get<glm::vec2>(fieldValue.Data));
                        break;
                    }
                    case ScriptFieldWidgetKind::Float3:
                    {
                        UI::PropertyFloat3(label, std::get<glm::vec3>(fieldValue.Data));
                        break;
                    }
                    case ScriptFieldWidgetKind::Float4:
                    {
                        if (fieldValue.Type == ScriptFieldType::Quaternion)
                        {
                            const glm::quat& quat = std::get<glm::quat>(fieldValue.Data);
                            glm::vec4 temp(quat.w, quat.x, quat.y, quat.z);
                            if (UI::PropertyFloat4(label, temp))
                            {
                                fieldValue.Data = glm::quat(temp.x, temp.y, temp.z, temp.w);
                            }
                        }
                        else
                        {
                            UI::PropertyFloat4(label, std::get<glm::vec4>(fieldValue.Data));
                        }
                        break;
                    }
                    case ScriptFieldWidgetKind::Color:
                    {
                        UI::PropertyColor(label, std::get<glm::vec4>(fieldValue.Data));
                        break;
                    }
                    case ScriptFieldWidgetKind::Text:
                    {
                        // PropertyString 是 char 缓冲：中转，改才写回
                        const std::string& text = std::get<std::string>(fieldValue.Data);
                        char buffer[256];
                        strncpy_s(buffer, text.c_str(), sizeof(buffer) - 1);
                        if (UI::PropertyString(label, buffer, sizeof(buffer)))
                        {
                            fieldValue.Data = std::string(buffer);
                        }
                        break;
                    }
                    case ScriptFieldWidgetKind::EntityRef:
                    {
                        UI::PropertyEntityRef(label, std::get<UUID>(fieldValue.Data), entity.GetScene());
                        break;
                    }
                    case ScriptFieldWidgetKind::AssetRef:
                    {
                        // PropertyAsset<T> 需要 Ref<T>&：向下转换中转，改才写回；空 Ref 转换安全
                        const Ref<Asset>& asset = std::get<Ref<Asset>>(fieldValue.Data);
                        bool modified = false;
                        Ref<Asset> newAsset;
                        switch (fieldValue.Type)
                        {
                            case ScriptFieldType::Material:
                            {
                                Ref<Material> temp = std::static_pointer_cast<Material>(asset);
                                modified = UI::PropertyAsset(label, temp);
                                newAsset = temp;
                                break;
                            }
                            case ScriptFieldType::Mesh:
                            {
                                Ref<Mesh> temp = std::static_pointer_cast<Mesh>(asset);
                                modified = UI::PropertyAsset(label, temp);
                                newAsset = temp;
                                break;
                            }
                            case ScriptFieldType::Texture2D:
                            {
                                Ref<Texture2D> temp = std::static_pointer_cast<Texture2D>(asset);
                                modified = UI::PropertyAsset(label, temp);
                                newAsset = temp;
                                break;
                            }
                            case ScriptFieldType::Script:
                            {
                                Ref<Script> temp = std::static_pointer_cast<Script>(asset);
                                modified = UI::PropertyAsset(label, temp);
                                newAsset = temp;
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                        if (modified)
                        {
                            fieldValue.Data = newAsset;
                        }
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
        }
```

**3) 补 include**：`static_pointer_cast` 需要四种资产类型的完整定义 —— 确认/补 `Lucky/Renderer/Material.h`、`Lucky/Renderer/Mesh.h`、`Lucky/Renderer/Texture.h`（`Lucky/Asset/Script.h` 在 P2.3 已加）。

**要点**：

- **`if (!sc.ScriptAsset) return;` 提前返回**：没有脚本就没有字段可画。注意这个早退发生在 `PropertyAsset` 之后 —— 用户把脚本拖成 `None` 时，同步已经在上一段执行过（清空了 `Fields`），所以早退不会漏掉清理
- **`TryResolveScriptClass` 替换掉原来的 `ResolveScriptClass`**（决策点 4.2）
- **`fieldValue` 显式写成 `ScriptFieldValue&`，不要用 `auto`**（决策点 4.5 的警示；规范 §13.9）
- **控件用 `field.Name.c_str()`** 当 label（决策点 4.3）
- **`switch` 判的是 Widget 而不是 Type**：22 种类型塌缩成 10 个分支；类型相关的差异（整数宽度 / `Double` / `Quaternion` / 四种资产）在分支内部用 `fieldValue.Type` 二次分发 —— 这是 P2.4 把 `Widget` 放进类型表的收益
- **`fieldValue.Type` 与 `field.Type` 正常情况一致**（同步保证），**不在这里做类型校验**（P2.5 的同步和 Step 2 的灌值各有一道）
- `String` 缓冲定长 256：超长的字符串会被截断显示，**不写回不损坏**；需要更长时再调
- `strncpy_s` 是 MSVC 安全函数，本工程只支持 MSVC ✓

### Step 7：编译验证

- Debug 编译 `Lucky` → 通过
- Debug 编译 `Luck3DApp` → 通过
- 不需要重跑 premake（无新增文件）
- **若报 `ScriptComponent` 未定义**：`ScriptEngine.cpp` 补 `#include "Lucky/Scene/Components/ScriptComponent.h"`
- **若报 `SyncScriptFieldMap` 未定义**：`ComponentSerializers.cpp` 补 `#include "Lucky/Scripting/ScriptEngine.h"`
- **若报 `ScriptFieldMap` 未定义**：`ScriptEngine.h` 确认 include 了 `Lucky/Scripting/ScriptFieldValue.h`（P2.4 已加）
- **若报 `DragDrop::EntityHierarchy` 未定义**：`PropertyGrid.cpp` 补 `#include "Lucky/Editor/DragDropPayloads.h"`

---

## 6. 疑点问答

### 6.1 为什么灌值要放在 `EntityInstances` 注册之前？

严格说两者顺序无关（`SetFieldValues` 不查实例表）。放在注册之前是为了让"注册"紧挨 `InvokeAwake()` —— 保持一个清晰的阅读顺序：**建对象 → 填数据 → 登记 → 触发生命周期**。

如果你觉得"先登记再灌值"更符合"对象先存在"的直觉，换过来也完全可以，**但灌值必须仍在 `InvokeAwake()` 之前**。

### 6.2 脚本的 `Awake` 里改了字段值，Inspector 里的值会被同步回去吗？

**不会，也不需要。**

- 运行态操作的是 `Scene::Copy` 的副本，改动在 Stop 时随副本丢弃
- 编辑器场景里的 `Fields` 是编辑态数据，运行态不回写
- 这与 Unity 的行为一致：Play 中改脚本字段，Stop 后回到编辑态的原值

### 6.3 如果 `Fields` 里的 `Type` 是 `None`（存档被手改坏）会怎样？

- Inspector：`switch` 落到 `default`，**不画任何控件**，该字段静默不可见
- 运行态：`SetFieldValues` 里 `it->second.Type != field.Type`（`None` vs `Float`）→ 打 WARN 并跳过 → 托管对象保留脚本初值
- **不会崩**。而且 P2.5 的 `SyncScriptFieldMap` 在打开场景时就会把这个 `None` 项按"类型不符"修掉（用脚本初值覆盖），所以正常情况下根本走不到这里

### 6.4 为什么不在 Inspector 里对"未同步的字段"显示个提示？

因为决策点 4.4-A 已经把同步放在了打开场景时，**正常流程下不存在"未同步"状态**。如果出现了，说明同步失败（脚本解析不了），而那已经有红色提示了 —— 再加一层"未同步"提示只会让用户面对两个提示猜哪个是根因。

### 6.5 字段很多时 Inspector 会很长，怎么办？

当前不管（P2.4 的疑点问答 6.6 已经说明"字段顺序不承诺、不做排序"）。字段多了本来就应该用分组或自定义 Inspector —— 那是后续增强，不是本 Phase 的问题。

### 6.6 一个实体的脚本字段值改动了，怎么知道场景"脏"了（需要保存）？

**当前做不到，也不在本 Phase 范围。** 原因是 `Draw_Script` 返回 `void`，属性的"是否被修改"传不出去（决策点 4.5）。要支持"场景标脏"需要改整条 `ComponentInspectors` 的接口契约（所有 `Draw_Xxx` 都返回 `bool`）—— 那是 E-TODO-08「场景修改标记」的活。

**但数据是安全的**：控件的改动是**原地写进** `sc.Fields`，只要用户按 Ctrl+S 就会存下来。

### 6.7 拖入脚本后立刻 `SyncScriptFieldMap`，如果脚本解析不了会怎样？

`SyncScriptFieldMap` 内部用的是**会打日志**的 `ResolveScriptClass` → 会打一条 ERROR，然后**保留 `Fields` 现有内容**（P2.5 的 6.2 分支：解析失败不清空）。同时 Inspector 的红色提示也会出现。

用户看到的是：红字提示 + 一条 ERROR 日志，**没有值被清掉**。这是正确的降级行为。

### 6.8 为什么 Inspector 用静默解析，而 `SyncScriptFieldMap` 用带日志的？

因为两者的**调用动机**不同：

| 调用方 | 动机 | 该不该打日志 |
|--------|------|-------------|
| Inspector（每帧） | 决定"要不要画红字" —— 纯粹的展示判断 | **不该**。红字就是给用户的反馈 |
| `SyncScriptFieldMap` | 一次维护动作（拖入脚本 / 打开场景） | **该**。这是"用户做了什么 → 结果如何"的记录 |

同一个判断，在不同动机下对日志的需求相反 —— 这正是要拆成两个接口的原因。

---

## 7. 验收标准

1. **编译通过**：`Lucky` 与 `Luck3DApp` 的 Debug / Release / Dist 三个 configuration 全通过
2. **控件出现**：给 `PlayerController` 加 `public float Speed = 3.0f;` / `public int Level = 1;` / `public bool LogPosition = false;` / `public Vector3 Offset = new Vector3(1, 2, 3);` → 重新编译 → 重启 → 打开场景 → Inspector 里出现 4 个控件，**且显示的是脚本里写的初值**（`3.0` / `1` / 未勾选 / `(1,2,3)`）
3. **拖动生效并持久化**：把 `Speed` 拖到 `7.5` → 保存场景 → 重启 → 打开 → 仍是 `7.5`；存档 YAML 里 `Value: 7.5`
4. **★ 脚本真的读到调过的值**：`Update` 里 `Debug.Log("speed = " + Speed);` → 在 Inspector 把 `Speed` 调成 `7.5` → Play → 日志**是 `speed = 7.5`**（**这是整个 Phase 2 的最终验证点**）
5. **灌值早于 Awake**：`Awake` 里 `Debug.Log("awake speed = " + Speed);` → Inspector 调成 `7.5` → Play → 日志是 `awake speed = 7.5`（**证明灌值在 Awake 之前**；若是 `3`，说明顺序反了）
6. **字段表没有的字段回落到脚本初值**：手动从存档里删掉 `Speed` 项 → 重开场景 → 同步会补回来（决策点 4.4-A）；若手工绕过同步直接 Play，脚本读到的是脚本初值 `3.0`，**不报错**
7. **新字段自动出现**：脚本加一个新字段 → 重新编译 → 重启 → 打开场景 → Inspector 立刻出现该控件（**这一条专门验 Step 4 的反序列化同步**）
8. **删字段自动消失**：脚本删掉一个字段 → 重新编译 → 重启 → 打开场景 → 该控件消失，日志有 `no longer exists in the script`
9. **不刷日志**：让脚本解析失败（把 `.cs` 改名不编译）→ 拖入 → 停在编辑器里**观察 10 秒** → 红色提示一直在，但**日志里只有一条 ERROR**（不是几十条）。**这一条专门验 Step 1 的静默解析**
10. **类型不符被拦**：手改存档把 `Speed` 的 `Type` 写成 `Int`（值 `Value: 3`），脚本里仍是 `float` → 重开场景 → 同步会重置为脚本初值；若绕过同步直接 Play，日志出现 `does not match the script type, skipped` 且**不崩**
13. **字符串编辑往返**：加 `public string Title = "player";` → Inspector 出现文本框 → 改成 `"boss"` → Play 后脚本读到 `"boss"` → 存盘重开仍是 `"boss"`（验 Text 中转与 `PropertyString`）
14. **实体引用槽**：加 `public Entity Target;` → Inspector 出现实体槽 → 从场景树拖一个实体进去 → Play 后脚本能用它访问组件 → 点 `X` 清空后显示 `None (Entity)`（验 `PropertyEntityRef` 与 `DragDrop::EntityHierarchy` 链路）
15. **大整数不被静默磨损**：加 `public long BigNumber = 5000000000;`（超 int32）→ Inspector 显示正确 → **不碰它**、挂着 Inspector 若干秒后存盘 → 存档里仍是 `5000000000`（验"改才写回"——若是截断值说明中转档写回条件错了）
16. **Double 精度**：加 `public double Pi = 3.141592653589793;` → 不编辑的情况下存盘重开，值不损失（同上，验中转档）；编辑成 `2.5` 后写回正确
11. **`OnDestroy` 链路未退化**：Play 中移除 ScriptComponent → 仍有 `OnDestroy` 打印（P1 的成果没被破坏）
12. **代码规范**：通过人工 checklist —— 控制语句全带花括号（§5.2）；`switch` 各 case 带花括号；范围 for 用 `auto`，但**字段值的引用显式写成 `ScriptFieldValue&`**（§13.9）；公有接口有 `/// <summary>` 中文注释（§4.1）；无"为对齐而对齐"的空格（§5.4）；无引用外部文档的注释、无"P3 会补齐"这类阶段性注释

> ⚠️ **第 4 条和第 5 条是 Phase 2 的收口判据**，必须真跑一次看日志。第 4 条证明"值进了脚本"，第 5 条证明"进得够早"。两条都过，Phase 2 才算真的完成。
>
> ⚠️ **第 9 条容易被忽略**：它是唯一能发现"Inspector 逐帧刷日志"的用例，而不刷日志正是本 Phase 特意拆出静默接口的原因。观察时不要只看一眼就走，**要停在编辑器里数秒**。

---

## 8. 对下一 Phase 的接线点

| 位置 | 后续 Phase | 打开方式 |
|------|-----------|---------|
| `ScriptInstance::SetFieldValues` | **P3 热重载** | 程序集重新加载后重建脚本实例时，直接复用这个函数把 `Fields` 灌进新对象 |
| `TryResolveScriptClass` | 任何每帧调用的展示路径 | 复用"有日志 / 静默"这对接口的范式 |
| `Deserialize_Script` 里的同步调用 | 若将来脚本字段类型扩展 | 新增类型时这里不需要动（同步逻辑不区分类型） |
| 控件映射（`Draw_Script` 的 Widget `switch`） | 类型扩展 | 加类型时：P2.4 类型表加一行 + 序列化读/写各一个 case + 这里对应 Widget 分支补载荷存取（Widget 种类不够用时才在 `ScriptFieldWidgetKind` 里加） |
| `Draw_Script` 忽略直接档控件返回值 | **E-TODO-08 场景修改标记** | 需要"场景标脏"时，把 `Draw_Xxx` 的契约改成返回 `bool`，此处收集 `UI::PropertyXxx` 的返回值（中转档已在用返回值） |
| `PropertyEntityRef` | 后续增强 | 校验"目标实体挂了对应脚本"（P2.4 决策点 4.10 的既定边界）；场景树以外的实体选择方式（弹窗搜索等） |

---

## 9. 变更清单速览

- **新增文件**：无
- **修改文件（6 个）**
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.h`：加 `TryResolveScriptClass`（公开）、`ResolveScriptClassImpl`（私有）、`ScriptInstance::SetFieldValues`
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`：`ResolveScriptClass` 拆成 `Impl` + 两个薄壳；加 `SetFieldValues` 实现；`OnCreateEntityScript` 插入灌值（可能补 `ScriptComponent.h` include）
  - `Lucky/Source/Lucky/Editor/ComponentInspectors.cpp`：`Draw_Script` 改用 `TryResolveScriptClass` + 按 `ScriptFieldWidgetKind` 画控件；匿名命名空间加两个整数中转辅助；补三种资产类型的 include
  - `Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp`：`Deserialize_Script` 末尾加一次 `SyncScriptFieldMap`（补 `ScriptEngine.h` include）
  - `Lucky/Source/Lucky/UI/PropertyGrid.h` / `PropertyGrid.cpp`：新写 `PropertyLong` 与 `PropertyEntityRef`
  - `Lucky/Source/Lucky/Scene/Entity.h`：加 `GetScene()` 访问器（一行）
- **删除**：无
- **不改动**：`Scene/Scene.cpp`（**刻意不改**，见约束 3）、`Scene/Components/ScriptComponent.h`、`ScriptFieldValue.h`、托管 C# 代码、premake
- **测试样本**：`Luck3DApp/Project/Assets/Scripts/PlayerController.cs` 建议保留 `Speed` 等字段（供验收第 2～5 条使用）；验收第 13～16 条的 `Title` / `Target` / `BigNumber` / `Pi` 测完可删

---

## 10. 参考

- 编码规范 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：§3.3 include 顺序、§4.1 XML 注释、§5.2 强制花括号、§5.4 对齐规则、§13.1 const 正确性、§13.7 错误处理、§13.9 auto 使用规范
- 前置详设 [Phase2.3_ScriptComponent_AssetRef.md](Phase2.3_ScriptComponent_AssetRef.md)（`Draw_Script` 的上一形态、`OnCreateEntityScript` 的签名改造）
- 前置详设 [Phase2.4_ScriptField_Metadata.md](Phase2.4_ScriptField_Metadata.md)（`ScriptField` / `GetFields` / `SetFieldValue`）
- 前置详设 [Phase2.5_ScriptField_Serialization.md](Phase2.5_ScriptField_Serialization.md)（`Fields` / `SyncScriptFieldMap` / 序列化；其 §8 关于"进 Play 前同步"的设想已被本 Phase 4.4-D 否决）
- UI 控件 `UI/PropertyGrid.h`：`PropertyFloat` / `PropertyFloat2-4` / `PropertyInt` / `PropertyCheckbox` / `PropertyColor` / `PropertyString` / `PropertyAsset<T>`；拖放常量 `Lucky/Editor/DragDropPayloads.h`（`EntityHierarchy`）
- 路径与时序 `Lucky/Source/Lucky/Core/Application.cpp`（`ScriptEngine::Init` 与打开场景的先后）
- 脚本系统路线图 [ScriptSystem_Roadmap.md](ScriptSystem_Roadmap.md) 第 4 节 Phase 2 出口标准「在 `.cs` 里写 `public float Speed = 3.0f;`，Inspector 出现 Speed 拖动条，保存场景后重新打开仍保留」
