# Luck3D

一个基于C++和imgui的简单3D建模软件。
持续更新中...

`Scripts/`目录包含 `Windows` 和 `Linux` 的构建脚本，`Vendor/`目录包含 Premake 二进制文件（当前版本为`5.0-beta2`）。

## 构建指南
1. 递归地克隆该存储库 `git clone --recursive https://github.com/LuckyGhostStudio/Luck3D`。
2. 打开 `Scripts/` 目录，运行对应平台的 `Setup-XXX` 脚本来生成项目文件。也可以通过更改脚本来更改生成的项目类型，当前 Windows 项目设置为 Visual Studio 2022，Linux 项目设置为 gmake2。
3. 三个 Premake 构建文件是 `Build.lua`、`Core/Build-Lucky.lua` 和 `App/Build-Luck3DApp.lua`。可以编辑这些文件来定制构建配置，编辑项目名称和工作区/解决方案等。

目前没有提供 MacOS 平台的构建脚本，可以复制 Linux 脚本并进行相应调整。

## 许可证
- 此存储库的 UNLICENSE (`UNLICENSE.txt` 文件)
- Premake 根据 BSD 3-Clause 获得许可 (`LICENSE.txt` 文件)