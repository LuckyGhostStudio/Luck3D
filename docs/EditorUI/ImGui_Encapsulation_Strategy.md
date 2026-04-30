# ImGui 封装策略设计文档

> **文档类型**：设计规划文档（策略总览）  
> **创建日期**：2025-07  
> **最后更新**：2025-07  
> **状态**：规划中 → 待实施  
> **关联文档**：  
> - [Roadmap_Feature_Development.md](../Roadmap_Feature_Development.md)  
> - [Architecture_Overview.md](../Architecture_Overview.md)  
> - [Coding_Style_Guide.md](../Coding_Style_Guide.md)  
> - **详细实现文档**：  
>   - [ImGui_Layer1_Infrastructure.md](./ImGui_Layer1_Infrastructure.md) — 基础设施层  
>   - [ImGui_Layer2_PropertyControls.md](./ImGui_Layer2_PropertyControls.md) — Property 控件层  
>   - [ImGui_Layer3_CommonWidgets_And_EditorPanel.md](./ImGui_Layer3_CommonWidgets_And_EditorPanel.md) — 通用控件层 + EditorPanel 改进

---

## 一、概述

### 1.1 背景

当前项目的编辑器 UI 层直接使用 ImGui 原生接口（`ImGui::` 命名空间），没有任何中间封装层。随着编辑器功能的增长，这种方式带来了以下问题：

- **重复代码**：Inspector 面板中大量 `Label + 控件` 的布局代码高度重复
- **风格不一致**：各面板的控件样式、间距、颜色缺乏统一管理
- **维护困难**：修改全局风格需要逐个文件修改
- **扩展受限**：未来接入脚本系统时，无法将 UI 接口暴露给脚本层
- **资产系统前置依赖**：即将实现的资产系统需要 AssetField 控件（拖拽选择资产），必须建立在封装层之上

### 1.2 目标

建立一套 **选择性封装** 的 ImGui 中间层（`Lucky::UI` 命名空间），实现：

1. 减少编辑器面板中的重复代码
2. 统一视觉风格，支持主题切换
3. 隔离引擎类型与 ImGui 类型的转换
4. 为未来的脚本绑定和编辑器扩展预留接口
5. 为资产系统提供 AssetField 等基础控件

### 1.3 设计原则

- **选择性封装，而非全量封装**：只封装高频、复合、涉及引擎类型转换的控件
- **不追求消灭 `ImGui::`**：查询类 API、底层布局 API 允许直接使用
- **三层架构**：基础设施 → 增强控件 → 语义化控件
- **渐进式迁移**：新代码使用封装层，旧代码按需迁移

### 1.4 代码规范要求

所有封装层代码必须严格遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)，关键要点：

- 命名空间：`Lucky::UI`（PascalCase）
- 类名：PascalCase（如 `ScopedStyle`、`ScopedColor`）
- 函数名：PascalCase（如 `BeginPropertyGrid()`、`PropertyFloat()`）
- 私有成员：`m_` 前缀（如 `m_Disabled`）
- 静态变量：`s_` 前缀（如 `s_Counter`）
- 花括号：Allman 风格（独占一行）
- 缩进：4 个空格
- 注释：中文，使用 `/// <summary>` XML 文档注释
- 智能指针：使用 `Ref<T>` / `Scope<T>` 别名
- 头文件保护：`#pragma once`

---

## 二、当前项目 ImGui 使用现状（基于实际代码分析）

### 2.1 使用范围

当前项目中 **所有 ImGui 原生调用都集中在编辑器层**，引擎核心层（渲染、ECS、序列化等）没有任何 ImGui 依赖，这是一个非常好的架构基础。

| 文件 | 行数 | ImGui 调用次数 | 使用的控件类型 |
|------|:---:|:---:|------|
| `InspectorPanel.cpp` | 527 | ~60 | `DragFloat/2/3/4`, `DragInt`, `ColorEdit3/4`, `InputText`, `InputInt`, `TreeNodeEx`, `TreePop`, `BeginCombo`, `Selectable`, `ImageButton`, `Text`, `PushID/PopID`, `SameLine`, `SetCursorPos/Y`, `PushStyleVar/PopStyleVar`, `CollapsingHeader`, `Checkbox`, `Button`, `Combo`, `Separator` |
| `SceneHierarchyPanel.cpp` | 341 | ~30 | `TreeNodeEx`, `TreePop`, `IsMouseClicked`, `IsWindowHovered`, `BeginPopupContextWindow`, `BeginPopupContextItem`, `BeginMenu`, `MenuItem`, `Separator`, `IsItemClicked`, `Selectable` |
| `SceneViewportPanel.cpp` | 397 | ~10 | `PushStyleVar/PopStyleVar`, `GetWindowContentRegionMin/Max`, `GetWindowPos`, `GetContentRegionAvail`, `Image` |
| `PreferencesPanel.cpp` | 197 | ~50 | `ColorEdit4`（大量重复）, `TreeNodeEx`, `TreePop`, `Checkbox`, `Selectable`, `BeginChild/EndChild`, `Separator`, `Button` |
| `EditorLayer.cpp` | 约 280 | ~15 | `BeginMainMenuBar`, `BeginMenu`, `MenuItem`, `EndMenu`, `EndMainMenuBar` |
| `EditorDockSpace.cpp` | 66 | ~15 | `GetMainViewport`, `SetNextWindowPos/Size/Viewport`, `PushStyleVar/Color`, `PopStyleVar/Color`, `Begin/End`, `DockSpace`, `GetIO` |
| `EditorPanel.cpp` | 41 | 5 | `Begin/End`, `BeginPopupContextItem`, `MenuItem`, `EndPopup` |

### 2.2 高频使用模式分析

#### 模式 1：Label + 控件（Inspector 面板，最高频）

```cpp
// 当前代码中的重复模式
ImGui::DragFloat("Intensity", &light.Intensity, 0.01f, 0.0f, 100.0f);
ImGui::ColorEdit3("Color", glm::value_ptr(light.Color));
ImGui::DragFloat3("Position", &transform.Translation.x, 0.01f);
ImGui::Combo("Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes));
ImGui::Checkbox("Is Global", &volume.IsGlobal);
```

**问题**：ImGui 原生的 Label 在控件右侧，而 Unity 风格是 Label 在左、控件在右。当前代码没有统一的两列布局。

#### 模式 2：纹理属性（材质编辑器，约 20 行/每个纹理槽）

```cpp
// 当前代码中的纹理属性逻辑（InspectorPanel.cpp 第 440-465 行）
Ref<Texture2D> texture = std::get<Ref<Texture2D>>(prop.Value);
if (!texture)
    texture = Renderer3D::GetDefaultTexture(TextureDefault::Black);
uint32_t textureID = texture->GetRendererID();
std::string strID = std::format("##{0}{1}", prop.Name, textureID);
ImGui::PushID(strID.c_str());
if (ImGui::ImageButton((ImTextureID)textureID, { 64, 64 }, { 0, 1 }, { 1, 0 }))
{
    std::string filepath = FileDialogs::OpenFile("Texture(*.png;*.jpg)\0*.png;*.jpg\0");
    if (!filepath.empty())
        material->SetTexture(prop.Name, Texture2D::Create(filepath));
}
ImGui::SameLine();
ImGui::Text(prop.Name.c_str());
ImGui::PopID();
```

**问题**：手动 ID 拼接、引擎类型强转 `(ImTextureID)textureID`、文件对话框逻辑与 UI 逻辑耦合。

#### 模式 3：组件头部（DrawComponent 模板）

```cpp
// 当前代码中的组件头部逻辑（InspectorPanel.h 第 55-95 行）
const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap 
    | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
bool opened = ImGui::TreeNodeEx((void*)typeid(TComponent).hash_code(), flags, name.c_str());
ImGui::SameLine(contentRegionAvail.x - 18);
if (ImGui::Button("+", { 30, 30 }))
    ImGui::OpenPopup("ComponentSettings");
// ... 弹出菜单逻辑 ...
```

**问题**：每个组件都重复相同的 TreeNode + 设置按钮 + 弹出菜单逻辑。

#### 模式 4：CollapsingHeader 子分组（PostProcessVolume、RenderState）

```cpp
// 当前代码中的子分组模式
if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
{
    ImGui::Checkbox("Bloom Enabled", &volume.BloomEnabled);
    if (volume.BloomEnabled)
    {
        ImGui::DragFloat("Threshold", &volume.BloomThreshold, 0.01f, 0.0f, 10.0f);
        // ...
    }
}
```

#### 模式 5：Enum 下拉框（多处重复的 Combo + static_cast 模式）

```cpp
// 当前代码中重复出现的枚举下拉框模式
const char* lightTypes[] = { "Directional", "Point", "Spot" };
int currentType = static_cast<int>(light.Type);
if (ImGui::Combo("Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
{
    light.Type = static_cast<LightType>(currentType);
}
```

**问题**：每个枚举属性都需要手动定义字符串数组、手动 `static_cast`。

### 2.3 已有封装

| 已有封装 | 位置 | 说明 |
|------|------|------|
| `EditorPanel` 基类 | `Lucky/Source/Lucky/Editor/EditorPanel.h/cpp` | 封装了 `Begin/End`、右键关闭菜单，提供 `OnGUI()` 虚函数 |
| `PanelManager` | `Lucky/Source/Lucky/Editor/PanelManager.h` | 面板注册、查找、生命周期管理 |
| `Lucky::UI::ColorButton` | `Lucky/Source/Lucky/ImGui/ImGui.h` | 仅一个控件：自定义颜色按钮（带调色板弹窗） |

**结论**：当前封装几乎为零，仅有面板级别的 `Begin/End` 封装和一个独立的 `ColorButton` 控件。

---

## 三、参考项目分析

### 3.1 Hazel 项目分析

Hazel 在 `Hazel::UI` 命名空间下建立了完整的封装体系，文件结构如下：

```
Hazel/src/Hazel/ImGui/
├── UICore.h / UICore.cpp           // ID 管理、布局辅助、BeginPropertyGrid
├── ImGuiUtilities.h                // RAII 工具类、颜色工具、绘制工具、增强控件
├── Colors.h                        // 主题颜色常量（constexpr IM_COL32）
├── ImGuiFonts.h                    // 字体管理
├── ImGuiWidgets.h                  // 高级复合控件（Vec3Editor 等）
└── ImGui.h                         // Property 系列语义化控件（71KB 大文件）
```

#### 值得借鉴的设计

| 设计 | 说明 | 本项目是否采用 |
|------|------|:---:|
| RAII Scoped 工具类 | `ScopedStyle`, `ScopedColour`, `ScopedFont`, `ScopedID` | ? 采用 |
| `ScopedColourStack` / `ScopedStyleStack` | 一次 Push 多个颜色/样式，析构时统一 Pop | ? 采用（简化版） |
| 全局 ID 生成器 | `GenerateID()` + `PushID()`/`PopID()` 作用域管理 | ? 采用 |
| Property 系统 | `Property(label, value, ...)` 统一的属性控件接口 | ? 采用（改进命名） |
| 主题颜色常量 | `Colors::Theme::accent`, `background`, `text` 等 | ? 采用（整合到 EditorPreferences） |
| `BeginPropertyGrid` 两列布局 | 使用 `ImGui::Columns(2)` 实现 Label+控件布局 | ?? 改用 `ImGui::Table` |
| 引擎类型 Image 封装 | `UI::Image(Ref<Texture2D>, ...)` | ? 采用 |
| `DrawItemActivityOutline` | 增强控件层的统一视觉效果（焦点/悬停高亮） | ? 采用 |
| `ShiftCursor` 系列 | 光标偏移辅助函数 | ? 采用 |
| `Draw::Underline` | 属性行下划线分隔 | ? 采用 |

#### 需要改进的设计

| Hazel 的做法 | 问题 | 本项目的改进 |
|------|------|------|
| 使用 `ImGui::Columns(2)` | 旧 API，嵌套有 bug，列宽不能自动响应 | 改用 `ImGui::Table` |
| Property 层砍掉 `format` 和 `flags` 参数 | 特殊需求（如 `"%.0f"` 整数格式、`ImGuiSliderFlags_Logarithmic`）无法满足 | 三层架构，Layer 2 保留完整参数 |
| 所有封装放在一个 71KB 的 `ImGui.h` 中 | 文件过大，编译慢，职责不清 | 按职责拆分为多个文件 |
| 偏移量硬编码（`ShiftCursor(10.0f, 9.0f)`） | 字体大小改变时需要手动调整所有偏移值 | 提取为 `Theme::Layout` 常量 |
| 颜色常量使用 `IM_COL32` 格式 | 与 ImGui 的 `ImVec4` 风格不一致 | 统一使用 `ImVec4` 格式 |
| Property 函数全部用 `static` 内联在头文件 | 编译时间长，每个包含该头文件的 .cpp 都会重新编译 | 声明在 .h，实现在 .cpp |
| `ScopedColour` 构造函数接受 `ImColor` 转换 | 隐式转换可能导致精度损失 | 直接接受 `ImVec4` 和 `ImU32` |

### 3.2 Lucky 项目分析

Lucky 项目（另一个参考项目）在 `Lucky::UI` 命名空间下仅有一个文件：

```
Lucky/Source/Lucky/ImGui/ImGui.h    // 仅 91 行，只有一个 ColorButton 控件
```

#### 可取之处

| 设计 | 说明 |
|------|------|
| 命名空间 `Lucky::UI` | 与本项目一致，可直接沿用 |
| 仅使用 ImGui 接口和类型 | 低阶封装不依赖引擎类型，保持了层次清晰 |
| 注释风格 | 使用 `/// <summary>` XML 文档注释，符合项目规范 |

#### 不足之处

| 问题 | 说明 |
|------|------|
| 封装量极少 | 仅一个 `ColorButton`，几乎没有实质性封装 |
| 没有 RAII 工具类 | 缺少 `ScopedStyle` 等基础设施 |
| 没有 Property 系统 | 没有 Label+控件的两列布局封装 |
| 没有 ID 管理 | 没有 `GenerateID()` 等 ID 冲突解决方案 |

### 3.3 Unity 编辑器参考

Unity 的 Inspector 面板 UI 模式是本项目的最终目标参考：

| Unity 特性 | 对应封装 |
|------|------|
| `EditorGUILayout.FloatField("Label", value)` | `UI::PropertyFloat("Label", value)` |
| `EditorGUILayout.ColorField("Label", color)` | `UI::PropertyColor("Label", color)` |
| `EditorGUILayout.ObjectField("Label", obj, typeof(Texture2D))` | `UI::PropertyTexture("Label", texture)` / `UI::PropertyAsset<T>("Label", asset)` |
| `EditorGUILayout.EnumPopup("Label", enumValue)` | `UI::PropertyCombo("Label", selected, options)` |
| `EditorGUILayout.Foldout(foldout, "Section")` | `UI::CollapsingHeader("Section")` |
| `EditorGUILayout.BeginVertical("box")` | `UI::BeginPropertyGrid()` / `UI::EndPropertyGrid()` |

---

## 四、封装架构设计

### 4.1 三层架构

```
┌─────────────────────────────────────────────────────────────────┐
│  Layer 3: 语义化控件（面向编辑器面板）                              │
│  PropertyFloat / PropertyColor / PropertyTexture / PropertyCombo │
│  ComponentHeader / MaterialEditor                                │
│  → 内置 Label+控件布局，参数精简，面板代码直接调用                    │
├─────────────────────────────────────────────────────────────────┤
│  Layer 2: 增强控件（1:1 映射 ImGui，加视觉增强）                    │
│  UI::DragFloat / UI::ColorEdit3 / UI::ImageButton               │
│  → 参数与 ImGui 原生完全一致，内部加 ActivityOutline 等视觉效果      │
│  → 保证任何 ImGui 能做的事这层都能做                                │
├─────────────────────────────────────────────────────────────────┤
│  Layer 1: 基础设施（所有层共用）                                    │
│  ScopedStyle / ScopedColor / ScopedID / ScopedFont              │
│  Theme::Colors / Theme::Styles / Theme::Fonts                   │
│  GenerateID() / PushID() / PopID()                              │
│  ShiftCursor() / Draw::Underline()                              │
│  BeginPropertyGrid() / EndPropertyGrid()                        │
└─────────────────────────────────────────────────────────────────┘
```

**各层职责：**

- **Layer 1（基础设施）**：RAII 工具类 + 主题颜色/样式常量 + ID 管理 + 布局辅助 + 绘制辅助。所有编辑器代码都使用这一层。
- **Layer 2（增强控件）**：和 ImGui 原生 **完全相同的参数签名**，只是在内部加了统一的视觉增强（如焦点高亮、悬停效果）。这层 **不砍参数**，保证任何 ImGui 能做的事这层都能做。当 Layer 3 无法满足特殊需求时，降级到这层。
- **Layer 3（语义化控件）**：面向 Inspector 的高级控件，**大幅精简参数**，内置 Label+控件两列布局。编辑器面板 90% 的代码使用这层。

**降级策略：**

```
编辑器面板代码
    │
    ├── 90% 场景 → Layer 3: UI::PropertyFloat("Intensity", value, 0.01f, 0.0f, 10.0f)
    │
    ├── 9% 场景  → Layer 2: UI::DragFloat(GenerateID(), &value, 0.01f, 0.0f, 0.0f, "%.0f", ImGuiSliderFlags_Logarithmic)
    │
    └── 1% 场景  → ImGui::原生 API（极端定制需求）
```

### 4.2 目录结构规划

```
Lucky/Source/Lucky/UI/
├── UICore.h / UICore.cpp               // Layer 1: ID 管理、布局辅助（ShiftCursor 等）
├── ScopedGuards.h                      // Layer 1: RAII 工具类（ScopedStyle/Color/ID/Font/Disable）
├── Theme.h                             // Layer 1: 布局常量（Theme::Layout）
├── DrawUtils.h / DrawUtils.cpp         // Layer 1: 绘制辅助（Underline、ActivityOutline 等）
├── Controls.h / Controls.cpp           // Layer 2: 增强控件（UI::DragFloat 等，1:1 映射 ImGui）
├── PropertyGrid.h / PropertyGrid.cpp   // Layer 3: Property 系列语义化控件
└── Widgets.h / Widgets.cpp             // Layer 3: 高级复合控件（ComponentHeader、TreeNode 封装等）
```

> **注意**：主题颜色系统已在 `EditorPreferences` 中实现（参见 [EditorPreferences_And_ThemeColor_System.md](./EditorPreferences_And_ThemeColor_System.md)），`Theme.h` 仅存放布局常量，不重复定义颜色。

### 4.3 命名空间

```cpp
namespace Lucky::UI
{
    // ---- Layer 1: 基础设施 ----
    void PushID();
    void PopID();
    const char* GenerateID();
    void ShiftCursorX(float distance);
    void ShiftCursorY(float distance);
    void ShiftCursor(float x, float y);
    void BeginPropertyGrid();
    void EndPropertyGrid();
    
    // ---- Layer 2: 增强控件 ----
    bool DragFloat(const char* label, float* v, float v_speed = 1.0f, ...);
    bool ColorEdit3(const char* label, float col[3], ...);
    bool ImageButton(const Ref<Texture2D>& texture, const ImVec2& size, ...);
    
    // ---- Layer 3: 语义化控件 ----
    bool PropertyFloat(const char* label, float& value, float delta = 0.1f, ...);
    bool PropertyColor(const char* label, glm::vec3& value);
    bool PropertyTexture(const char* label, Ref<Texture2D>& texture);
    bool PropertyCombo(const char* label, int& selected, const char** options, int count);
    bool ComponentHeader(const char* name, bool openByDefault = true);
    
    // ---- RAII 工具类 ----
    class ScopedStyle;
    class ScopedColor;
    class ScopedID;
    class ScopedFont;
    class ScopedDisable;
    
    // ---- 绘制辅助 ----
    namespace Draw
    {
        void Underline(bool fullWidth = false, float offsetX = 0.0f, float offsetY = -1.0f);
    }
    
    // ---- 布局常量 ----
    namespace Theme::Layout { ... }
}
```

---

## 五、封装范围决策

### 5.1 推荐封装的控件（按优先级）

| 优先级 | 控件 | 当前代码中的使用位置 | 理由 |
|:---:|------|------|------|
| **P0** | `ScopedStyle` / `ScopedColor` / `ScopedID` / `ScopedFont` / `ScopedDisable` | 所有面板 | RAII 风格管理，消除 Push/Pop 不匹配风险 |
| **P0** | `GenerateID()` / `PushID()` / `PopID()` | InspectorPanel 材质编辑器 | 解决 ID 冲突问题 |
| **P0** | `BeginPropertyGrid()` / `EndPropertyGrid()` | InspectorPanel | 统一 Label+控件两列布局 |
| **P0** | `PropertyFloat` / `PropertyFloat2` / `PropertyFloat3` / `PropertyFloat4` | InspectorPanel（Transform、Light、PostProcess、Material） | 重复代码最多的控件 |
| **P0** | `PropertyInt` / `PropertyDragInt` | InspectorPanel（RenderQueue、BloomIterations） | 统一风格 |
| **P0** | `PropertyColor` (vec3/vec4) | InspectorPanel（Light.Color、材质颜色属性） | 颜色编辑需要统一风格 |
| **P0** | `PropertyCombo` | InspectorPanel（LightType、ShadowType、TonemapMode、RenderingMode、CullMode、DepthTest、BlendMode） | 枚举下拉框重复模式最多 |
| **P0** | `PropertyCheckbox` | InspectorPanel（IsGlobal、BloomEnabled、FXAAEnabled、VignetteEnabled、DepthWrite） | 统一风格 |
| **P1** | `PropertyTexture` | InspectorPanel 材质编辑器 | 涉及引擎类型 `Ref<Texture2D>` 的转换，为资产系统做准备 |
| **P1** | `PropertyString` / `PropertyReadOnlyString` | InspectorPanel（Name、Mesh） | 统一风格 |
| **P1** | `ComponentHeader` | InspectorPanel `DrawComponent` 模板 | 替代当前的 TreeNode + 设置按钮 + 弹出菜单逻辑 |
| **P1** | `UI::Image` / `UI::ImageButton` | InspectorPanel 材质编辑器、SceneViewportPanel | 接受 `Ref<Texture2D>` 而非 `ImTextureID` |
| **P1** | `Draw::Underline` / `DrawItemActivityOutline` | 所有 Property 控件内部 | 统一视觉效果 |
| **P1** | `ShiftCursor` / `ShiftCursorX` / `ShiftCursorY` | 多处 | 光标偏移辅助 |
| **P2** | `Theme::Layout` 布局常量 | 所有面板 | 消除硬编码魔法数字 |
| **P2** | `CollapsingSection` | InspectorPanel（PostProcess 子分组、RenderState） | 统一子分组样式 |
| **P2** | `PropertyButton` | InspectorPanel 材质编辑器（预设按钮） | 统一按钮样式 |
| **P3** | `PropertyAsset<T>` | 未来资产系统 | 通用资产引用字段，支持拖拽 |
| **P3** | `Vec3Editor`（带 XYZ 彩色按钮） | InspectorPanel Transform | 视觉效果好，但非必需 |

### 5.2 不建议封装的

| 类别 | 示例 | 理由 |
|------|------|------|
| 查询类 API | `IsMouseClicked`, `GetCursorPos`, `IsWindowHovered` | 封装后只是无意义的转发 |
| 底层布局 API | `SameLine`, `SetCursorPos`, `Spacing`, `Indent` | 使用场景差异大，封装收益低 |
| 窗口管理 API | `Begin/End` | 已在 `EditorPanel` 基类中封装 |
| 菜单 API | `BeginMenu`, `MenuItem` | 菜单结构各不相同，封装收益低 |
| ImGui 生命周期 | `CreateContext`, `NewFrame`, `Render` | 仅在 `ImGuiLayer` 中使用一次 |
| DockSpace API | `DockSpace`, `SetNextWindowPos` | 仅在 `EditorDockSpace` 中使用一次 |
| ImGuizmo API | `Manipulate`, `ViewManipulate` | 仅在 `SceneViewportPanel` 中使用 |

---

## 六、核心难点分析与解决方案

### 6.1 封装粒度 — 到底暴露多少参数

**问题**：ImGui 原生 `DragFloat` 有 7 个参数（label, v, speed, min, max, format, flags），封装时应该暴露多少？

**Hazel 的做法**：在 Property 层只暴露常用参数（label, value, delta, min, max, helpText），砍掉了 `format` 和 `flags`。但这导致需要整数格式 `"%.0f"` 或对数拖拽 `ImGuiSliderFlags_Logarithmic` 时无法实现。

**本项目的解决方案 — 三层策略**：

```cpp
// Layer 2: 完整参数，和 ImGui 原生一致（不存在"暴露多少"的纠结）
bool UI::DragFloat(const char* label, float* v, 
                   float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, 
                   const char* format = "%.3f", ImGuiSliderFlags flags = 0);

// Layer 3: 精简参数，面向编辑器（大幅简化）
bool UI::PropertyFloat(const char* label, float& value, 
                       float delta = 0.1f, float min = 0.0f, float max = 0.0f);
```

- 编辑器面板 90% 的场景用 `PropertyFloat`（精简参数）
- 特殊需求降级到 `UI::DragFloat`（完整参数）
- 极端情况直接用 `ImGui::DragFloat`（原生）

### 6.2 Label + 控件的自适应布局

**问题**：ImGui 原生的 Label 在控件右侧，而 Unity 风格是 Label 在左、控件在右，且需要响应面板宽度变化。

**方案对比**：

| 方案 | 优点 | 缺点 | 推荐度 |
|------|------|------|:---:|
| **`ImGui::Table`** | 新 API，支持嵌套，列宽可自动调整，支持 `ImGuiTableFlags_Resizable` | 稍复杂 | ??? **最优** |
| `ImGui::Columns(2)` | 实现简单，Hazel 采用此方案 | 旧 API，嵌套有 bug，列宽不能自动响应 | ?? 其次 |
| 手动 `SameLine` + `SetCursorPosX` | 完全可控 | 代码繁琐，每个控件都要手动计算 | ? 不推荐 |

**本项目推荐方案：`ImGui::Table`**（详见 [ImGui_Layer1_Infrastructure.md](./ImGui_Layer1_Infrastructure.md)）

### 6.3 ID 冲突问题

**问题**：当同一面板中绘制两个材质，每个材质都有 `"Color"` 属性时，如果用 label 作为 ID，两个 `"Color"` 会 ID 冲突。

**当前项目中已有此问题**：InspectorPanel 的材质编辑器中使用 `std::format("##{0}{1}", prop.Name, textureID)` 手动拼接唯一 ID。

**解决方案：全局自增 ID 生成器**（详见 [ImGui_Layer1_Infrastructure.md](./ImGui_Layer1_Infrastructure.md)）

### 6.4 返回值语义统一

**问题**：ImGui 的 `DragFloat` 返回 `true` 表示"值被修改了"，但 `BeginCombo` 返回 `true` 表示"下拉框打开了"。

**解决方案**：所有 Property 系列控件统一返回 `bool modified`，表示"值是否被用户修改"。（详见 [ImGui_Layer2_PropertyControls.md](./ImGui_Layer2_PropertyControls.md)）

### 6.5 引擎类型转换

**问题**：当前 `ImageButton` 需要传 `(ImTextureID)textureID`，这是 OpenGL 的 `GLuint` 强转。

**解决方案**：封装层接受引擎类型 `Ref<Texture2D>`，内部处理转换。（详见 [ImGui_Layer3_CommonWidgets_And_EditorPanel.md](./ImGui_Layer3_CommonWidgets_And_EditorPanel.md)）

### 6.6 Undo/Redo 集成点

**问题**：当前项目还没有 Undo/Redo 系统，但封装层是最佳的集成点。

**预留设计**：在 Property 控件内部预留 hook 点，未来自动获得 Undo/Redo 能力。（详见 [ImGui_Layer2_PropertyControls.md](./ImGui_Layer2_PropertyControls.md)）

### 6.7 拖拽（Drag & Drop）集成

**问题**：ImGui 的拖拽 API 比较底层，当纹理槽需要支持"从资产浏览器拖拽纹理"时，逻辑会和控件封装紧密耦合。

**解决方案**：在 `PropertyTexture` 内部集成拖拽接收。（详见 [ImGui_Layer2_PropertyControls.md](./ImGui_Layer2_PropertyControls.md)）

### 6.8 与 EditorPreferences 颜色系统的关系

**问题**：项目已有 `EditorPreferences` 颜色系统（参见 [EditorPreferences_And_ThemeColor_System.md](./EditorPreferences_And_ThemeColor_System.md)），封装层的 `Theme` 是否会与之冲突？

**解决方案**：
- `EditorPreferences::ColorSettings` 管理 **运行时可修改的颜色**（通过 Preferences 面板调整）
- `UI::Theme::Layout` 仅管理 **布局常量**（间距、偏移、尺寸等），不涉及颜色
- 封装层内部需要颜色时，通过 `EditorPreferences::Get().GetColors()` 获取，不重复定义

---

## 七、封装后的代码对比

### 7.1 InspectorPanel 中的灯光组件

**封装前（当前代码）：**

```cpp
DrawComponent<LightComponent>("Light", entity, [](LightComponent& light)
{
    const char* lightTypes[] = { "Directional", "Point", "Spot" };
    int currentType = static_cast<int>(light.Type);
    if (ImGui::Combo("Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
    {
        LightType newType = static_cast<LightType>(currentType);
        if (newType != light.Type)
        {
            light.Type = newType;
            if (newType == LightType::Directional)
                light.Shadows = ShadowType::Hard;
            else
                light.Shadows = ShadowType::None;
        }
    }
    ImGui::Separator();
    ImGui::ColorEdit3("Color", glm::value_ptr(light.Color));
    ImGui::DragFloat("Intensity", &light.Intensity, 0.01f, 0.0f, 100.0f);
    if (light.Type == LightType::Point || light.Type == LightType::Spot)
        ImGui::DragFloat("Range", &light.Range, 0.1f, 0.1f, 1000.0f);
    // ... 更多属性 ...
});
```

**封装后：**

```cpp
DrawComponent<LightComponent>("Light", entity, [](LightComponent& light)
{
    UI::BeginPropertyGrid();
    
    const char* lightTypes[] = { "Directional", "Point", "Spot" };
    int currentType = static_cast<int>(light.Type);
    if (UI::PropertyCombo("Type", currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
    {
        LightType newType = static_cast<LightType>(currentType);
        if (newType != light.Type)
        {
            light.Type = newType;
            light.Shadows = (newType == LightType::Directional) ? ShadowType::Hard : ShadowType::None;
        }
    }
    
    UI::PropertyColor("Color", light.Color);
    UI::PropertyFloat("Intensity", light.Intensity, 0.01f, 0.0f, 100.0f);
    
    if (light.Type == LightType::Point || light.Type == LightType::Spot)
    {
        UI::PropertyFloat("Range", light.Range, 0.1f, 0.1f, 1000.0f);
    }
    // ... 更多属性 ...
    
    UI::EndPropertyGrid();
});
```

**改进**：自动两列布局、统一行间距和下划线、自动 ID 生成。

### 7.2 材质编辑器中的纹理属性

**封装前（当前代码，约 20 行）：**

```cpp
case ShaderUniformType::Sampler2D:
{
    Ref<Texture2D> texture = std::get<Ref<Texture2D>>(prop.Value);
    if (!texture)
        texture = Renderer3D::GetDefaultTexture(TextureDefault::Black);
    uint32_t textureID = texture->GetRendererID();
    std::string strID = std::format("##{0}{1}", prop.Name, textureID);
    ImGui::PushID(strID.c_str());
    if (ImGui::ImageButton((ImTextureID)textureID, { 64, 64 }, { 0, 1 }, { 1, 0 }))
    {
        std::string filepath = FileDialogs::OpenFile("Texture(*.png;*.jpg)\0*.png;*.jpg\0");
        if (!filepath.empty())
            material->SetTexture(prop.Name, Texture2D::Create(filepath));
    }
    ImGui::SameLine();
    ImGui::Text(prop.Name.c_str());
    ImGui::PopID();
    break;
}
```

**封装后（约 5 行）：**

```cpp
case ShaderUniformType::Sampler2D:
{
    Ref<Texture2D> texture = std::get<Ref<Texture2D>>(prop.Value);
    if (UI::PropertyTexture(prop.Name.c_str(), texture))
    {
        material->SetTexture(prop.Name, texture);
    }
    break;
}
```

代码量减少 75%，且 ID 冲突、类型转换、默认纹理处理等逻辑全部内聚在 `PropertyTexture` 中。

### 7.3 PreferencesPanel 中的颜色编辑

**封装前（当前代码，大量重复）：**

```cpp
changed |= ImGui::ColorEdit4("Grid X Axis", glm::value_ptr(colors.GridAxisXColor));
changed |= ImGui::ColorEdit4("Grid Z Axis", glm::value_ptr(colors.GridAxisZColor));
changed |= ImGui::ColorEdit4("Grid Lines", glm::value_ptr(colors.GridLineColor));
// ... 40+ 行类似代码 ...
```

**封装后：**

```cpp
UI::BeginPropertyGrid();
changed |= UI::PropertyColor4("Grid X Axis", colors.GridAxisXColor);
changed |= UI::PropertyColor4("Grid Z Axis", colors.GridAxisZColor);
changed |= UI::PropertyColor4("Grid Lines", colors.GridLineColor);
// ... 统一风格，自动两列布局 ...
UI::EndPropertyGrid();
```

---

## 八、分阶段实施路线

### Phase 1 — 基础设施（建议立即实施）

**目标**：建立封装层的基础框架，不改变现有面板代码。

**工作内容**：
- 创建 `Lucky/Source/Lucky/UI/` 目录
- 实现 RAII 工具类：`ScopedStyle`、`ScopedColor`、`ScopedID`、`ScopedFont`、`ScopedDisable`
- 实现 `UICore`：`GenerateID()`、`PushID()`/`PopID()`、`ShiftCursor` 系列
- 实现 `Theme::Layout` 布局常量
- 实现 `Draw::Underline`、`DrawItemActivityOutline`

**详细设计**：[ImGui_Layer1_Infrastructure.md](./ImGui_Layer1_Infrastructure.md)  
**预估工作量**：~300-400 行代码

### Phase 2 — 核心控件封装（建议紧接 Phase 1）

**目标**：封装高频控件，迁移 InspectorPanel。

**工作内容**：
- 实现 `BeginPropertyGrid()` / `EndPropertyGrid()`（基于 `ImGui::Table`）
- 封装 Layer 2 增强控件：`UI::DragFloat/2/3/4`、`UI::DragInt`、`UI::ColorEdit3/4`、`UI::InputText`、`UI::Checkbox`、`UI::BeginCombo`/`UI::EndCombo`
- 封装 Layer 3 Property 系列控件：`PropertyFloat/2/3/4`、`PropertyInt`、`PropertyString`、`PropertyColor`/`PropertyColor4`、`PropertyTexture`、`PropertyCombo`、`PropertyCheckbox`
- 封装 `UI::Image` / `UI::ImageButton`（接受 `Ref<Texture2D>`）
- 迁移 InspectorPanel 使用新控件

**详细设计**：[ImGui_Layer2_PropertyControls.md](./ImGui_Layer2_PropertyControls.md)  
**预估工作量**：~600-900 行代码

### Phase 3 — 通用控件与 EditorPanel 改进（建议中期实施）

**目标**：封装非 Property 格式的通用控件，改进 EditorPanel 基类。

**工作内容**：
- 封装 `ComponentHeader`（替代 `DrawComponent` 模板中的 TreeNode + 设置按钮逻辑）
- 封装 `CollapsingSection`（替代 `ImGui::CollapsingHeader`）
- 封装 `BeginTreeNode` / `EndTreeNode`
- 改进 `EditorPanel` 基类（增加 `OnBeginGUI`/`OnEndGUI` 钩子、窗口标志配置等）
- 迁移 SceneHierarchyPanel、PreferencesPanel 使用新控件

**详细设计**：[ImGui_Layer3_CommonWidgets_And_EditorPanel.md](./ImGui_Layer3_CommonWidgets_And_EditorPanel.md)  
**预估工作量**：~400-600 行代码

### Phase 4 — 资产系统 UI（远期目标）

**目标**：为资产系统提供专用控件。

**工作内容**：
- 实现 `PropertyAsset<T>`（通用资产引用字段，支持拖拽）
- 实现 `AssetBrowserItem`（资产浏览器项）
- 实现拖拽源/目标的统一封装
- 实现 `Vec3Editor`（带 XYZ 彩色按钮的向量编辑器）

**预估工作量**：取决于资产系统的实现

---

## 九、总结

| 维度 | 结论 |
|------|------|
| **现在能否开始封装？** | ? 完全可以，条件已经成熟 |
| **是否有必要现在开始？** | ? 非常有必要，再拖下去迁移成本会急剧上升 |
| **会不会影响当前功能？** | ? 不会，渐进式迁移，新旧代码可以共存 |
| **对资产系统的影响？** | 如果不提前封装，资产系统的 AssetField 控件将无法优雅实现 |
| **核心原则** | 封装的目标不是"消灭 `ImGui::`"，而是"减少重复代码、统一视觉风格、为未来扩展预留接口" |
