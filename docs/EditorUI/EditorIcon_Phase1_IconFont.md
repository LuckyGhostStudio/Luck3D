# 编辑器图标系统 Phase 1：图标字体（Icon Font）

> **文档版本**：v1.0  
> **创建日期**：2026-05-24  
> **状态**：待实施  
> **优先级**：? 第二优先级  
> **所属模块**：EditorUI / ImGui 集成层  
> **关联文档**：  
> - [ImGui_Layer1_Infrastructure.md](./ImGui_Layer1_Infrastructure.md)（ImGui 基础设施层）  
> - [EditorIcon_Phase2_EditorIconManager.md](./EditorIcon_Phase2_EditorIconManager.md)（固定图标管理器）  
> - [EditorIcon_Phase3_ThumbnailRenderer.md](./EditorIcon_Phase3_ThumbnailRenderer.md)（渲染缩略图）  
> **代码规范**：[Coding_Style_Guide.md](../Coding_Style_Guide.md)

---

## 一、概述

### 1.1 目标

为编辑器的菜单项（MenuItem）、面板标签页（Tab）等 UI 元素提供轻量级的单色矢量图标支持。图标字体与文本字体合并渲染，零额外 Draw Call，与文本天然对齐。

### 1.2 适用场景

| 场景 | 示例 |
|------|------|
| 主菜单项 | File 菜单前的文件图标、Edit 菜单前的编辑图标 |
| 面板 Tab 标签 | Hierarchy 面板 Tab 前的层级图标、Inspector 面板 Tab 前的检查图标 |
| 右键菜单项 | Create Empty 前的加号图标、Delete 前的删除图标 |
| 工具栏按钮 | 播放/暂停/停止按钮（如果未来有工具栏） |

### 1.3 不适用场景

- 资产类型图标（Material 材质球、Mesh 网格等）→ 使用 Phase 2 纹理图标
- 资产缩略图预览 → 使用 Phase 3 渲染缩略图
- 需要彩色/多色的图标 → 使用纹理图标

---

## 二、图标字体选型

### 2.1 方案对比

| 方案 | 图标数量 | 许可证 | 文件大小 | 风格 | 推荐度 |
|------|---------|--------|---------|------|--------|
| **方案 A：Lucide Icons** | 1500+ | ISC（MIT 兼容） | ~200KB TTF | 线条风格，现代简洁 | ??? 推荐 |
| 方案 B：FontAwesome 6 Free | 2000+ | SIL OFL / CC BY 4.0 | ~400KB TTF | 实心+线条，经典 | ?? |
| 方案 C：Material Design Icons | 2100+ | Apache 2.0 | ~600KB TTF | Google 风格 | ? |
| 方案 D：Codicons (VS Code) | 400+ | CC BY 4.0 | ~50KB TTF | 代码编辑器风格 | ?? |

### 2.2 推荐方案：方案 A ? Lucide Icons

**推荐原因**：
1. **许可证友好**：ISC 许可证，无任何使用限制，可商用
2. **风格统一**：纯线条风格，与深色编辑器主题搭配极佳
3. **图标覆盖全面**：文件、文件夹、设置、搜索、播放、暂停、灯泡、盒子、球体等编辑器常用图标齐全
4. **社区活跃**：持续更新，有 C/C++ 头文件生成工具
5. **文件体积适中**：~200KB，不会显著增加应用体积

**备选方案 B ? FontAwesome 6 Free**：
- 优点：图标数量最多，社区最大，ImGui 生态中使用最广泛（有现成的 `IconsFontAwesome6.h`）
- 缺点：Free 版本图标风格混合（Solid + Regular），部分好看的图标在 Pro 版本中

### 2.3 最终决策

优先使用 **Lucide Icons**。如果在实施过程中发现 Lucide 缺少某些关键图标，可以考虑同时加载 FontAwesome 作为补充（ImGui 支持合并多个图标字体）。

---

## 三、文件结构

### 3.1 资源文件

```
Luck3DApp/
└── Resources/
    └── Fonts/
        ├── Opensans/           # 已有：文本字体
        │   ├── OpenSans-Bold.ttf
        │   └── OpenSans-Regular.ttf
        └── Icons/              # 新增：图标字体
            └── lucide.ttf      # Lucide 图标字体文件
```

### 3.2 源代码文件

```
Lucky/Source/Lucky/ImGui/
├── ImGuiLayer.h                # 已有
├── ImGuiLayer.cpp              # 修改：加载图标字体
└── IconsFontLucide.h           # 新增：图标码点常量定义
```

---

## 四、详细实现

### 4.1 IconsFontLucide.h ? 图标码点常量定义

此文件定义所有需要使用的 Lucide 图标的 Unicode 码点常量。Lucide 图标使用 Private Use Area（PUA）区域的码点。

```cpp
// Lucky/Source/Lucky/ImGui/IconsFontLucide.h
#pragma once

// Lucide Icons - https://lucide.dev/
// 图标字体码点范围
#define ICON_MIN_LC 0xE000
#define ICON_MAX_LC 0xF000

// ======== 文件与文件夹 ========
#define ICON_LC_FILE                "\xee\x80\x80"  // 文件
#define ICON_LC_FILE_PLUS           "\xee\x80\x81"  // 新建文件
#define ICON_LC_FILE_TEXT           "\xee\x80\x82"  // 文本文件
#define ICON_LC_FOLDER              "\xee\x80\x83"  // 文件夹
#define ICON_LC_FOLDER_OPEN         "\xee\x80\x84"  // 打开的文件夹
#define ICON_LC_FOLDER_PLUS         "\xee\x80\x85"  // 新建文件夹

// ======== 编辑操作 ========
#define ICON_LC_PLUS                "\xee\x80\x90"  // 加号（创建）
#define ICON_LC_TRASH               "\xee\x80\x91"  // 删除
#define ICON_LC_PENCIL              "\xee\x80\x92"  // 编辑/重命名
#define ICON_LC_COPY                "\xee\x80\x93"  // 复制
#define ICON_LC_SCISSORS            "\xee\x80\x94"  // 剪切
#define ICON_LC_CLIPBOARD           "\xee\x80\x95"  // 粘贴
#define ICON_LC_UNDO                "\xee\x80\x96"  // 撤销
#define ICON_LC_REDO                "\xee\x80\x97"  // 重做

// ======== 面板标签 ========
#define ICON_LC_LIST_TREE           "\xee\x80\xa0"  // Hierarchy 面板
#define ICON_LC_SLIDERS             "\xee\x80\xa1"  // Inspector 面板
#define ICON_LC_FOLDER_TREE         "\xee\x80\xa2"  // Project 面板
#define ICON_LC_MONITOR             "\xee\x80\xa3"  // Scene 面板
#define ICON_LC_LAYERS              "\xee\x80\xa4"  // Render Pipeline 面板
#define ICON_LC_SETTINGS            "\xee\x80\xa5"  // Preferences 面板
#define ICON_LC_SUN                 "\xee\x80\xa6"  // Lighting 面板

// ======== 播放控制 ========
#define ICON_LC_PLAY                "\xee\x80\xb0"  // 播放
#define ICON_LC_PAUSE               "\xee\x80\xb1"  // 暂停
#define ICON_LC_SQUARE              "\xee\x80\xb2"  // 停止
#define ICON_LC_SKIP_FORWARD        "\xee\x80\xb3"  // 下一帧

// ======== 变换工具 ========
#define ICON_LC_MOVE                "\xee\x80\xc0"  // 移动工具
#define ICON_LC_ROTATE_3D           "\xee\x80\xc1"  // 旋转工具
#define ICON_LC_MAXIMIZE            "\xee\x80\xc2"  // 缩放工具

// ======== 窗口/视图 ========
#define ICON_LC_MAXIMIZE_2          "\xee\x80\xd0"  // 最大化
#define ICON_LC_MINIMIZE_2          "\xee\x80\xd1"  // 最小化
#define ICON_LC_X                   "\xee\x80\xd2"  // 关闭
#define ICON_LC_SEARCH              "\xee\x80\xd3"  // 搜索
#define ICON_LC_EYE                 "\xee\x80\xd4"  // 可见
#define ICON_LC_EYE_OFF             "\xee\x80\xd5"  // 隐藏

// ======== 其他 ========
#define ICON_LC_SAVE                "\xee\x80\xe0"  // 保存
#define ICON_LC_DOWNLOAD            "\xee\x80\xe1"  // 导入
#define ICON_LC_UPLOAD              "\xee\x80\xe2"  // 导出
#define ICON_LC_REFRESH             "\xee\x80\xe3"  // 刷新
#define ICON_LC_INFO                "\xee\x80\xe4"  // 信息
#define ICON_LC_ALERT_TRIANGLE      "\xee\x80\xe5"  // 警告
#define ICON_LC_CIRCLE_X            "\xee\x80\xe6"  // 错误
```

> **注意**：上述码点为示例值。实际码点需要根据下载的 Lucide TTF 文件中的 cmap 表确定。可以使用 Lucide 官方提供的工具或 [IconFontCppHeaders](https://github.com/juliettef/IconFontCppHeaders) 项目自动生成。

### 4.2 获取正确码点的方法

**方案 A（推荐）：使用 IconFontCppHeaders 项目**

[IconFontCppHeaders](https://github.com/juliettef/IconFontCppHeaders) 项目为多种图标字体（包括 Lucide）自动生成 C/C++ 头文件，包含正确的码点定义。

```bash
# 克隆项目
git clone https://github.com/juliettef/IconFontCppHeaders.git

# 直接使用生成好的头文件
# IconFontCppHeaders/IconsLucide.h
```

**方案 B：手动查询**

1. 访问 [Lucide Icons](https://lucide.dev/icons/) 网站
2. 搜索需要的图标
3. 查看图标的 Unicode 码点
4. 手动编写 UTF-8 编码的宏定义

**推荐方案 A**，直接使用社区维护的头文件，避免手动维护码点。

### 4.3 ImGuiLayer.cpp ? 加载图标字体

修改 `ImGuiLayer::OnAttach()` 方法，在加载文本字体后合并加载图标字体：

```cpp
// Lucky/Source/Lucky/ImGui/ImGuiLayer.cpp

#include "IconsFontLucide.h"    // 新增：图标码点定义

void ImGuiLayer::OnAttach()
{
    // ... 现有代码：创建 ImGui 上下文、设置 ConfigFlags ...

    Application& app = Application::GetInstance();
    float fontSize = 20.0f * app.GetWindow().GetDPI() / s_StandardDPI;

    // ---- 加载文本字体 ----
    io.Fonts->AddFontFromFileTTF("Resources/Fonts/Opensans/OpenSans-Bold.ttf", fontSize);       // 0号：粗体
    
    // 默认字体（1号：Regular）+ 合并图标字体
    io.FontDefault = io.Fonts->AddFontFromFileTTF("Resources/Fonts/Opensans/OpenSans-Regular.ttf", fontSize);   // 1号

    // ---- 合并图标字体到默认字体 ----
    ImFontConfig iconConfig;
    iconConfig.MergeMode = true;                        // 合并模式：将图标字形合并到上一个加载的字体中
    iconConfig.PixelSnapH = true;                       // 像素对齐
    iconConfig.GlyphMinAdvanceX = fontSize;             // 图标最小宽度（等宽）
    iconConfig.GlyphOffset.y = 2.0f;                    // 垂直偏移微调（使图标与文本基线对齐）

    static const ImWchar iconRanges[] = { ICON_MIN_LC, ICON_MAX_LC, 0 };
    io.Fonts->AddFontFromFileTTF("Resources/Fonts/Icons/lucide.ttf", fontSize, &iconConfig, iconRanges);

    // ... 现有代码：设置样式、加载偏好设置、初始化 GLFW/OpenGL 后端 ...
}
```

### 4.4 关键参数说明

| 参数 | 值 | 说明 |
|------|-----|------|
| `MergeMode` | `true` | 将图标字形合并到前一个字体（Regular）中，使用同一字体即可同时渲染文本和图标 |
| `PixelSnapH` | `true` | 水平像素对齐，避免图标模糊 |
| `GlyphMinAdvanceX` | `fontSize` | 确保所有图标等宽，避免不同图标宽度不一致导致布局跳动 |
| `GlyphOffset.y` | `2.0f` | 垂直偏移微调值，需要根据实际效果调整（正值向下偏移） |
| `iconRanges` | `{ICON_MIN_LC, ICON_MAX_LC, 0}` | 只加载图标码点范围内的字形，减少内存占用 |

---

## 五、使用方式

### 5.1 在菜单项中使用

```cpp
// EditorLayer.cpp - UI_DrawMenuBar()

if (ImGui::BeginMenu(ICON_LC_FILE " File"))     // 菜单标题带图标
{
    if (ImGui::MenuItem(ICON_LC_FILE_PLUS " New"))          // 新建
    { /* ... */ }
    
    if (ImGui::MenuItem(ICON_LC_FOLDER_OPEN " Open..."))    // 打开
    { /* ... */ }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem(ICON_LC_SAVE " Save"))              // 保存
    { /* ... */ }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem(ICON_LC_DOWNLOAD " Import Model..."))   // 导入模型
    { /* ... */ }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem(ICON_LC_X " Quit", "Ctrl Q"))       // 退出
    { /* ... */ }
    
    ImGui::EndMenu();
}
```

### 5.2 在面板 Tab 中使用

面板名称在 `EditorLayer::OnAttach()` 中注册时指定，Tab 标题直接使用带图标的字符串：

```cpp
// EditorLayer.cpp - OnAttach()

m_PanelManager->AddPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL_ID, ICON_LC_LIST_TREE " Hierarchy", true, m_Scene);
m_PanelManager->AddPanel<SceneViewportPanel>(SCENE_VIEWPORT_PANEL_ID, ICON_LC_MONITOR " Scene", true, m_Scene);
m_PanelManager->AddPanel<InspectorPanel>(INSPECTOR_PANEL_ID, ICON_LC_SLIDERS " Inspector", true, m_Scene);
m_PanelManager->AddPanel<ProjectAssetsPanel>(PROJECT_ASSETS_PANEL_ID, ICON_LC_FOLDER_TREE " Project", true);
m_PanelManager->AddPanel<RenderPipelinePanel>(RENDER_PIPELINE_PANEL_ID, ICON_LC_LAYERS " Render Pipeline", true);
m_PanelManager->AddPanel<PreferencesPanel>(PREFERENCES_PANEL_ID, ICON_LC_SETTINGS " Preferences", false);
m_PanelManager->AddPanel<LightingPanel>(LIGHTING_PANEL_ID, ICON_LC_SUN " Lighting", false, m_Scene);
```

### 5.3 在右键菜单中使用

```cpp
// SceneHierarchyPanel.cpp - DrawEntityCreateMenu()

if (ImGui::BeginMenu(ICON_LC_PLUS " Create"))
{
    if (ImGui::MenuItem(ICON_LC_BOX " Create Empty"))
    { /* ... */ }
    
    if (ImGui::BeginMenu(ICON_LC_BOX " 3D Object"))
    {
        if (ImGui::MenuItem("Cube"))    { /* ... */ }
        if (ImGui::MenuItem("Sphere"))  { /* ... */ }
        // ...
        ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu(ICON_LC_SUN " Light"))
    {
        if (ImGui::MenuItem("Directional Light"))   { /* ... */ }
        if (ImGui::MenuItem("Point Light"))         { /* ... */ }
        if (ImGui::MenuItem("Spot Light"))          { /* ... */ }
        ImGui::EndMenu();
    }
    
    ImGui::EndMenu();
}
```

---

## 六、实施步骤

### 6.1 步骤清单

| 步骤 | 内容 | 涉及文件 | 预计工作量 |
|------|------|---------|-----------|
| 1 | 下载 Lucide TTF 字体文件 | `Resources/Fonts/Icons/lucide.ttf` | 5 分钟 |
| 2 | 获取/生成图标码点头文件 | `Lucky/Source/Lucky/ImGui/IconsFontLucide.h` | 10 分钟 |
| 3 | 修改 ImGuiLayer 加载图标字体 | `Lucky/Source/Lucky/ImGui/ImGuiLayer.cpp` | 15 分钟 |
| 4 | 修改菜单项添加图标 | `Luck3DApp/Source/EditorLayer.cpp` | 20 分钟 |
| 5 | 修改面板注册名称添加图标 | `Luck3DApp/Source/EditorLayer.cpp` | 5 分钟 |
| 6 | 修改右键菜单添加图标 | `Luck3DApp/Source/Panels/SceneHierarchyPanel.cpp` | 15 分钟 |
| 7 | 调整 GlyphOffset.y 对齐参数 | `Lucky/Source/Lucky/ImGui/ImGuiLayer.cpp` | 10 分钟 |

### 6.2 下载 Lucide 字体

```bash
# 方法 1：从 npm 包获取
npm install lucide-static
# TTF 文件位于 node_modules/lucide-static/font/lucide.ttf

# 方法 2：从 GitHub Release 下载
# https://github.com/lucide-icons/lucide/releases
# 下载 lucide-font-x.x.x.zip，解压获取 lucide.ttf

# 方法 3：使用 IconFontCppHeaders 项目（推荐，同时获取 TTF + 头文件）
git clone https://github.com/juliettef/IconFontCppHeaders.git
# 使用 IconsLucide.h 头文件
```

### 6.3 Premake 构建配置

无需修改构建配置。TTF 文件作为运行时资源放在 `Resources/` 目录下，不参与编译。头文件放在 Lucky 引擎源码目录中，会被自动包含。

---

## 七、注意事项与常见问题

### 7.1 字体图集大小

当合并图标字体后，ImGui 的字体图集（Font Atlas）纹理会变大。如果图标范围过大，可能导致：
- 字体图集纹理超过 GPU 最大纹理尺寸
- 内存占用增加

**解决方案**：只加载需要的码点范围（通过 `iconRanges` 限制），不要加载整个 PUA 区域。

### 7.2 DPI 缩放

图标字体的大小应该跟随文本字体的 DPI 缩放。当前代码已经通过 `fontSize` 统一计算，图标字体使用相同的 `fontSize` 即可自动适配。

### 7.3 粗体字体中的图标

当前代码中 0 号字体是粗体（Bold），1 号字体是常规（Regular）。图标字体只合并到了 Regular 字体中。如果需要在粗体文本中也显示图标，需要额外合并一次：

```cpp
// 加载粗体
io.Fonts->AddFontFromFileTTF("Resources/Fonts/Opensans/OpenSans-Bold.ttf", fontSize);

// 合并图标到粗体
ImFontConfig iconConfigBold;
iconConfigBold.MergeMode = true;
iconConfigBold.PixelSnapH = true;
iconConfigBold.GlyphMinAdvanceX = fontSize;
iconConfigBold.GlyphOffset.y = 2.0f;
io.Fonts->AddFontFromFileTTF("Resources/Fonts/Icons/lucide.ttf", fontSize, &iconConfigBold, iconRanges);

// 加载常规字体
io.FontDefault = io.Fonts->AddFontFromFileTTF("Resources/Fonts/Opensans/OpenSans-Regular.ttf", fontSize);

// 合并图标到常规字体
ImFontConfig iconConfigRegular;
iconConfigRegular.MergeMode = true;
iconConfigRegular.PixelSnapH = true;
iconConfigRegular.GlyphMinAdvanceX = fontSize;
iconConfigRegular.GlyphOffset.y = 2.0f;
io.Fonts->AddFontFromFileTTF("Resources/Fonts/Icons/lucide.ttf", fontSize, &iconConfigRegular, iconRanges);
```

### 7.4 图标颜色

图标字体的颜色由 ImGui 的当前文本颜色决定。如果需要特定颜色的图标，使用 `ImGui::PushStyleColor` / `ImGui::PopStyleColor`：

```cpp
ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));  // 黄色
ImGui::Text(ICON_LC_ALERT_TRIANGLE);    // 黄色警告图标
ImGui::PopStyleColor();
```

---

## 八、验收标准

1. ? 编辑器启动无报错，字体加载成功
2. ? 主菜单项前显示对应图标
3. ? 面板 Tab 标签前显示对应图标
4. ? 右键菜单项前显示对应图标
5. ? 图标与文本垂直对齐，无明显偏移
6. ? 图标在不同 DPI 下正确缩放
7. ? 图标在粗体和常规字体中均可正常显示
