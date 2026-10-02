# Phase 2.3：ScriptComponent 改为脚本资产引用

## 1. 概述

把 `ScriptComponent` 从"存类名字符串"改成"存脚本资产引用"，Inspector 用 `UI::PropertyAsset<Script>` 绘制 —— **拖拽能力全部白拿，路线图里的"ClassName 下拉框"正式作废**。

本 Phase 覆盖三件事：

1. **组件数据模型**：`std::string ClassName` → `Ref<Script> ScriptAsset`
2. **序列化**：写/读 `AssetHandle`（照 `MeshRendererComponent` 的材质写法）
3. **运行态接入**：`Scene::OnRuntimeStart` 从"拿字符串查类"改成"由 `Ref<Script>` 解析类"

**决策已定：不做老场景迁移。** 现有 `ScriptTest.luck3d` 里是旧的 `ClassName:` 格式，本 Phase 之后它里面的脚本会丢 —— 按你的决定，删掉重建即可，不为它写迁移代码。

### 1.1 关键约束

- **必须用 `Ref<Script>`，不能用裸 `AssetHandle`。** `Scene.h` 的 `Scene::Copy` 文档写得很明确：
  - 「资产引用（Mesh / Material / Texture / SkyboxMaterial 等 `Ref<Asset>`）：**共享**，副本与源指向同一份资产」
  - 「Asset Handle：**副本为无效 Handle**（副本不进入 AssetRegistry）」

  运行态场景是 `Scene::Copy` 出来的副本。存裸 handle → 副本里失效 → 脚本实例化时解析不到类。存 `Ref<Script>` → 共享，运行态可用。这也是 P2.1 决策点 4.1 选方案 A 的直接后果。
- **`ClassName` 要删掉，不与 `ScriptAsset` 并存。** 两者并存立刻产生"以哪个为准"的歧义，且在序列化、Inspector、运行态三处都要写优先级逻辑。
- **Inspector 必须能显示"脚本解析不了"的状态。** 拖进来的 `.cs` 如果没参与编译，`Ref<Script>` 是有效的，但解析不到类 —— 这个状态**不能静默**，否则用户会以为"拖进去了就能跑"。
- **`ScriptEngine::OnCreateEntityScript` 的签名要改**：原来收 `const std::string& fullClassName`（内部再查 `EntityClasses`），现在由调用方解析好直接传 `Ref<ScriptClass>`。详见决策点 4.1。
- **不依赖 cwd**（规范 §13.10）：反序列化取资产一律走 `AssetManager::GetAsset<T>(handle)`，不要自己拼路径。
- 代码风格遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：组件用 `struct`、`= default` 构造（§13.3）；控制语句强制花括号（§5.2）；智能指针引用显式声明类型（§13.9）。

### 1.2 前置条件

| 依赖 | 说明 |
|------|------|
| P2.1 | `AssetType::Script` + `class Script` + `ScriptImporter` 已就位 |
| P2.2 | `ScriptEngine::ResolveScriptClass(const std::string&)` 已就位 |
| `UI::PropertyAsset<T>` | 已就绪（`UI/PropertyGrid.h`），类型校验靠 `T::StaticAssetType()` |
| `AssetManager::GetAsset<T>` | 已就绪，且 P2.1 已补 `GetExpectedAssetType<Script>` 特化 |
| `AssetHandle` | 已提供 `uint64_t` 构造与 `IsValid()` |
| `ComponentRegistry` | `ScriptComponent` 已在 `ComponentCores.cpp` 注册（Copy / Has / Inspector 三处） |

### 1.3 本 Phase **不做**的事

- 不做字段 `FieldMap`（P2.5）
- 不做字段级 Inspector 控件（P2.6）
- **不做老场景 `ClassName:` 格式的迁移**（已决策：删场景重建）
- 不做脚本编译、不做热重载（P3）
- 不改 `Scene::DestroyEntity` / `OnComponentRemoved`（P1 的成果，本 Phase 不动）

---

## 2. 涉及的文件

### 2.1 新建

无。

### 2.2 修改

| 文件 | 改动 |
|------|------|
| `Lucky/Source/Lucky/Scene/Components/ScriptComponent.h` | 字段换成 `Ref<Script> ScriptAsset`；include 换成 `Asset/Script.h` |
| `Lucky/Source/Lucky/Scripting/ScriptEngine.h` | `OnCreateEntityScript` 签名改为收 `Ref<ScriptClass>` |
| `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp` | 对应改实现（去掉内部查表与找不到的分支） |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | `OnRuntimeStart` 改为"取 `Ref<Script>` → 解析 `ScriptClass` → 实例化" |
| `Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp` | `Serialize_Script` / `Deserialize_Script` 改写 handle |
| `Lucky/Source/Lucky/Editor/ComponentInspectors.cpp` | `Draw_Script` 换成 `UI::PropertyAsset<Script>` + 解析失败提示 |

### 2.3 不修改

- `Scene/Scene.h`：`OnRuntimeStart` 的签名不变（内部实现改）
- `Editor/ComponentInspectors.cpp` 里 `ScriptComponent` 的**注册块**（图标 + 增删回调）：`e.AddComponent<ScriptComponent>()` / `e.RemoveComponent<ScriptComponent>()` 都还能用，不需要改
- `Scene/ComponentCores.cpp`：`CopyComponentValue<ScriptComponent>` 是通用值拷贝，`Ref<Script>` 值拷贝即共享，**不需要改**
- `Asset/*`：P2.1 已完成
- 托管 C# 代码：本 Phase 完全不用改

---

## 3. 现状回顾

### 3.1 `ScriptComponent` 当前形态

`Scene/Components/ScriptComponent.h`：

```cpp
#pragma once

#include <string>

namespace Lucky
{
    /// <summary>
    /// 脚本组件：把用户 C# 脚本类挂到实体上
    /// 只保存类的全名（Namespace.ClassName），实际的托管对象由 ScriptEngine 统一管理
    /// </summary>
    struct ScriptComponent
    {
        std::string ClassName;

        ScriptComponent() = default;
        ScriptComponent(const ScriptComponent& other) = default;
        ScriptComponent(const std::string& className)
            : ClassName(className) {}
    };
}
```

### 3.2 Inspector 当前形态

`Editor/ComponentInspectors.cpp`：

```cpp
        void Draw_Script(Entity entity)
        {
            ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

            char buf[256] = {};
            std::strncpy(buf, sc.ClassName.c_str(), sizeof(buf) - 1);

            if (UI::PropertyString("Class", buf, sizeof(buf)))
            {
                sc.ClassName = buf;
            }
        }
```

### 3.3 运行态当前形态

`Scene/Scene.cpp` 的 `OnRuntimeStart`：

```cpp
    void Scene::OnRuntimeStart()
    {
        ScriptEngine::OnRuntimeStart(this);

        auto view = m_Registry.view<ScriptComponent>();
        for (entt::entity entityHandle : view)
        {
            Entity entity{ entityHandle, this };
            const ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

            if (sc.ClassName.empty())
            {
                continue;
            }
            if (!ScriptEngine::EntityScriptClassExists(sc.ClassName))
            {
                LF_CORE_WARN("Scene::OnRuntimeStart - script class '{0}' not found for entity '{1}'", sc.ClassName, entity.GetName());
                continue;
            }

            ScriptEngine::OnCreateEntityScript(entity, sc.ClassName);
        }

        m_State = SceneState::Play;
    }
```

### 3.4 `OnCreateEntityScript` 当前实现

`Scripting/ScriptEngine.cpp`：

```cpp
    void ScriptEngine::OnCreateEntityScript(Entity entity, const std::string& fullClassName)
    {
        auto it = s_Data->EntityClasses.find(fullClassName);
        if (it == s_Data->EntityClasses.end())
        {
            LF_CORE_WARN("ScriptEngine: entity script class '{}' not found", fullClassName);
            return;
        }

        Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(it->second, entity);
        s_Data->EntityInstances[entity.GetUUID()] = instance;

        instance->InvokeAwake();
    }
```

**这个函数全仓库只有一个调用点**（`Scene::OnRuntimeStart`），所以改签名没有连带影响。

### 3.5 资产引用的序列化/反序列化范式

`Serialization/ComponentSerializers.cpp` 里 `MeshRendererComponent` 的材质列表（**本 Phase 照抄的对象**）：

序列化：

```cpp
            out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
            for (const Ref<Material>& material : mr.Materials)
            {
                out << YAML::BeginMap;
                if (material)
                {
                    out << YAML::Key << "AssetHandle" << YAML::Value << material->GetHandle();
                }
                else
                {
                    out << YAML::Key << "AssetHandle" << YAML::Value << static_cast<uint64_t>(0);
                }
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
```

反序列化：

```cpp
                for (auto materialNode : materialsNode)
                {
                    if (materialNode["AssetHandle"])
                    {
                        // 新格式：通过 AssetHandle 从 AssetManager 获取材质
                        uint64_t handleValue = materialNode["AssetHandle"].as<uint64_t>();
                        AssetHandle handle(handleValue);

                        Ref<Material> material = nullptr;
                        if (handle.IsValid())
                        {
                            material = AssetManager::GetAsset<Material>(handle);
                        }

                        if (!material)
                        {
                            LF_CORE_ERROR("SceneSerializer: Failed to load material asset [{0}]", handleValue);
                            material = Renderer3D::GetInternalErrorMaterial();
                        }

                        mr.Materials.push_back(material);
                    }
                    // ...
                }
```

**三个可复用的细节**：`material->GetHandle()` 直接交给 YAML emitter；读的时候 `.as<uint64_t>()` 再构造 `AssetHandle`；`handle.IsValid()` 判有效性。

### 3.6 `Scene::Copy` 的资产处理契约

`Scene/Scene.h` 中 `Copy` 的 XML 注释（**决定了为什么必须用 `Ref<Script>`**）：

```
    /// 语义边界：
    /// - 组件：值语义完整拷贝（新 registry 不与源共享任何组件存储）
    /// - 资产引用（Mesh / Material / Texture / SkyboxMaterial 等 Ref<Asset>）：共享，副本与源指向同一份资产
    /// - UUID / RootEntityOrder / EnvironmentSettings / ViewportSize / Name：完整拷贝
    /// - Asset Handle：副本为无效 Handle（副本不进入 AssetRegistry）
    /// - SceneState：副本一律初始为 Edit
```

### 3.7 现有脚本场景的序列化格式（将被丢弃）

`Luck3DApp/Project/Assets/Scenes/ScriptTest.luck3d`：

```yaml
    ScriptComponent:
      ClassName: Sandbox.PlayerController
```

本 Phase 之后这个键不再被读取。见决策点 4.2。

### 3.8 编辑器里直接写 `ImGui::` 的先例

`Editor/MaterialEditor.cpp`：

```cpp
                    ImGui::TextColored({1.0f, 0.6f, 0.0f, 1.0f}, " *");
```

`ComponentInspectors.cpp` 目前**只用 `UI::` 包装**，但项目里"在 Editor 层直接用 ImGui"是有先例的。见决策点 4.3。

---

## 4. 关键设计决策（多方案对比）

### 4.1 决策点 1：`OnCreateEntityScript` 的签名怎么改

#### 方案 A：改成收 `Ref<ScriptClass>`（**推荐 ✅**）

```cpp
static void OnCreateEntityScript(Entity entity, const Ref<ScriptClass>& scriptClass);
```

调用方（`Scene::OnRuntimeStart`）负责"`Ref<Script>` → `ResolveScriptClass` → `ScriptClass`"这一段。

- **优点**：
  1. **职责归位**：`Scene` 是唯一知道"实体上挂的是哪个脚本资产"的地方，解析也在这里做最自然；`ScriptEngine` 只管"给我一个类，我把它实例化"
  2. 消除了 `OnCreateEntityScript` 内部的"查不到就 WARN 并 return"分支 —— 解析失败已经在 `ResolveScriptClass` 里报过 ERROR 了，报两次是噪音
  3. 传 `Ref<ScriptClass>` 而不是字符串，**顺带把 P2.4/P2.6 需要的东西准备好了**：将来在实例化时要用 `ScriptClass` 反射字段，签名已经带进来了
- **缺点**：`ScriptEngine` 的公开接口从"收字符串"变成"收对象"，调用方多一行解析代码

#### 方案 B：改成收 `const Ref<Script>&`，`ScriptEngine` 内部解析

```cpp
static void OnCreateEntityScript(Entity entity, const Ref<Script>& scriptAsset);
```

- **优点**：`Scene` 侧改动最小（一行都不用写解析）
- **缺点**：
  1. `ScriptEngine` 要 include `Asset/Script.h` 才能认这个参数类型 —— 让**脚本运行时层依赖资产层的一个具体类型**，比方案 A 的耦合更重（方案 A 用的是 `ScriptClass`，本来就在 `Scripting` 层内）
  2. `Scene` 侧其实**也需要**解析结果：它要在"解析失败"时给出**带实体名的**上下文日志（"实体 Foo 上的脚本解析失败"比"类 X 没找到"更有用）。解析放 `ScriptEngine` 里，`Scene` 就拿不到这个信息了
- **次优。**

#### 方案 C：保留字符串签名不动，`Scene` 解析后拼回全名再传

```cpp
ScriptEngine::OnCreateEntityScript(entity, scriptClass->GetNamespace() + "." + scriptClass->GetName());
```

- **优点**：引擎侧零改动
- **缺点**：**解析出来的对象又拆回字符串、对方再拿字符串查一次表**，纯粹的无用功；而且 `ScriptClass` 的 `GetNamespace()` 可能是空串，拼接逻辑要判空，容易写错。**否决。**

#### 方案 D：新增一个重载，旧的保留

```cpp
static void OnCreateEntityScript(Entity entity, const Ref<ScriptClass>& scriptClass);
static void OnCreateEntityScript(Entity entity, const std::string& fullClassName);   // 旧
```

- **优点**：不破坏任何既有调用
- **缺点**：旧签名的**唯一调用点就在本 Phase 要改的地方**，保留它等于留一个没人用的死接口 + 两套语义。**否决。**

**结论：方案 A。** 注意旧函数体里那句 `LF_CORE_WARN("ScriptEngine: entity script class '{}' not found")` 要**删掉** —— 未找到类的诊断已经在 `ResolveScriptClass` 里做了（P2.2 决策点 4.5）。

---

### 4.2 决策点 2：读到旧的 `ClassName:` 键怎么办

#### 方案 A：`Deserialize_Script` 里"读不到 `AssetHandle` 键就直接返回"，组件留一个空 `ScriptAsset`（**推荐 ✅**）

```cpp
            ScriptComponent& sc = entity.AddComponent<ScriptComponent>();

            YAML::Node handleNode = node["AssetHandle"];
            if (!handleNode)
            {
                // 旧格式（ClassName）：不做迁移，脚本引用留空
                LF_CORE_WARN("SceneSerializer: ScriptComponent uses legacy 'ClassName' format, script reference is dropped. Reassign the script asset in the Inspector.");
                return;
            }
```

- **优点**：
  1. 组件**仍然存在**（`AddComponent` 已经执行），实体的组件构成不发生意外变化 —— 用户打开老场景看到的是"ScriptComponent 在，但脚本是 None"，一眼就知道要重新拖一个
  2. 一条 WARN 交代清楚发生了什么、该怎么办
  3. 零迁移逻辑
- **缺点**：老场景的脚本信息确实丢了 —— 但这是**你已明确接受**的取舍

#### 方案 B：读到 `ClassName` 就按名字反查资产，自动补上

- **优点**：老场景无痛打开
- **缺点**：需要在反序列化时**遍历所有 Script 资产比对类名**（Registry 遍历 + 每个都 `GetAsset`），在"打开场景"这条热路径上加一次全量扫描；而且反查到的资产**不一定是用户当初拖的那个**（两个同名 `.cs`）。**你已明确说不需要，否决。**

#### 方案 C：什么都不做，`AddComponent` 后直接不管

- **优点**：代码最少
- **缺点**：用户打开老场景，脚本静默消失，**没有任何提示** —— 属于最难查的一类问题。**A 只多两行日志，没理由不做。**

**结论：方案 A。**

---

### 4.3 决策点 3：Inspector 里"解析失败"怎么显示

#### 方案 A：直接用 `ImGui::TextColored`（**推荐 ✅**）

```cpp
if (sc.ScriptAsset && !ScriptEngine::ResolveScriptClass(sc.ScriptAsset->GetClassName()))
{
    ImGui::TextColored({0.9f, 0.35f, 0.35f, 1.0f}, "脚本类未找到：请确认该脚本已参与编译");
}
```

- **优点**：
  1. **零新增 API**，改动面最小
  2. 项目有先例（`MaterialEditor.cpp:136` 就是这么写的）
  3. 颜色就地指定，不需要动 `Theme`
- **缺点**：`ComponentInspectors.cpp` 目前在"只用 `UI::` 包装"这件事上是干净的，引入一处裸 `ImGui::` 打破了局部一致性

#### 方案 B：给 `UI` 加一个包装，如 `UI::TextWarning(const char*)`

- **优点**：保持 `ComponentInspectors.cpp` 只依赖 `UI::` 的纯粹性；将来别处要用也能复用
- **缺点**：**新增公共 API**，还要决定颜色是硬编码还是进 `Theme`；而 `UI/` 层目前没有任何文本类控件（`Widgets.h` 里只有 Collapsing / TreeNode / Image / DragDrop / Popup / DropdownList），为一条提示专门开一个类别显得突兀

#### 方案 C：不做错误提示，只打日志

- **优点**：代码最少
- **缺点**：用户拖了个没编译的 `.cs` 进来，Inspector 里显示得好好的，进 Play 什么也不发生，**只能去日志里翻** —— 体验很差。**否决。**

**结论：方案 A。** 若你更看重 `ComponentInspectors.cpp` 的"只用 `UI::`"这条约定，就走方案 B，但要接受"新增一个只服务一处的公共 API"。

---

### 4.4 决策点 4：`EntityScriptClassExists` 还要不要留

改完之后它的**唯一调用点**（`Scene::OnRuntimeStart`）没了，变成无调用点的公开接口。

#### 方案 A：保留（**推荐 ✅**）

- **优点**：
  1. 它是"按全名精确查"的 O(1) 接口，和 `ResolveScriptClass`（按简单名遍历查）职责不同，不是重复
  2. P3 热重载后要做"脚本类还在不在"的校验，这个接口正好用得上
  3. 删了以后再想加回来，又是一轮改动
- **缺点**：当前确实没有调用点（一个 3 行的静态函数，不构成维护负担）

#### 方案 B：删掉，等 P3 需要再加

- **优点**：不留死代码
- **缺点**：将来加回来还要动头文件；且删掉它会让 `ScriptEngine` 少一个语义清晰的查询入口

**结论：方案 A。**（如果你更倾向"不留死代码"，删掉也完全可以，本 Phase 之后没有调用点。）

---

### 4.5 决策点 5：`ScriptComponent` 要不要保留带参构造

当前有 `ScriptComponent(const std::string& className)`。

#### 方案 A：保留一个 `ScriptComponent(const Ref<Script>& scriptAsset)`（**推荐 ✅**）

- **优点**：规范 §13.3 明确要求组件"提供带参数的构造函数"；`AddComponent<ScriptComponent>(scriptAsset)` 这种写法在测试/工具代码里很顺手
- **缺点**：目前没有调用点

#### 方案 B：删掉带参构造，只留 `= default`

- **优点**：无多余接口
- **缺点**：与规范 §13.3 和项目里其他组件（`MeshRendererComponent` 带 `const std::vector<Ref<Material>>&` 构造）的形态不一致

**结论：方案 A。** 保留带参构造。

---

## 5. 实现步骤（按依赖顺序）

每步完成后 `Lucky` 与 `Luck3DApp` 都应能编译通过。

> **⚠️ 步骤顺序的意义**：Step 1～3 改的是"数据与接口"，Step 4 改的是"谁调用" —— 必须先把接口改成新形态，再改调用方，否则中间必然编译不过。若你更希望"每一步都能编译通过"，可以先做 Step 1 + Step 2（组件 + 序列化，此时运行态还没改，会用旧字段编译不过），所以**推荐严格按下面顺序一次做完 1～5**。

### Step 1：改 `ScriptComponent.h`

**文件**：`Lucky/Source/Lucky/Scene/Components/ScriptComponent.h`

```cpp
#pragma once

#include "Lucky/Asset/Script.h"

#include "Lucky/Core/Base.h"

namespace Lucky
{
    /// <summary>
    /// 脚本组件：把用户 C# 脚本挂到实体上
    /// 只保存脚本资产引用，实际的托管对象由 ScriptEngine 统一管理
    /// </summary>
    struct ScriptComponent
    {
        Ref<Script> ScriptAsset;

        ScriptComponent() = default;
        ScriptComponent(const ScriptComponent& other) = default;
        ScriptComponent(const Ref<Script>& scriptAsset)
            : ScriptAsset(scriptAsset) {}
    };
}
```

**要点**：

- **`#include <string>` 可以删掉**：`ClassName` 没了之后本文件不再直接用 `std::string`（`Script.h` 会带上 `<string>`）
- **`#include "Lucky/Core/Base.h"` 必须加**：`Ref` 别名定义在 `Base.h`（`Asset.h` 不含它）
- include 顺序遵循规范 §3.3：工程内头按 `Lucky/...` 全路径写，不用相对路径（本文件原先是相对路径 `Components/...` 风格的是 `Entity.h`，`ScriptComponent.h` 原本只有标准库，**以 `Lucky/` 全路径为准**，与 `Scene.h` 一致）
- 成员名用 `ScriptAsset` 而不是 `Script`：`Script` 是类名，用做成员名会造成 `Script Script;` 这类别扭写法，也容易在 `sc.Script` 处读成类型
- 组件仍是 `struct` + 公开成员（规范 §13.3），不要改成 `class` 加 getter

### Step 2：改序列化

**文件**：`Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp`

把 `Serialize_Script` / `Deserialize_Script` 整体替换为：

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
        }
```

**要点**：

- **完全照抄 3.5 里材质的写法**：`GetHandle()` 直写、`.as<uint64_t>()` + `AssetHandle(value)` 读、`IsValid()` 判有效性。不要自己发明 YAML 转换
- **注意 `handleNode.as<uint64_t>()` 不要写成 `.as<AssetHandle>()`** —— 项目里现有代码统一用 `uint64_t` 中转，保持一致（有没有 `AssetHandle` 的 YAML `convert` 特化不确定，走 `uint64_t` 是已被验证能编译的路径）
- **`LF_CORE_WARN` 而不是 `ERROR`**：读旧场景是用户的正常行为，不是错误。措辞要交代清楚"发生了什么、怎么办"
- **没有 `Renderer3D::GetInternalErrorMaterial()` 那样的兜底**：材质丢了有个"内部错误材质"能顶上，脚本没有等价物 —— 脚本引用为空就是"这个实体没有脚本"，这是**合法状态**（对应 Inspector 里的 `None (Script)`），不该报 ERROR 也不该造一个假的 `Script` 对象
- 第二条 `LF_CORE_ERROR` 的判据是 `!sc.ScriptAsset && handle.IsValid()`：handle 有效但取不到资产，才是真的加载失败（比如 `.cs` 被删了但 Registry 还没同步）

### Step 3：改 `OnCreateEntityScript` 签名与实现

**文件 A**：`Lucky/Source/Lucky/Scripting/ScriptEngine.h`

```cpp
        /// <summary>
        /// 为一个实体实例化托管对象并调用 Awake
        /// 由 Scene::OnRuntimeStart 在遍历 ScriptComponent 时调用；脚本类由调用方解析后传入
        /// </summary>
        /// <param name="entity">目标实体</param>
        /// <param name="scriptClass">已解析出的脚本类（不为空）</param>
        static void OnCreateEntityScript(Entity entity, const Ref<ScriptClass>& scriptClass);
```

**文件 B**：`Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`

```cpp
    void ScriptEngine::OnCreateEntityScript(Entity entity, const Ref<ScriptClass>& scriptClass)
    {
        LF_CORE_ASSERT(scriptClass, "ScriptEngine::OnCreateEntityScript - scriptClass must not be null");

        Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(scriptClass, entity);
        s_Data->EntityInstances[entity.GetUUID()] = instance;

        instance->InvokeAwake();
    }
```

**要点**：

- 用 `LF_CORE_ASSERT` 表达前置条件（规范 §13.7：前置条件用 ASSERT，仅 Debug 生效）。这里**不需要**运行时判空再打 WARN —— 调用方在 `Scene::OnRuntimeStart` 里已经用 `if (!scriptClass) continue;` 拦过了，再判一次是重复
- **删掉**原来的 `EntityClasses.find()` 查询与那句 `LF_CORE_WARN("...not found")`，诊断职责已归 `ResolveScriptClass`
- 参数类型 `const Ref<ScriptClass>&`：`ScriptClass` 在**同一个头文件**里定义，不需要新增 include

### Step 4：改 `Scene::OnRuntimeStart`

**文件**：`Lucky/Source/Lucky/Scene/Scene.cpp`

```cpp
    void Scene::OnRuntimeStart()
    {
        ScriptEngine::OnRuntimeStart(this);

        auto view = m_Registry.view<ScriptComponent>();
        for (entt::entity entityHandle : view)
        {
            Entity entity{ entityHandle, this };
            const ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

            if (!sc.ScriptAsset)
            {
                continue;
            }

            Ref<ScriptClass> scriptClass = ScriptEngine::ResolveScriptClass(sc.ScriptAsset->GetClassName());
            if (!scriptClass)
            {
                LF_CORE_ERROR("Scene::OnRuntimeStart - 实体 '{0}' 的脚本 '{1}' 解析失败，已跳过", entity.GetName(), sc.ScriptAsset->GetClassName());
                continue;
            }

            ScriptEngine::OnCreateEntityScript(entity, scriptClass);
        }

        m_State = SceneState::Play;
    }
```

**要点**：

- **判空从"字符串是否为空"变成"指针是否为空"**：`sc.ScriptAsset` 为空就是没挂脚本，直接跳过 —— 对应原来 `if (sc.ClassName.empty())`
- **删掉 `EntityScriptClassExists` 那个前置检查**：`ResolveScriptClass` 内部已经做了"找不到就返回 nullptr + 打 ERROR"，外面再查一次是重复劳动
- 多出的那句 `LF_CORE_ERROR` 带上**实体名**：这是 `Scene` 层独有的信息，能直接告诉用户"是哪个物体上的脚本坏了"。`ResolveScriptClass` 只能报类名，报不了实体名 —— 这就是决策点 4.1 选方案 A 的价值所在
- `entity.GetName()` 返回 `const std::string&`，直接喂给 spdlog 的 `{0}` 没问题（项目里已有同样用法）
- 日志用 `{0}` / `{1}` 占位（spdlog 支持编号占位），与 `Scene.cpp` 里既有的 `LF_CORE_WARN("Scene::OnRuntimeStart - script class '{0}' not found for entity '{1}'", ...)` 风格一致

### Step 5：改 Inspector

**文件**：`Lucky/Source/Lucky/Editor/ComponentInspectors.cpp`

```cpp
        void Draw_Script(Entity entity)
        {
            ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

            UI::PropertyAsset("Script", sc.ScriptAsset);

            if (sc.ScriptAsset && !ScriptEngine::ResolveScriptClass(sc.ScriptAsset->GetClassName()))
            {
                ImGui::TextColored({0.9f, 0.35f, 0.35f, 1.0f}, "脚本类未找到：请确认该脚本已参与编译，且类名与文件名一致");
            }
        }
```

**要点**：

- **一行就是全部拖拽实现**：`UI::PropertyAsset` 内部已经处理了"拖拽 payload 类型 = `DragDrop::AssetHandle`"、"目标端按 `T::StaticAssetType()` 过滤"、"悬停高亮"、"`IsDelivery()` 才赋值"。这就是 P2.1 决策点 4.1 选"引入 `Script` 资产类"换来的收益
- **模板参数不用显式写**：`sc.ScriptAsset` 是 `Ref<Script>`，`PropertyAsset<T>` 的 `T` 会被推导成 `Script`
- `UI::PropertyAsset` 会返回 `bool`（是否被修改）。当前 `Draw_Script` 返回 `void`，**忽略返回值即可** —— 项目里 `Draw_MeshFilter` / `Draw_Camera` 等也是这么处理的（不像 `Draw_Script` 原来那样需要手动同步字符串缓冲）
- 错误提示放在**字段下方**，不占用标签列，视觉上归属清晰
- `ImGui::TextColored` 需要 `<imgui.h>`。`ComponentInspectors.cpp` 里能用到哪些 ImGui 符号取决于它现有的 include —— **如果编译报"未声明的标识符 ImGui"，在文件顶部补 `#include <imgui.h>`**（放在第三方头分组，规范 §3.3）。若你选择了决策点 4.3 的方案 B（加 `UI::` 包装），则不需要这个 include
- `ResolveScriptClass` 在解析失败时会**打一条 ERROR 日志**。Inspector 每帧都在画，失败时日志会**逐帧刷屏**。这是 P3 异常兜底"不做运行时抑制"同一类问题的另一面：**本 Phase 先接受它**（用户看到一次就该去修了），若实测觉得吵，正确处理是给 `ResolveScriptClass` 加一个"静默查询"重载（如 `TryResolveScriptClass`），**而不是**在 Inspector 里缓存状态 —— 那会引入"脚本重编译后缓存不刷新"的问题。**这一点要记在验收里实测。**

### Step 6：编译验证

- Debug 编译 `Lucky` → 通过
- Debug 编译 `Luck3DApp` → 通过
- 不需要重跑 premake（无新增文件）
- **如果报 `Ref` 未定义**：`ScriptComponent.h` 少了 `#include "Lucky/Core/Base.h"`
- **如果报 `Script` 未定义**：少了 `#include "Lucky/Asset/Script.h"`

---

## 6. 疑点问答

### 6.1 为什么不保留 `ClassName` 做过渡字段？

因为过渡字段会**渗透到每一层**：序列化要写两份、读的时候要判优先级、Inspector 要显示两份、运行态要判断"用哪个"。而过渡期的真实需求是"老场景能打开" —— 你已经明确不需要，所以没有理由引入这套复杂度。

### 6.2 `AddComponent<ScriptComponent>()` 之后立刻读 `AssetManager::GetAsset<Script>`，会不会有加载顺序问题？

不会。`SceneSerializer` 反序列化发生在**打开场景**时，此时 `AssetManager::Init()` 早已完成（`Application::Init` 里的顺序是 `Project::Load` → `AssetManager::Init` → `Renderer::Init` → `ScriptEngine::Init`）。`GetAsset` 会按需从缓存或磁盘加载。

### 6.3 运行态场景（`Scene::Copy` 的副本）能不能正常用 `ScriptAsset`？

**能**，这正是选 `Ref<Script>` 的原因（约束 1.1）。`Ref<Script>` 是 `shared_ptr`，`Scene::Copy` 按值拷贝组件时引用计数 +1、指向同一个 `Script` 对象，副本和源共享。运行态**不需要**回查 `AssetRegistry`。

顺带说明：`Scene.h` 里那句"Asset Handle：副本为无效 Handle"说的是**组件里存裸 `AssetHandle` 字段**的情况（例如 `MeshFilterComponent::MeshAsset`），那种确实会在副本里失效。所以**不要**为了"省一个 shared_ptr"改成裸 handle。

### 6.4 拖材质进脚本字段会怎样？

**会被拒绝，且提示是现成的**。`PropertyAsset<Script>` 里那个 `AssetManager::GetAssetType(handle) == T::StaticAssetType()` 判断会失败，不赋值、不高亮、源端 tooltip 显示"不允许"。你不需要写任何校验代码。

### 6.5 拖一个不是 `Entity` 派生类的 `.cs` 进来会怎样？

分两步看：

1. **拖拽能成功**（`PropertyAsset` 只校验"是 `Script` 类型的资产"，它不知道也不该知道类继承关系）
2. **Inspector 立刻显示红色提示**（`ResolveScriptClass` 找不到 → 因为非派生类不进 `EntityClasses`，见 P2.2 的 6.3）
3. **进 Play 时会打一条带实体名的 ERROR 并跳过**

**这是有意的**：拖拽层只管类型，语义校验交给脚本层。用户看到红字就知道"这个脚本不能用"。

### 6.6 `ScriptComponent` 的 Inspector 注册块要不要改？

**不用**。那块注册的是图标和增删回调：

```cpp
            [](Entity e) { e.AddComponent<ScriptComponent>(); },
            [](Entity e) { e.RemoveComponent<ScriptComponent>(); });
```

两个 lambda 都不碰 `ClassName`，`AddComponent` 默认构造出 `ScriptAsset == nullptr` 的组件，正是我们想要的"已添加但未赋值"状态。**P1 做的 `OnComponentRemoved` 通知链路也不受影响**（它只看 `ComponentType::Script`，不看组件内容）。

### 6.7 老场景打开后 `ScriptComponent` 是空的，会不会影响 P1 的 `OnDestroy`？

不会。`Scene::OnRuntimeStart` 里 `if (!sc.ScriptAsset) continue;` 直接跳过 → 不会创建 `ScriptInstance` → `EntityInstances` 里没有它 → `OnDestroy` 自然不会被调用（没有实例就没有回调，符合语义）。

### 6.8 为什么 `Draw_Script` 里的错误提示用"脚本类未找到"而不是复用 `ResolveScriptClass` 的日志措辞？

因为两者受众不同：`ResolveScriptClass` 的日志是给**看日志的人**（追根因），Inspector 的提示是给**盯着 Inspector 的人**（立刻知道要做什么）。措辞可以各有侧重，但**必须都指向同一个动作**：去检查脚本有没有编译、类名和文件名一致不一致。

---

## 7. 验收标准

1. **编译通过**：`Lucky` 与 `Luck3DApp` 的 Debug / Release / Dist 三个 configuration 全通过
2. **拖拽可用**：Project 面板把 `PlayerController.cs` 拖到 Inspector 的 Script 字段上 → 字段显示 `PlayerController`（不是 `None (Script)`）
3. **类型过滤生效**：拖一个 `.lmat` 到 Script 字段上 → **不赋值**，字段仍是原来的内容
4. **存盘持久化**：保存场景 → 重新打开 → Script 字段仍是那个脚本（验证序列化 handle 的读写）
5. **场景文本格式正确**：`.luck3d` 里 `ScriptComponent` 段是 `AssetHandle: <数字>` 而不是 `ClassName: ...`
6. **运行态可用**：Play → `PlayerController.Awake` 打出 `Hello Luck3D`，Cube 按脚本逻辑移动（验证 `Ref<Script>` 穿过 `Scene::Copy` 仍然有效）
7. **老场景行为符合预期**：打开旧的 `ScriptTest.luck3d` → 有 ScriptComponent、脚本字段为 `None (Script)`、日志有那条 `legacy 'ClassName' format` 的 WARN、**不崩**
8. **错误态可见**：把 `PlayerController.cs` 改名（不重新编译）→ 拖进去 → Inspector **立刻**显示红色提示；日志有 `未找到脚本类`
9. **无多余报错**：正常流程（拖入 + 存盘 + 重启 + Play + Stop）日志里没有 ERROR
10. **`OnDestroy` 链路未退化**：Play 中 Inspector 移除 ScriptComponent → 仍有 `OnDestroy` 打印一次（P1 的成果没被破坏）
11. **代码规范**：通过人工 checklist —— 组件是 `struct` 且带 `= default` 与带参构造（§13.3）；控制语句全带花括号（§5.2）；`UI::PropertyAsset` 这种智能指针引用**没有**用 `auto`（§13.9）；公有接口有 `/// <summary>` 中文注释（§4.1）；无"为对齐而对齐"的空格（§5.4）；无引用外部文档的注释、无"P2.5 会补齐"这类阶段性注释

> ⚠️ **第 8 条要重点实测两件事**：① 提示是否**每帧刷屏**（见 Step 5 要点里的说明）；② 红色文字在**浅色主题**下是否看得清（`{0.9, 0.35, 0.35}` 是深红，浅色背景上应该没问题，但要眼见为实）。

> ⚠️ **第 7 条不要跳过**：它是唯一验证"不做迁移"这个决定没有引发崩溃的用例。

---

## 8. 对下一 Phase 的接线点

| 位置 | 后续 Phase | 打开方式 |
|------|-----------|---------|
| `ScriptComponent::ScriptAsset` | **P2.5** | 在它旁边加 `FieldMap`；字段的"所属脚本"就是 `ScriptAsset` |
| `ScriptEngine::OnCreateEntityScript(entity, scriptClass)` | **P2.6** | 已经有 `scriptClass` 在手，紧接着就能反射字段列表、把 `FieldMap` 的值写进托管对象（**必须在 `InvokeAwake()` 之前**） |
| `Draw_Script` 里的 `ResolveScriptClass` 调用 | **P2.6** | 解析成功后拿到 `ScriptClass`，直接用它反射出字段列表来画控件 —— 同一个调用点，天然复用 |
| `EntityScriptClassExists`（保留但暂无调用点） | **P3 热重载** | 重新加载程序集后校验脚本类是否还在 |
| 那条 `legacy 'ClassName' format` 的 WARN | 未来的格式迁移 | 如果哪天又需要兼容，钩子已经留好了 |

---

## 9. 变更清单速览

- **新增文件**：无
- **修改文件（6 个）**
  - `Lucky/Source/Lucky/Scene/Components/ScriptComponent.h`：`ClassName` → `Ref<Script> ScriptAsset`；include 换成 `Asset/Script.h` + `Core/Base.h`
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.h`：`OnCreateEntityScript` 签名改为 `const Ref<ScriptClass>&`
  - `Lucky/Source/Lucky/Scripting/ScriptEngine.cpp`：实现去掉查表与 not-found 分支，加 `LF_CORE_ASSERT`
  - `Lucky/Source/Lucky/Scene/Scene.cpp`：`OnRuntimeStart` 改为"取 `Ref<Script>` → 解析 → 实例化"，并补一条带实体名的 ERROR
  - `Lucky/Source/Lucky/Serialization/ComponentSerializers.cpp`：`Serialize_Script` / `Deserialize_Script` 改走 `AssetHandle`
  - `Lucky/Source/Lucky/Editor/ComponentInspectors.cpp`：`Draw_Script` 换成 `UI::PropertyAsset<Script>` + 解析失败提示（可能需补 `#include <imgui.h>`）
- **删除**：`ScriptComponent::ClassName` 字段及其序列化键
- **不改动**：`Scene/Scene.h`、`Scene/ComponentCores.cpp`（值拷贝天然共享 `Ref`）、`Editor/ComponentInspectors.cpp` 的注册块、`Asset/*`、托管 C# 代码、premake
- **会失效的既有数据**：旧 `.luck3d` 里的 `ScriptComponent.ClassName`（按决策直接丢弃）

---

## 10. 参考

- 编码规范 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：§3.3 include 顺序、§4.1 XML 注释、§5.2 强制花括号、§5.4 对齐规则、§13.3 ECS 组件、§13.7 错误处理、§13.9 auto 使用规范、§13.10 路径解析
- 前置详设 [Phase2.1_Script_As_Asset.md](Phase2.1_Script_As_Asset.md)（`Script` 资产与 `AssetType::Script`）
- 前置详设 [Phase2.2_ScriptClass_Resolution.md](Phase2.2_ScriptClass_Resolution.md)（`ResolveScriptClass`）
- 前置实现 [Phase1.4_ScriptComponent.md](Phase1.4_ScriptComponent.md)（`ScriptComponent` 的原始形态）
- 资产引用范式 [PhaseB_Independent_Material_File.md](../AssetSystem/PhaseB_Independent_Material_File.md)、[PhaseD_Scene_As_Asset.md](../AssetSystem/PhaseD_Part1_Scene_As_Asset.md)
- 拖拽与 AssetField：[PhaseD_Asset_System_Enhancement.md](../AssetSystem/PhaseD_Asset_System_Enhancement.md)、`UI/PropertyGrid.h` 的 `PropertyAsset<T>`
- 脚本系统路线图 [ScriptSystem_Roadmap.md](ScriptSystem_Roadmap.md) 第 4 节 Phase 2（其中"ClassName 选择从字符串改为下拉列表"一条**已被本 Phase 的资产引用方案取代，作废**）
