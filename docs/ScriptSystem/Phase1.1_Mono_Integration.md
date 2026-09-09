# Phase 1.1：Mono 运行时依赖引入

## 1. 概述

Phase 1 的第一步：把 Mono Embedding 运行时作为第三方依赖引入项目，让 `Lucky` 静态库能编译出对 `mono-2.0-sgen` 的引用，`Luck3DApp` 运行时能加载到 mono 的 DLL 与 BCL（Base Class Library）。

**本 Phase 只做"把依赖放进来 + 让工程能链上"这一件事**，不写任何调用 mono API 的代码，不引入 `ScriptEngine`。落地完成后的**唯一验证方式**是：
- `premake5 vs2022` 重新生成工程无报错
- `Lucky` / `Luck3DApp` 在 Debug / Release 三个配置下都能编译通过
- 输出目录下能看到 `mono-2.0-sgen.dll` + `mono/` 目录完整拷贝

### 1.1 关键决策

- **Mono 发行版本**：使用 Hazel Engine 同款的 mono 6.12 Windows MSVC 预编译版（版本号：`mono-2.0-sgen`）
- **Vendor 布局**：与 `assimp` 保持一致，`include / bin / lib` 三段式，分 Debug / Release 目录
- **Debug/Release 配置**：mono 官方 Windows 版本仅提供一个 Release 构建；`Debug` 与 `Release/Dist` 三个 configuration 全部链接同一份 `mono-2.0-sgen.lib`（Hazel 的做法一致，Debug 端不使用 mono-debug 版本，除非未来接托管调试器时再单独引入）
- **CRT 兼容**：mono 官方 lib 用 `/MT`（static runtime），项目当前 `staticruntime "off"` 走 `/MD`。需要验证是否链得上；如果失败，改用 Hazel 版预编译（`/MD`）
- **部署方式**：`Luck3DApp` 的 `postbuildcommands` 追加拷贝 `mono-2.0-sgen.dll` 和整个 `mono/` 子目录到 `targetdir`

### 1.2 前置依赖

- Phase 0 全部完成（Runtime 钩子链路已就绪）

### 1.3 本 Phase **不做**的事

- 不新增任何 C++ / C# 源码文件
- 不写 `ScriptEngine::Init`（那是 P1.3）
- 不写 `Lucky-ScriptCore` 托管工程（那是 P1.2）
- 不注册任何 Internal Call（那是 P1.5）
- 不改 `Scene::OnRuntimeStart / OnRuntimeStop` 的实现（那是 P1.6）
- 不做 mono 调试器接入（Phase 4 议题）

---

## 2. 涉及的文件

### 需要新建

| 路径 | 说明 |
|------|------|
| `Lucky/Vendor/mono/include/**` | mono 公共头文件（`mono/jit/jit.h`、`mono/metadata/*.h` 等） |
| `Lucky/Vendor/mono/lib/Debug/mono-2.0-sgen.lib` | 链接库（Debug 配置用） |
| `Lucky/Vendor/mono/lib/Release/mono-2.0-sgen.lib` | 链接库（Release / Dist 配置用） |
| `Lucky/Vendor/mono/bin/Debug/mono-2.0-sgen.dll` | 运行时 DLL（Debug 配置用） |
| `Lucky/Vendor/mono/bin/Release/mono-2.0-sgen.dll` | 运行时 DLL（Release / Dist 配置用） |
| `Lucky/Vendor/mono/etc/mono/config` | mono 启动时读取的 assembly binding 配置 |
| `Lucky/Vendor/mono/etc/mono/4.5/machine.config` | mono 启动时读取的 CLR 机器配置 |
| `Lucky/Vendor/mono/lib/mono/4.5/**` | BCL（`mscorlib.dll` 等），mono JIT 初始化时必须能找到 |

> 具体子目录以选用的 mono 发行版实际结构为准。这里给出的是 Hazel 版本的目录形态。

### 需要修改

| 文件 | 说明 |
|------|------|
| `Dependencies.lua` | 追加 `IncludeDir["mono"]`、`LibraryDir["mono"]`、`Library["mono"]` |
| `Lucky/Build-Lucky.lua` | `includedirs` 追加 mono 头；每个 `filter "configurations:*"` 追加 `libdirs` + `links` |
| `Luck3DApp/Build-Luck3DApp.lua` | 每个 `filter "configurations:*"` 的 `postbuildcommands` 追加 mono DLL 与 `mono/` 目录拷贝 |

### 无需修改

- `Build.lua`：mono 不是 premake 子项目（没有 `premake5.lua`），无需 `group "Dependencies" include`
- 任何 C++ 源码：本 Phase 只是让工程"能链上"，不引入任何调用

---

## 3. 现状分析

### 3.1 现有 Vendor 依赖组织方式

参考 [Dependencies.lua](../../Dependencies.lua)（截至 Phase 0.6）：

```lua
IncludeDir = {}
IncludeDir["GLFW"]   = "%{wks.location}/Lucky/Vendor/GLFW/include"
IncludeDir["assimp"] = "%{wks.location}/Lucky/Vendor/assimp/include"
-- ...

LibraryDir = {}
LibraryDir["assimp_Debug"]   = "%{wks.location}/Lucky/Vendor/assimp/bin/windows/Debug"
LibraryDir["assimp_Release"] = "%{wks.location}/Lucky/Vendor/assimp/bin/windows/Release"

Library = {}
Library["assimp_Debug"]   = "assimp-vc143-mtd"
Library["assimp_Release"] = "assimp-vc143-mt"
```

**观察**：`assimp` 是唯一一个"预编译二进制 + 按配置分 Debug/Release"的依赖，正好可作为 mono 的模板。GLFW / GLAD / imgui / yaml-cpp 都是 premake 子项目，不适合参考。

### 3.2 现有 postbuildcommands 拷贝方式

参考 [Luck3DApp/Build-Luck3DApp.lua](../../Luck3DApp/Build-Luck3DApp.lua)：

```lua
filter "configurations:Debug"
    postbuildcommands
    {
        '{COPY} "%{wks.location}/Lucky/Vendor/assimp/bin/windows/Debug/assimp-vc143-mtd.dll" "%{cfg.targetdir}"',
    }
```

**观察**：`{COPY}` 是 premake 的跨平台令牌。DLL 直接拷贝到 `targetdir` 即可（与 exe 同目录，Windows 加载器能找到）。BCL 目录（`mono/`）则需要保留目录结构，用 `{COPYDIR}` 或 xcopy 通配符实现。

### 3.3 静态运行时状态

`Lucky` 和 `Luck3DApp` 均声明 `staticruntime "off"`，即使用 `/MD`（动态 CRT）。这与 mono 官方 Windows 二进制包的 `/MT` 存在潜在冲突??运行时可能出现 CRT 堆分离、`FILE*` 跨模块传递崩溃等问题。**必须选用同样 `/MD` 编译的 mono lib**（Hazel 官方仓库里的 `Hazel-dev/Hazel/vendor/mono/lib/**` 就是 `/MD` 版本，直接拿来用即可）。

### 3.4 项目当前不存在的目录

`Lucky/Vendor/mono/` 目录**不存在**。Phase 1.1 的动作从"创建这个目录并把二进制放进去"开始。

---

## 4. 关键设计决策（方案对比）

### 4.1 【决策点 1】mono 二进制来源

#### 方案 A：直接沿用 Hazel Engine 的预编译二进制（**推荐 ★★★**）

- 从 Hazel 官方仓库 `Hazel-dev` 的 `Hazel/vendor/mono/` 复制过来（include / lib / bin / etc / lib/mono/4.5）
- Hazel 已经踩过 CRT 匹配、BCL 目录结构、debug/release 兼容等所有坑
- 使用的是 mono 6.12 官方版本，稳定

- **优点**
  - 与 Roadmap 中"参考 Hazel-ScriptCore"的定位天然吻合，后续 P1.2 / P1.3 的实现可以直接对照 Hazel 源码
  - 无需自己在 Windows 上重编 mono（mono 官方 Windows 构建流程只支持 mingw + msys，复杂度高）
  - 已知 `/MD` 编译，与 `staticruntime "off"` 匹配
- **缺点**
  - 依赖对 Hazel 仓库的一次性拷贝，属于外部资产（约 30MB）
  - Hazel 版本升级时不会自动同步（但对本项目不敏感，mono 6.12 是稳定版）

#### 方案 B：从 mono-project.com 下载官方 Windows Installer

- 官方安装包（`mono-6.12.x-gtksharp-x64-openssl.msi`）里带有完整的 SDK
- 拆出 `include / lib / bin / etc` 放到 Vendor

- **优点**：来源官方权威
- **缺点**
  - 官方 Windows 版是 `/MT`，需要额外确认或自己编 `/MD` 版本
  - 官方安装包体积大（约 400MB），需要手动裁剪
  - Hazel 项目实测过的兼容性无法直接复用

#### 方案 C：源码编译

- **优点**：完全掌控版本
- **缺点**：工具链复杂（msys2 + mingw），编译时间以小时计，且 Windows 上 sgen GC 编译容易出问题

**结论**：采用**方案 A**（沿用 Hazel）。

### 4.2 【决策点 2】Debug 版 mono lib 是否使用独立版本

#### 方案 A：Debug / Release / Dist 三个配置全部链接同一份 `mono-2.0-sgen.lib`（**推荐 ★★★**）

- **优点**
  - 与 Hazel 一致（Hazel 也不给 mono 分 debug）
  - 减少 Vendor 目录体积（少一份 lib + dll + pdb）
  - Debug 配置里若真需要看 mono 内部堆栈，可以后期单独引入 `mono-2.0-sgen-debug.lib` + `mono-2.0-sgen.pdb`
- **缺点**
  - Debug 构建的 Luck3DApp 里，跨到 mono 内部时无源码堆栈（只有函数名）??接受

#### 方案 B：mono 也分 Debug / Release 两份

- **优点**：符合本项目 `assimp` 的现有惯例
- **缺点**
  - Hazel 未提供 Debug 版 mono lib，需要另找或自行编译
  - 增加维护成本，收益低（脚本系统 MVP 阶段几乎不需要调试 mono 本身）

**结论**：采用**方案 A**。`Dependencies.lua` 里 mono 的 `LibraryDir` 只需要一个 key（不带 `_Debug` / `_Release` 后缀），或者两个 key 指向同一目录??后者代码更整齐，见 §5 实现步骤。

### 4.3 【决策点 3】BCL（`mono/lib/mono/4.5`）的部署路径

mono `mono_set_assemblies_path()` 要求指定一个目录，其下能找到 `mono/4.5/mscorlib.dll` 等 BCL。运行时布局有两种候选：

#### 方案 A：exe 同级目录下放 `mono/lib/...`，运行时 `mono_set_assemblies_path("mono/lib")`（**推荐 ★★★**）

exe 目录布局：

```
Luck3DApp.exe
mono-2.0-sgen.dll
mono/
  ├─ lib/
  │   └─ mono/
  │       └─ 4.5/
  │           └─ mscorlib.dll ...
  └─ etc/
      └─ mono/
          ├─ config
          └─ 4.5/
              └─ machine.config
```

- **优点**
  - 与 Hazel 完全一致，方便后续 P1.3 的实现照抄
  - `mono/` 目录结构一体化，未来添加 `mono/etc/mono/config` 时天然对齐
- **缺点**：无

#### 方案 B：只拷贝 BCL 到自定义子目录（例如 `Resources/mono/lib`）

- **优点**：exe 同级目录更清爽
- **缺点**：偏离 mono 官方 / Hazel 惯例；未来接第三方 mono 插件容易踩路径问题

**结论**：采用**方案 A**。P1.1 阶段先把整棵 `mono/` 目录拷到 exe 同级，P1.3 的 `mono_set_assemblies_path("mono/lib")` 直接生效。

### 4.4 【决策点 4】mono 目录的拷贝方式（premake 语法）

exe 同级需要一份完整的 `mono/` 目录树（可能 20MB+，几十个文件）。有三种拷贝方式：

#### 方案 A：premake 的 `{COPYDIR}` 令牌（**推荐 ★★★**）

```lua
postbuildcommands
{
    '{COPY} "%{wks.location}/Lucky/Vendor/mono/bin/Release/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
    '{COPYDIR} "%{wks.location}/Lucky/Vendor/mono/etc" "%{cfg.targetdir}/mono/etc"',
    '{COPYDIR} "%{wks.location}/Lucky/Vendor/mono/lib/mono" "%{cfg.targetdir}/mono/lib/mono"',
}
```

- **优点**
  - 跨平台（Windows 用 xcopy /E /I / Q，Linux 用 cp -r）
  - premake 5.0.0-beta1 之后原生支持
  - 语义清晰
- **缺点**：全量递归拷贝，若 `mono/` 目录有 100 个文件，每次编译都要拷 100 次；但可以配合 `xcopy /D` 只拷贝修改过的（premake `{COPYDIR}` 内部实现即包含此优化）

#### 方案 B：显式 `xcopy` 命令

```lua
postbuildcommands
{
    '{ECHO} "Copying mono runtime..."',
    'xcopy /E /Y /I /Q "%{wks.location}/Lucky/Vendor/mono/lib/mono" "%{cfg.targetdir}/mono/lib/mono"',
}
```

- **优点**：完全掌控参数
- **缺点**：跨平台差，只能 Windows 用；未来做 Linux 版编辑器时要改

#### 方案 C：不拷贝，让 exe 通过 `mono_set_assemblies_path` 指向 Vendor 目录

```cpp
mono_set_assemblies_path("../Lucky/Vendor/mono/lib");
```

- **优点**：零拷贝、零 postbuild、编译最快
- **缺点**
  - 相对路径依赖 exe 相对 workspace 的位置，`Binaries/windows-x64-Debug/Luck3DApp/` 深度不同，代码里 `../../../Lucky/Vendor/...` 又难写又易错
  - Dist 打包发布时无法直接把 `Binaries/**/Dist/Luck3DApp/` 拷走使用（mono 会找不到 BCL）
  - 与 Hazel 不一致

**结论**：采用**方案 A**（`{COPYDIR}`）。

### 4.5 【决策点 5】`Lucky-ScriptCore.dll` 的存放路径

虽然 `Lucky-ScriptCore.dll` 是 P1.2 的产物，但**部署路径**在 P1.1 就要定好，避免 postbuild 拷贝方案需要二次改动。

#### 方案 A：exe 同级目录 `Resources/Scripts/Lucky-ScriptCore.dll`（**推荐 ★★★**）

- **优点**
  - 与项目现有的 `Resources/Icons/**`、`Resources/Shaders/**` 目录风格统一
  - 与用户脚本 `Assets/Scripts/Binaries/App.dll` 明确区分（一个是引擎自带，一个是项目产物）
  - P1.3 里 `ScriptEngine::LoadCoreAssembly("Resources/Scripts/Lucky-ScriptCore.dll")` 路径固定，无需可配置
- **缺点**：无

#### 方案 B：exe 同级根目录 `Lucky-ScriptCore.dll`

- **优点**：路径最短
- **缺点**：与 mono BCL、Assimp DLL 混在一起，未来引擎侧再多几个 dll 时目录乱

#### 方案 C：与 mono BCL 放一起：`mono/lib/Lucky-ScriptCore.dll`

- **优点**：便于 mono 通过 assemblies path 自动找到
- **缺点**：污染 mono 目录（未来重新拉 mono 时容易忘记备份）；且 Hazel 也不是这么做的

**结论**：采用**方案 A**。P1.1 不实际拷贝这个 dll（因为文件还不存在），但 §5 的实施步骤会预留占位注释，P1.2 落地时启用。

---

## 5. 实现步骤

按下列顺序落地，每一步落地后 `premake5 vs2022` 都能生成、`Lucky` 都能编译。

### Step 1：创建 Vendor 目录并放入二进制

从 Hazel-dev 仓库 `Hazel/vendor/mono/` 完整拷贝到 `D:/Projects/C++/Luck3D/Lucky/Vendor/mono/`。目录布局最终应为：

```
Lucky/Vendor/mono/
├─ include/
│   └─ mono-2.0/
│       └─ mono/
│           ├─ jit/jit.h
│           ├─ metadata/*.h
│           └─ utils/*.h
├─ lib/
│   ├─ Debug/
│   │   └─ mono-2.0-sgen.lib
│   ├─ Release/
│   │   └─ mono-2.0-sgen.lib
│   └─ mono/
│       └─ 4.5/
│           ├─ mscorlib.dll
│           ├─ System.dll
│           └─ ...
├─ bin/
│   ├─ Debug/
│   │   └─ mono-2.0-sgen.dll
│   └─ Release/
│       └─ mono-2.0-sgen.dll
└─ etc/
    └─ mono/
        ├─ config
        └─ 4.5/machine.config
```

> **说明**：如果 Hazel 只提供一份 `lib/mono-2.0-sgen.lib`（未分 Debug/Release），就把它同时放到 `lib/Debug/` 和 `lib/Release/` 下（复制两份）。`bin/` 下的 DLL 同理。这样 `Dependencies.lua` 的 `LibraryDir` / `Library` 写法与 `assimp` 完全对称，易读易维护。

### Step 2：修改 `Dependencies.lua`

在文件末尾追加（与 assimp 段并列）：

```lua
-- 包含目录
IncludeDir["mono"] = "%{wks.location}/Lucky/Vendor/mono/include/mono-2.0"

-- 库目录
LibraryDir["mono_Debug"]   = "%{wks.location}/Lucky/Vendor/mono/lib/Debug"
LibraryDir["mono_Release"] = "%{wks.location}/Lucky/Vendor/mono/lib/Release"

-- Libs
Library["mono_Debug"]   = "mono-2.0-sgen"
Library["mono_Release"] = "mono-2.0-sgen"
```

> **注意 include 深度**：mono 头文件通常按 `#include <mono/jit/jit.h>` 方式引用，所以 include 根要指向 `mono-2.0/`（其下第一级是 `mono/`）。若 Vendor 目录里没有 `mono-2.0/` 中间层（例如 Hazel 版本直接就是 `include/mono/jit/jit.h`），则改为 `IncludeDir["mono"] = ".../Vendor/mono/include"`。以实际 Vendor 布局为准。

### Step 3：修改 `Lucky/Build-Lucky.lua`

**改动 A**：`includedirs` 段追加：

```lua
includedirs
{
    "Source",
    "Vendor",
    -- ... 现有条目 ...
    "%{IncludeDir.assimp}",
    "%{IncludeDir.mono}",
}
```

**改动 B**：Windows 系统需要额外链接 `Bcrypt.lib / Version.lib / Winmm.lib / Ws2_32.lib`??**这些已经存在于 `filter "system:windows"` 的 links 段**（`Dependencies.lua` 中 `Library["WinSock"]` 等），所以无额外动作。

**改动 C**：三个 configuration filter 追加 mono 的 libdir + link。当前 assimp 用的是"Debug 走 mtd 版、Release/Dist 走 mt 版"的模式，mono 也照办：

```lua
filter "configurations:Debug"
    libdirs
    {
        "%{LibraryDir.assimp_Debug}",
        "%{LibraryDir.mono_Debug}",
    }
    links
    {
        "%{Library.assimp_Debug}",
        "%{Library.mono_Debug}",
    }

    defines { "LF_DEBUG" }
    runtime "Debug"
    symbols "On"

filter "configurations:Release"
    libdirs
    {
        "%{LibraryDir.assimp_Release}",
        "%{LibraryDir.mono_Release}",
    }
    links
    {
        "%{Library.assimp_Release}",
        "%{Library.mono_Release}",
    }

    defines { "LF_RELEASE" }
    runtime "Release"
    optimize "On"
    symbols "On"

filter "configurations:Dist"
    libdirs
    {
        "%{LibraryDir.assimp_Release}",
        "%{LibraryDir.mono_Release}",
    }
    links
    {
        "%{Library.assimp_Release}",
        "%{Library.mono_Release}",
    }

    defines { "LF_DIST" }
    runtime "Release"
    optimize "On"
    symbols "Off"
```

> 原本 `libdirs { "..." } links { "..." }` 的单条写法要改成多条数组写法（premake 允许 libdirs / links 多次调用累加）。为了跟 assimp 组织在一起，用数组式更整齐。

### Step 4：修改 `Luck3DApp/Build-Luck3DApp.lua`

**改动**：三个 configuration 的 `postbuildcommands` 追加 mono DLL + `mono/` 目录拷贝。以 Debug 为例：

```lua
filter "configurations:Debug"
    defines { "LF_DEBUG" }
    runtime "Debug"
    symbols "On"

    postbuildcommands
    {
        '{COPY} "%{wks.location}/Lucky/Vendor/assimp/bin/windows/Debug/assimp-vc143-mtd.dll" "%{cfg.targetdir}"',
        '{COPY} "%{wks.location}/Lucky/Vendor/mono/bin/Debug/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
        '{COPYDIR} "%{wks.location}/Lucky/Vendor/mono/etc" "%{cfg.targetdir}/mono/etc"',
        '{COPYDIR} "%{wks.location}/Lucky/Vendor/mono/lib/mono" "%{cfg.targetdir}/mono/lib/mono"',
    }
```

Release / Dist 段同理，把 `bin/Debug` 换成 `bin/Release`。

**注意 `{COPYDIR}` 的目标路径**：写成 `"%{cfg.targetdir}/mono/etc"` 而不是 `"%{cfg.targetdir}/mono"`，因为 `{COPYDIR}` 是把源目录**下的所有内容**拷到目标目录，而不是把源目录本身作为子目录嵌入。这样最终 `targetdir/mono/etc/mono/config` 才对应源 `Vendor/mono/etc/mono/config`。

### Step 5：`premake5 vs2022` 重新生成工程并验证

- 执行仓库根目录下的 `Setup.bat` 或 `premake5 vs2022 Build.lua`
- 打开生成的 `Luck3D.sln`
- **验证 1**：`Lucky.vcxproj` 的 "Additional Include Directories" 里出现 `Vendor/mono/include/mono-2.0`
- **验证 2**：`Lucky.vcxproj` 的 "Additional Library Directories" 里出现 `Vendor/mono/lib/Debug`（Debug 配置下）
- **验证 3**：`Lucky.vcxproj` 的 "Additional Dependencies" 里出现 `mono-2.0-sgen.lib`
- **验证 4**：Debug 编译 `Lucky` 通过；Release 编译 `Lucky` 通过；Dist 编译 `Lucky` 通过
- **验证 5**：编译 `Luck3DApp` 通过，输出目录 `Binaries/windows-x64-Debug/Luck3DApp/` 下能看到：
  - `Luck3DApp.exe`
  - `mono-2.0-sgen.dll`
  - `mono/etc/mono/config`
  - `mono/etc/mono/4.5/machine.config`
  - `mono/lib/mono/4.5/mscorlib.dll`

**验证 5 若某个文件缺失**：绝大多数情况是 `{COPYDIR}` 的源路径与实际 Vendor 布局不符（Hazel 版有的把 `4.5/` 直接放在 `mono/lib/mono/4.5/`，有的多一层）。按实际目录调整 postbuildcommands 的源路径即可。

---

## 6. 坑点提醒

### 6.1 CRT 匹配（`/MD` vs `/MT`）

- 项目所有 project `staticruntime "off"` → 使用 `/MD`
- 若 Vendor 中放入的 `mono-2.0-sgen.lib` 是 `/MT` 编译（例如从 mono 官方 msi 拆出的），链接时会出现 `LNK4098: 默认库"MSVCRT"与其他库的使用冲突` 类似的**警告**，运行时会有堆分离风险
- **判定方式**：`dumpbin /directives mono-2.0-sgen.lib` 查看，输出里若有 `/DEFAULTLIB:libcmt` 就是 `/MT`；若有 `/DEFAULTLIB:msvcrt` 就是 `/MD`
- **规避方式**：Hazel-dev 仓库里的 mono lib 是 `/MD` 版本，直接用它。若 Hazel 也换成 `/MT` 了，退回到自行下载 xamarin 提供的 `/MD` 版本 mono，或者暂时把项目改成 `staticruntime "on"`（但这会连锁影响 assimp / GLFW / spdlog 等所有依赖，代价太大）

### 6.2 Windows 上 mono 头文件需要 `WIN32_LEAN_AND_MEAN` 前置

若在某个 cpp 里同时 include `<Windows.h>` 和 `<mono/jit/jit.h>`，mono 头会引入一个 `interface` 关键字冲突（Windows COM 头文件把 `interface` 定义为 `struct` 宏）。**规避方式**：mono 相关的 include 放在最前面；或者在 include mono 头之前 `#undef interface`。

本 Phase 不涉及此问题（还没有 cpp 引用 mono 头），但要提前留意；P1.3 落地时会在 `ScriptEngine.cpp` 里处理。

### 6.3 `{COPYDIR}` 与目标目录已存在

`{COPYDIR}` 首次运行会创建目标目录；后续运行会把源目录里的**新增/更新**文件复制过去，但**不会删除**目标目录里被源侧删掉的文件。这意味着：如果未来 mono 版本升级、某个 BCL dll 被移除，目标目录会残留旧文件。**MVP 阶段无影响**；Phase 3 若做发布打包脚本时再考虑先 rimraf。

### 6.4 `premake5 vs2022` 命令入口

- 项目根 premake 主文件是 `Build.lua`（不是默认的 `premake5.lua`），所以命令写法是：
  ```
  premake5 --file=Build.lua vs2022
  ```
- 或者项目提供的 `Setup.bat` 已经封装好该命令
- 文档中提示"重新生成工程"时，必须走这个入口

### 6.5 静态库 `Lucky` 的 mono link 是否会传递给 `Luck3DApp`

- premake 生成的 vcxproj 中，静态库项目声明的 `links` 会通过 `<AdditionalDependencies>` 传递给依赖它的可执行项目
- 因此 `Luck3DApp` 无需重复在自己的 `Build-Luck3DApp.lua` 里 `links { "mono-2.0-sgen" }`??`Lucky` 那边配好即可
- **例外**：`libdirs` 也是传递的；无需在 `Luck3DApp` 侧重复

### 6.6 若使用 include depth 为 `mono-2.0` 时的写法

Hazel 版 mono 的 include 目录结构可能是这两种之一：

- 版本 X：`Vendor/mono/include/mono/jit/jit.h`（无中间 `mono-2.0`）
- 版本 Y：`Vendor/mono/include/mono-2.0/mono/jit/jit.h`（带中间 `mono-2.0`）

- 版本 X：`IncludeDir["mono"] = ".../Vendor/mono/include"`
- 版本 Y：`IncludeDir["mono"] = ".../Vendor/mono/include/mono-2.0"`

- 判断方式：拷贝完 Vendor 目录后，在文件系统里搜 `jit.h`，看它上面隔了多少层。上面 §5 Step 2 的示例用的是版本 Y。落地时以实际布局为准。

---

## 7. 验收标准

对照 Roadmap Phase 1 第 (1) 条"引入 Mono 依赖"，本 Phase 完成后应满足：

1. **premake 生成通过**：`premake5 --file=Build.lua vs2022` 无 warning、无 error
2. **Lucky 三配置编译通过**：Debug / Release / Dist 三个配置下 `Lucky` 静态库均可编译
3. **Luck3DApp 三配置编译通过**：Debug / Release / Dist 三个配置下 `Luck3DApp` 均可编译、可启动（启动后行为与 Phase 0 完全一致，无脚本系统影响）
4. **产物齐全**：`Binaries/windows-x64-Debug/Luck3DApp/` 下同时存在：
   - `Luck3DApp.exe`
   - `mono-2.0-sgen.dll`
   - `mono/etc/mono/config`
   - `mono/etc/mono/4.5/machine.config`
   - `mono/lib/mono/4.5/mscorlib.dll`
5. **零回归**：编辑器功能与 Phase 0.6 完成时完全一致??Scene / Game 面板、Play / Stop 工具条、Inspector、SceneHierarchy 均正常
6. **烟囱测试（可选，推荐做一次）**：在 `Sandbox`（或临时 test）代码中加一段：
   ```cpp
   #include <mono/jit/jit.h>
   // ...
   MonoDomain* domain = mono_jit_init("LuckyTest");
   LUCKY_INFO("mono version: {}", mono_get_runtime_build_info());
   mono_jit_cleanup(domain);
   ```
   跑一次能打印出 mono 版本号，即证明"链接 + DLL 加载 + BCL 定位"整条链路通畅。**验证完删除该临时代码**（对应记忆规则：不留阶段性代码 [[memory:du4s18ic]]）。

---

## 8. 后续 Phase 的接入点

本 Phase 落地后，下列节点可以在后续 Phase 中直接使用：

| 位置 | 后续 Phase | 会做什么 |
|------|-----------|---------|
| `Lucky/Vendor/mono/**` | P1.3 | `ScriptEngine.cpp` include `<mono/jit/jit.h>` 等头文件 |
| `Luck3DApp/**/mono/` 部署目录 | P1.3 | `mono_set_assemblies_path("mono/lib")` 加载 BCL |
| `Luck3DApp/**/Resources/Scripts/Lucky-ScriptCore.dll` | P1.2 → P1.3 | P1.2 输出该 dll 到此路径；P1.3 `LoadCoreAssembly` 从此路径加载 |
| `Luck3DApp/Assets/Scripts/Binaries/App.dll` | P1.2 → P1.3 | P1.7 编译 Sandbox 输出到此路径；P1.3 `LoadAppAssembly` 从此路径加载 |

---

## 9. 变更清单速览

- **新增**
  - `Lucky/Vendor/mono/` 目录树（外部二进制资产，从 Hazel-dev 拷贝）
- **修改**
  - `Dependencies.lua`：追加 `IncludeDir["mono"]` / `LibraryDir["mono_Debug"] / [mono_Release]` / `Library["mono_Debug"] / [mono_Release]`
  - `Lucky/Build-Lucky.lua`：`includedirs` 追加 mono include；三个 configuration filter 追加 mono libdir + link
  - `Luck3DApp/Build-Luck3DApp.lua`：三个 configuration 的 `postbuildcommands` 追加 mono DLL 拷贝 + `mono/etc`、`mono/lib/mono` 目录拷贝
- **删除**：无

---

## 10. 参考

- Hazel Engine（`Hazel-dev/Hazel/vendor/mono/`）：mono 预编译资产与目录结构参考
- Mono Embedding 官方文档：<https://www.mono-project.com/docs/advanced/embedding/>
- premake 文档 `postbuildcommands` / `{COPY}` / `{COPYDIR}`：<https://premake.github.io/docs/Tokens/>
