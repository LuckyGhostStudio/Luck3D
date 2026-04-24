
# Luck3D 编码规范

> 本文档基于 Luck3D 项目现有代码风格整理，适用于 Lucky 引擎（C++）及 Luck3DApp 编辑器的所有源代码。

---

## 目录

1. [项目结构](#1-项目结构)
2. [命名规范](#2-命名规范)
3. [文件组织](#3-文件组织)
4. [注释规范](#4-注释规范)
5. [代码格式](#5-代码格式)
6. [类与结构体](#6-类与结构体)
7. [内存管理](#7-内存管理)
8. [枚举](#8-枚举)
9. [宏定义](#9-宏定义)
10. [模板](#10-模板)
11. [GLSL 着色器](#11-glsl-着色器)
12. [Lua 构建脚本](#12-lua-构建脚本)
13. [其他约定](#13-其他约定)

---

## 1. 项目结构

```
Luck3D/
├── Lucky/                      # 引擎核心库
│   ├── Source/
│   │   ├── Lucky/              # 引擎源码
│   │   │   ├── Asset/          # 资产导入
│   │   │   ├── Core/           # 核心模块（Application、Event、Input、Log 等）
│   │   │   ├── Editor/         # 编辑器框架（EditorPanel、PanelManager 等）
│   │   │   ├── ImGui/          # ImGui 集成层
│   │   │   ├── Math/           # 数学工具
│   │   │   ├── Renderer/       # 渲染系统
│   │   │   │   └── Passes/     # 渲染 Pass
│   │   │   ├── Scene/          # 场景与 ECS
│   │   │   │   └── Components/ # 组件定义
│   │   │   ├── Serialization/  # 序列化
│   │   │   └── Utils/          # 工具类
│   │   ├── lcpch.h             # 预编译头文件
│   │   └── Lucky.h             # 引擎公共头文件
│   └── Vendor/                 # 第三方库
├── Luck3DApp/                  # 编辑器应用
│   ├── Source/
│   │   ├── Panels/             # 编辑器面板
│   │   ├── EditorLayer.h/cpp   # 编辑器主层
│   │   └── ...
│   ├── Assets/                 # 编辑器资产（Shader、纹理、模型、场景）
│   └── Resources/              # 编辑器资源（字体等）
├── docs/                       # 项目文档
├── Scripts/                    # 构建脚本
└── Vendor/                     # 全局第三方工具（Premake）
```

### 模块划分原则

- **Lucky/**：引擎核心，编译为静态库，不依赖编辑器代码
- **Luck3DApp/**：编辑器应用，依赖 Lucky 引擎库
- **Vendor/**：第三方库以 Git Submodule 或预编译二进制方式管理

---

## 2. 命名规范

### 2.1 通用规则

| 类别 | 风格 | 示例 |
|------|------|------|
| **命名空间** | PascalCase | `Lucky` |
| **类名** | PascalCase | `Renderer3D`、`EditorCamera`、`ShaderLibrary` |
| **结构体名** | PascalCase | `DrawCommand`、`BufferElement`、`SceneLightData` |
| **函数/方法** | PascalCase | `BeginScene()`、`GetShaderLibrary()`、`OnImGuiRender()` |
| **公有成员变量** | PascalCase | `Translation`、`Materials`、`Enabled` |
| **私有成员变量** | `m_` + PascalCase | `m_RendererID`、`m_Scene`、`m_Shader` |
| **静态成员变量** | `s_` + PascalCase | `s_Instance`、`s_Data`、`s_CoreLogger` |
| **静态常量** | `s_` + PascalCase | `s_MaxDirectionalLights`、`s_MaxPointLights` |
| **局部变量** | camelCase | `dataSize`、`lastShaderID`、`worldPos` |
| **函数参数** | camelCase | `filepath`、`entityID`、`shader` |
| **枚举类型** | PascalCase | `EventType`、`ShaderDataType`、`TextureDefault` |
| **枚举值** | PascalCase | `WindowClose`、`MouseScrolled`、`Sampler2D` |
| **宏** | `LF_` + UPPER_SNAKE_CASE | `LF_CORE_ASSERT`、`LF_BIND_EVENT_FUNC` |
| **模板参数** | `T` / `T` + PascalCase | `T`、`TPanel`、`TComponent`、`Args` |

### 2.2 前缀约定

- `m_`：非静态成员变量（private/protected）
- `s_`：静态变量（包括文件作用域的 `static` 变量）
- `LF_`：引擎宏前缀（Lucky Framework）
- `u_`：Shader 中的 uniform 变量
- `a_`：Shader 中的顶点属性（attribute）
- `v_`：Shader 中的 varying/输出变量

### 2.3 命名惯例

- **Get/Set 方法**：使用 `GetXxx()` / `SetXxx()` 命名
- **布尔方法**：使用 `Is` / `Has` 前缀，如 `IsRunning()`、`HasComponent<T>()`
- **回调方法**：使用 `On` 前缀，如 `OnUpdate()`、`OnEvent()`、`OnComponentAdded()`
- **UI 方法**：使用 `UI_` 前缀，如 `UI_DrawMenuBar()`
- **工厂方法**：使用 `Create` 前缀，如 `Create()`、`CreateRef()`、`CreateScope()`

---

## 3. 文件组织

### 3.1 文件命名

- 头文件使用 `.h` 后缀，源文件使用 `.cpp` 后缀
- 文件名使用 PascalCase，与主要类名一致：`Renderer3D.h`、`EditorCamera.cpp`
- 组件文件以 `Component` 结尾：`TransformComponent.h`、`MeshRendererComponent.h`
- 渲染 Pass 文件以 `Pass` 结尾：`OpaquePass.h`、`SilhouettePass.cpp`

### 3.2 头文件保护

统一使用 `#pragma once`，不使用传统的 `#ifndef` 头文件保护：

```cpp
#pragma once

// 头文件内容...
```

### 3.3 包含顺序

```cpp
#include "lcpch.h"              // 1. 预编译头文件（仅 .cpp 文件）

#include "CurrentFile.h"        // 2. 对应的头文件

#include "Lucky/Core/Base.h"    // 3. 项目内部头文件
#include "Lucky/Renderer/..."

#include <glm/glm.hpp>          // 4. 第三方库头文件
#include <entt.hpp>

#include <string>               // 5. 标准库头文件（通常已在 PCH 中）
#include <vector>
```

### 3.4 预编译头文件（PCH）

- 预编译头文件为 `lcpch.h`，包含常用标准库头文件和日志模块
- 所有 `.cpp` 文件的第一行必须包含 `#include "lcpch.h"`
- 标准库头文件（`<string>`、`<vector>` 等）通常无需在 `.h` 中重复包含

### 3.5 前向声明

优先使用前向声明减少头文件依赖：

```cpp
class RenderPipeline;   // 前向声明
struct RenderContext;    // 前向声明
```

---

## 4. 注释规范

### 4.1 XML 文档注释

所有公有接口（类、方法、参数）使用 `/// <summary>` 风格的 XML 文档注释：

```cpp
/// <summary>
/// 绘制网格（使用指定材质列表）
/// 材质列表的索引对应 SubMesh 的 MaterialIndex
/// </summary>
/// <param name="transform">模型变换矩阵</param>
/// <param name="mesh">网格</param>
/// <param name="materials">材质列表</param>
/// <param name="entityID">实体 ID（用于鼠标拾取，-1 表示无效）</param>
static void DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh,
                     const std::vector<Ref<Material>>& materials, int entityID = -1);
```

### 4.2 行内注释

- 使用 `//` 行内注释，放在代码行末尾或上方
- 注释使用**中文**，与代码之间保留适当空格
- 成员变量的注释对齐到同一列

```cpp
uint32_t m_RendererID;  // 着色器 ID
std::string m_Name;     // 着色器名字
```

### 4.3 分隔注释

使用 `// ---- 描述 ----` 风格分隔代码逻辑块：

```cpp
// ---- 排序不透明物体 ----
std::sort(...);

// ---- 构建 RenderContext ----
RenderContext context;

// ---- 执行 OpaquePass + PickingPass ----
s_Data.Pipeline.GetPass<OpaquePass>()->Execute(context);
```

使用 `// ======== 描述 ========` 风格分隔更大的逻辑区域：

```cpp
// ======== 渲染管线 ========
RenderPipeline Pipeline;

// ======== Outline 数据 ========
Ref<Framebuffer> TargetFramebuffer;
```

### 4.4 TODO 注释

使用 `TODO` 标记待完成的工作：

```cpp
// TODO 默认材质参数保存到 .mat 中
// TODO 可以直接使用 mesh->GetVertices()
// TODO 优化：相同材质跳过
```

---

## 5. 代码格式

### 5.1 缩进与空格

- 使用 **4 个空格**缩进，不使用 Tab
- 运算符两侧加空格：`a + b`、`x = 0`
- 逗号后加空格：`func(a, b, c)`
- 指针/引用符号靠近类型：`const std::string& name`、`Ref<Shader> shader`

### 5.2 花括号

使用 **Allman 风格**（花括号独占一行）。

**强制花括号**：`if`、`else`、`for`、`while`、`do` 等控制语句的执行体**必须**使用花括号 `{}`，即使只有一行语句也不允许省略：

```cpp
// ? 正确
if (condition)
{
    DoSomething();
}

// ? 错误：禁止省略花括号
if (condition)
    DoSomething();
```

示例：

```cpp
void Renderer3D::Init()
{
    // ...
}

if (condition)
{
    // ...
}
else
{
    // ...
}

for (const auto& item : collection)
{
    // ...
}
```

### 5.3 行宽

- 建议行宽不超过 **120 字符**
- 超长行在逻辑断点处换行，续行缩进一级

### 5.4 空行

- 函数之间保留一个空行
- 逻辑块之间保留一个空行
- 类的 `public`/`private` 区域之间不需要额外空行

---

## 6. 类与结构体

### 6.1 使用原则

- **class**：用于有行为（方法）、有封装（private 成员）的类型
- **struct**：用于纯数据类型（POD）、组件、配置数据

```cpp
// struct：纯数据，成员默认 public
struct DrawCommand
{
    glm::mat4 Transform;
    Ref<Mesh> MeshData;
    // ...
};

// class：有封装和行为
class Material
{
public:
    void SetShader(const Ref<Shader>& shader);
    // ...
private:
    Ref<Shader> m_Shader;
    // ...
};
```

### 6.2 访问修饰符顺序

```cpp
class MyClass
{
public:
    // 构造/析构
    // 公有方法
    // 公有静态方法
public:
    // 公有成员变量（如有）
private:
    // 私有方法
private:
    // 私有成员变量
    // 友元声明
};
```

### 6.3 构造函数

- 使用成员初始化列表，冒号换行缩进：

```cpp
Entity(entt::entity entityID, Scene* scene);

BufferElement(ShaderDataType type, const std::string& name, bool normalized = false)
    : Name(name), Type(type), Size(ShaderDataTypeSize(type)), Offset(0), Normalized(normalized)
{
}
```

- 默认构造/拷贝构造使用 `= default`：

```cpp
TransformComponent() = default;
TransformComponent(const TransformComponent& other) = default;
```

- 禁止拷贝使用 `= delete`：

```cpp
Singleton(const Singleton&) = delete;
Singleton& operator=(const Singleton&) = delete;
```

### 6.4 虚函数

- 基类析构函数声明为 `virtual`
- 派生类重写使用 `override` 关键字
- 默认析构使用 `virtual ~ClassName() = default;` 或 `~ClassName() override = default;`

```cpp
// 基类
class RenderPass
{
public:
    virtual ~RenderPass() = default;
    virtual void Init() = 0;
    virtual void Execute(const RenderContext& context) = 0;
};

// 派生类
class OpaquePass : public RenderPass
{
public:
    void Init() override {}
    void Execute(const RenderContext& context) override;
};
```

### 6.5 友元声明

友元声明放在类的 `private` 区域末尾，附带注释：

```cpp
private:
    friend class Entity;                // 友元类 Entity
    friend class SceneHierarchyPanel;   // 友元类 SceneHierarchyPanel
    friend class SceneSerializer;       // 友元类 SceneSerializer
```

### 6.6 运算符重载

- 类型转换运算符放在 public 区域：

```cpp
operator bool() const { return m_EntityID != entt::null; }
operator entt::entity() const { return m_EntityID; }
operator uint32_t() const { return (uint32_t)m_EntityID; }
```

### 6.7 内联方法

简短的 Getter/Setter 可以在头文件中内联定义：

```cpp
const std::string& GetName() const { return m_Name; }
void SetName(const std::string& name) { m_Name = name; }
bool IsRunning() const { return m_IsRunning; }
```

---

## 7. 内存管理

### 7.1 智能指针别名

项目定义了统一的智能指针别名，**禁止直接使用** `std::shared_ptr` / `std::unique_ptr`：

```cpp
template<typename T>
using Ref = std::shared_ptr<T>;     // 共享所有权

template<typename T>
using Scope = std::unique_ptr<T>;   // 独占所有权
```

### 7.2 创建方式

使用工厂函数创建智能指针：

```cpp
auto shader = CreateRef<Shader>(filepath);      // 等价于 std::make_shared
auto window = CreateScope<Window>(spec);        // 等价于 std::make_unique
```

### 7.3 类型转换

使用 `RefAs` 进行智能指针的向下转型：

```cpp
template<typename T1, typename T2>
Ref<T2> RefAs(const Ref<T1>& ref);

// 使用
auto panel = RefAs<EditorPanel, InspectorPanel>(panelData.Panel);
```

### 7.4 使用原则

- **Ref（shared_ptr）**：用于需要共享所有权的资源（Shader、Material、Texture、Mesh 等）
- **Scope（unique_ptr）**：用于独占所有权的对象（Window、PanelManager 等）
- **裸指针**：仅用于非拥有的引用（如 `Scene*`），不负责生命周期管理

---

## 8. 枚举

### 8.1 枚举类（enum class）

优先使用 `enum class`（强类型枚举）：

```cpp
enum class ShaderDataType
{
    None = 0,
    Float,
    Float2,
    Float3,
    Float4,
    Mat3,
    Mat4,
    Int,
    Int2,
    Int3,
    Int4,
    Bool
};
```

### 8.2 传统枚举（enum）

仅在需要隐式转换为整数（如位标志）时使用传统枚举：

```cpp
enum EventCategory
{
    None = 0,
    EventCategoryApplication    = BIT(0),
    EventCategoryInput          = BIT(1),
    EventCategoryKeyboard       = BIT(2),
    EventCategoryMouse          = BIT(3),
    EventCategoryMouseButton    = BIT(4)
};
```

---

## 9. 宏定义

### 9.1 命名

所有宏使用 `LF_` 前缀 + UPPER_SNAKE_CASE：

```cpp
#define LF_CORE_ASSERT(x, ...)
#define LF_BIND_EVENT_FUNC(func)
#define LF_CORE_TRACE(...)
#define BIT(x) (1 << x)
```

### 9.2 日志宏

项目提供两套日志宏，分别用于引擎内核和客户端：

```cpp
// 内核日志
LF_CORE_TRACE(...)   // 详细调试信息
LF_CORE_INFO(...)    // 程序状态或重要事件
LF_CORE_WARN(...)    // 可能导致问题的情况
LF_CORE_ERROR(...)   // 可恢复错误
LF_CORE_FATAL(...)   // 不可恢复错误

// 客户端日志
LF_TRACE(...)
LF_INFO(...)
LF_WARN(...)
LF_ERROR(...)
LF_FATAL(...)
```

### 9.3 断言宏

```cpp
LF_ASSERT(condition, message)       // 客户端断言
LF_CORE_ASSERT(condition, message)  // 内核断言（仅 Debug 模式生效）
```

---

## 10. 模板

### 10.1 模板参数命名

- 单一类型参数：`T`
- 多类型参数：`T` + 描述性名称，如 `TPanel`、`TComponent`
- 可变参数包：`Args`

```cpp
template<typename T, typename... Args>
T& AddComponent(Args&&... args);

template<typename TPanel, typename... Args>
Ref<TPanel> AddPanel(const char* strID, bool isOpenByDefault, Args&&... args);
```

### 10.2 模板方法定义

模板方法直接在头文件中定义（类内或类外均可）：

```cpp
template<typename T>
T& Entity::GetComponent()
{
    LF_CORE_ASSERT(HasComponent<T>(), "Entity dose not have component!");
    return m_Scene->m_Registry.get<T>(m_EntityID);
}
```

### 10.3 static_assert

在模板中使用 `static_assert` 进行编译期类型检查：

```cpp
template<typename TPanel>
Ref<TPanel> AddPanel(const PanelData& panelData)
{
    static_assert(std::is_base_of<EditorPanel, TPanel>::value,
                  "PanelManager::AddPanel requires TPanel to inherit from EditorPanel");
    // ...
}
```

---

## 11. GLSL 着色器

### 11.1 文件组织

- 顶点着色器：`.vert` 后缀
- 片段着色器：`.frag` 后缀
- 着色器文件放在 `Assets/Shaders/` 目录下
- 子功能着色器放在子目录中（如 `Shaders/Outline/`）

### 11.2 版本声明

```glsl
#version 450 core
```

### 11.3 命名规范

| 类别 | 前缀 | 示例 |
|------|------|------|
| 顶点属性 | `a_` | `a_Position`、`a_Normal`、`a_TexCoord` |
| Uniform 变量 | `u_` | `u_ObjectToWorldMatrix`、`u_Camera`、`u_Albedo` |
| Varying 输出 | `v_` | `v_Output` |
| Uniform Block | `u_` + PascalCase | `u_Camera`（block 实例名） |

### 11.4 布局限定符

显式指定 `location` 和 `binding`：

```glsl
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    vec3 Position;
} u_Camera;
```

### 11.5 注释

着色器中使用 `//` 行内注释，使用中文：

```glsl
layout(location = 0) in vec3 a_Position;    // 位置
layout(location = 1) in vec4 a_Color;       // 颜色
```

---

## 12. Lua 构建脚本

### 12.1 Premake 脚本

- 使用 Premake5 作为构建系统
- 主构建脚本为 `Build.lua`，各项目有独立的 `Build-*.lua`
- 注释使用 `--`，使用中文

```lua
workspace "Luck3D"      -- 解决方案名称
    architecture "x64"  -- 体系结构
    configurations { "Debug", "Release", "Dist" }
    startproject "Luck3DApp"
```

---

## 13. 其他约定

### 13.1 const 正确性

- 不修改对象的方法标记为 `const`
- 参数传递优先使用 `const&`

```cpp
const std::string& GetName() const { return m_Name; }
void SetShader(const Ref<Shader>& shader);
static void BeginScene(const EditorCamera& camera, const SceneLightData& lightData);
```

### 13.2 静态类模式

无需实例化的工具类/管理器使用全静态方法（如 `Renderer3D`、`RenderCommand`、`Input`）：

```cpp
class Renderer3D
{
public:
    static void Init();
    static void Shutdown();
    static void BeginScene(...);
    static void EndScene();
    // ...
};
```

### 13.3 ECS 组件

- 组件使用 `struct`，成员默认 public
- 组件名以 `Component` 结尾
- 提供默认构造和拷贝构造（`= default`）
- 提供带参数的构造函数

```cpp
struct MeshRendererComponent
{
    std::vector<Ref<Material>> Materials;

    MeshRendererComponent() = default;
    MeshRendererComponent(const MeshRendererComponent& other) = default;
    MeshRendererComponent(const std::vector<Ref<Material>>& materials)
        : Materials(materials) {}

    // Getter/Setter 方法...
};
```

### 13.4 渲染 Pass 模式

- 继承 `RenderPass` 基类
- 实现 `Init()`、`Execute()`、`GetName()` 纯虚函数
- 通过 `RenderContext` 获取渲染数据，避免直接依赖全局状态

```cpp
class OpaquePass : public RenderPass
{
public:
    void Init() override {}
    void Execute(const RenderContext& context) override;
    const std::string& GetName() const override
    {
        static std::string name = "OpaquePass";
        return name;
    }
};
```

### 13.5 数据对齐

GPU 数据结构需要手动填充以满足 `std140` 布局的 16 字节对齐：

```cpp
struct CameraUBOData
{
    glm::mat4 ViewProjectionMatrix; // VP 矩阵
    glm::vec3 Position;             // 相机位置
    char padding[4];                // 填充到 16 字节对齐
};
```

### 13.6 默认参数值

- 结构体成员提供默认初始值
- 函数参数的默认值在声明处指定

```cpp
struct DrawCommand
{
    uint64_t SortKey = 0;
    float DistanceToCamera = 0.0f;
    int EntityID = -1;
};

static void DrawMesh(..., int entityID = -1);
```

### 13.7 错误处理

- 使用 `LF_CORE_ASSERT` 进行前置条件检查（仅 Debug 模式）
- 使用 `LF_CORE_ERROR` 记录运行时错误
- 对于可恢复的错误，提供 fallback 行为（如使用内部错误材质替代丢失材质）

```cpp
// 当前 SubMesh 材质不存在 使用内部错误材质（表示材质丢失）
if (!material || !material->GetShader())
{
    material = s_Data.InternalErrorMaterial;
}
```

### 13.8 类型转换

- 优先使用 C++ 风格转换：`static_cast<uint32_t>(value)`
- 简单的整数转换可使用 C 风格：`(uint32_t)m_EntityID`

---

## 附录：常用类型速查

| 别名 | 实际类型 | 用途 |
|------|---------|------|
| `Ref<T>` | `std::shared_ptr<T>` | 共享所有权的资源 |
| `Scope<T>` | `std::unique_ptr<T>` | 独占所有权的对象 |
| `DeltaTime` | 自定义类型 | 帧间隔时间 |
| `UUID` | 自定义类型 | 全局唯一标识符 |
| `Entity` | 自定义类型 | ECS 实体句柄 |
