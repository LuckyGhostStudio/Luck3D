# Luck3D

一个基于 C++ 和 imgui 的 3D 游戏引擎。
持续更新中...

## 构建指南

### 前置条件

- Windows
- Visual Studio 2022（含 C++ 桌面开发负载）
- .NET Framework 4.7.2 目标包（VS2022 安装器 → 单个组件中勾选）
- Git

### 构建步骤

**1. 克隆（递归）**

```
git clone --recursive https://github.com/LuckyGhostStudio/Luck3D
```

**2. 生成主工程**

进入 `Scripts/` 目录，运行 `Setup-Windows.bat`，生成 `Luck3D.sln`。

**3. 构建引擎与编辑器**

用 VS2022 打开 `Luck3D.sln`，选择 `Debug|x64` 或 `Release|x64`，生成解决方案。

**4. 生成并构建脚本工程**

进入 `Luck3DApp/Project/` 目录，运行 `Win-GenProject.bat` 生成 `Project.sln`；用 VS2022 打开 `Project.sln`，选择与第 3 步相同的配置，生成解决方案。

**5. 运行**

运行 `Binaries\windows-x64\<配置>\Luck3DApp\Luck3DApp.exe`（`<配置>` 为第 3 步选择的 Debug 或 Release）。

### 定制构建

Premake 构建文件共四个：`Build.lua`（工作区）、`Lucky/Build-Lucky.lua`（引擎）、`Lucky-ScriptCore/Build-Lucky-ScriptCore.lua`（脚本核心库）、`Luck3DApp/Build-Luck3DApp.lua`（编辑器应用）。可以编辑这些文件来定制构建配置、项目名称和工作区/解决方案等。

目前没有提供 MacOS 平台的构建脚本，可以复制 Linux 脚本并进行相应调整。

## 许可证
- 此存储库的 UNLICENSE (`UNLICENSE.txt` 文件)
- Premake 根据 BSD 3-Clause 获得许可 (`LICENSE.txt` 文件)