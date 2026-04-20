# Assimp 自动化构建集成方案设计文档

## 1. 背景与问题

### 1.1 当前状况

Luck3D 项目通过 git submodule 引用 assimp 库（位于 `Lucky/Vendor/assimp/`），当前采用**方案一（预编译二进制）**的方式集成：

- 预编译好的 `.lib` 和 `.dll` 文件存放在 `Lucky/Vendor/assimp-bin/windows/` 目录
- premake 脚本通过 `libdirs` 和 `links` 链接静态导入库
- 通过 `postbuildcommands` 在构建后自动复制 DLL 到可执行文件目录

### 1.2 方案一的局限性

| 问题 | 说明 |
|------|------|
| git 仓库体积增大 | 预编译二进制文件（~25MB）需要提交到仓库 |
| 版本更新不便 | 每次更新 assimp 子模块版本后，需要手动重新用 CMake 构建并替换二进制 |
| 跨平台扩展困难 | 每增加一个平台/编译器组合，都需要手动构建并提交对应的二进制 |
| 编译器版本耦合 | 预编译的 `assimp-vc143-mt.lib` 与 VS2022 绑定，换编译器需重新构建 |

### 1.3 方案二的目标

**将 CMake 构建 assimp 的完整流程自动化**，使开发者在首次 clone 项目后，只需运行一个脚本即可完成所有构建准备工作，无需手动操作 CMake。

---

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                    开发者工作流                           │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  git clone --recursive ...                              │
│       │                                                 │
│       ▼                                                 │
│  Scripts/Setup-Windows.bat                              │
│       │                                                 │
│       ├──? [检测] assimp-bin 是否已存在？                │
│       │         │                                       │
│       │    ┌────┴────┐                                  │
│       │    │ 不存在   │                                  │
│       │    └────┬────┘                                  │
│       │         ▼                                       │
│       ├──? Scripts/Build-Assimp-Windows.bat             │
│       │         │                                       │
│       │         ├── cmake -G ... (生成项目)              │
│       │         ├── cmake --build ... Debug             │
│       │         ├── cmake --build ... Release           │
│       │         └── 复制产物到 assimp-bin/               │
│       │                                                 │
│       ▼                                                 │
│  premake5 --file=Build.lua vs2022                       │
│       │                                                 │
│       ▼                                                 │
│  打开 .sln 开始开发                                      │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

---

## 3. 目录结构设计

```
Luck3D/
├── Scripts/
│   ├── Setup-Windows.bat          # 项目初始化入口（修改）
│   ├── Setup-Linux.sh             # Linux 初始化入口（修改）
│   └── Build-Assimp-Windows.bat   # assimp 自动构建脚本（新增）
├── Lucky/
│   └── Vendor/
│       ├── assimp/                # git submodule（不修改）
│       └── assimp-bin/            # 构建产物输出目录
│           └── windows/
│               ├── Debug/
│               │   ├── assimp-vc143-mtd.dll
│               │   └── assimp-vc143-mtd.lib
│               └── Release/
│                   ├── assimp-vc143-mt.dll
│                   └── assimp-vc143-mt.lib
├── Build.lua                      # premake 入口（可选：增加检测逻辑）
├── Dependencies.lua               # 依赖路径定义（不变）
├── .gitignore                     # 忽略 assimp-bin/ 和 assimp/build/
└── ...
```

---

## 4. 详细实现

### 4.1 构建脚本：`Scripts/Build-Assimp-Windows.bat`

```bat
@echo off
setlocal enabledelayedexpansion

echo ============================================
echo   Assimp Automated Build Script (Windows)
echo ============================================
echo.

:: ============ 配置区 ============
SET SCRIPT_DIR=%~dp0
SET PROJECT_ROOT=%SCRIPT_DIR%..
SET ASSIMP_SOURCE=%PROJECT_ROOT%\Lucky\Vendor\assimp
SET BUILD_DIR=%ASSIMP_SOURCE%\build
SET OUTPUT_DIR=%PROJECT_ROOT%\Lucky\Vendor\assimp-bin\windows

:: CMake 生成器（可根据安装的 VS 版本修改）
SET CMAKE_GENERATOR=Visual Studio 17 2022
SET CMAKE_ARCH=x64

:: ============ 前置检查 ============
echo [1/5] 检查 CMake 是否可用...
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [错误] 未找到 CMake！请安装 CMake 并确保其在 PATH 中。
    echo        下载地址: https://cmake.org/download/
    exit /b 1
)
for /f "tokens=3" %%v in ('cmake --version ^| findstr /i "cmake version"') do (
    echo        CMake 版本: %%v
)
echo.

:: ============ 检查源码 ============
echo [2/5] 检查 assimp 源码...
if not exist "%ASSIMP_SOURCE%\CMakeLists.txt" (
    echo [错误] 未找到 assimp 源码！请确保已初始化 git submodule：
    echo        git submodule update --init --recursive
    exit /b 1
)
echo        源码路径: %ASSIMP_SOURCE%
echo.

:: ============ CMake 配置 ============
echo [3/5] 配置 CMake 项目...
cmake -G "%CMAKE_GENERATOR%" -A %CMAKE_ARCH% ^
    -S "%ASSIMP_SOURCE%" ^
    -B "%BUILD_DIR%" ^
    -DBUILD_SHARED_LIBS=ON ^
    -DASSIMP_BUILD_TESTS=OFF ^
    -DASSIMP_INSTALL=OFF ^
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF ^
    -DASSIMP_BUILD_SAMPLES=OFF ^
    -DASSIMP_BUILD_DOCS=OFF ^
    -DASSIMP_WARNINGS_AS_ERRORS=OFF ^
    -DASSIMP_NO_EXPORT=OFF

if %ERRORLEVEL% neq 0 (
    echo [错误] CMake 配置失败！
    exit /b 1
)
echo.

:: ============ 构建 Debug ============
echo [4/5] 构建 Debug 配置...
cmake --build "%BUILD_DIR%" --config Debug --parallel
if %ERRORLEVEL% neq 0 (
    echo [错误] Debug 构建失败！
    exit /b 1
)

:: 复制 Debug 产物
if not exist "%OUTPUT_DIR%\Debug" mkdir "%OUTPUT_DIR%\Debug"
copy /Y "%BUILD_DIR%\bin\Debug\assimp-vc143-mtd.dll" "%OUTPUT_DIR%\Debug\" >nul
copy /Y "%BUILD_DIR%\lib\Debug\assimp-vc143-mtd.lib" "%OUTPUT_DIR%\Debug\" >nul
echo        Debug 产物已复制到: %OUTPUT_DIR%\Debug\
echo.

:: ============ 构建 Release ============
echo [5/5] 构建 Release 配置...
cmake --build "%BUILD_DIR%" --config Release --parallel
if %ERRORLEVEL% neq 0 (
    echo [错误] Release 构建失败！
    exit /b 1
)

:: 复制 Release 产物
if not exist "%OUTPUT_DIR%\Release" mkdir "%OUTPUT_DIR%\Release"
copy /Y "%BUILD_DIR%\bin\Release\assimp-vc143-mt.dll" "%OUTPUT_DIR%\Release\" >nul
copy /Y "%BUILD_DIR%\lib\Release\assimp-vc143-mt.lib" "%OUTPUT_DIR%\Release\" >nul
echo        Release 产物已复制到: %OUTPUT_DIR%\Release\
echo.

:: ============ 完成 ============
echo ============================================
echo   Assimp 构建完成！
echo ============================================
echo.
echo   Debug:   %OUTPUT_DIR%\Debug\
echo   Release: %OUTPUT_DIR%\Release\
echo.

endlocal
exit /b 0
```

### 4.2 修改项目初始化脚本：`Scripts/Setup-Windows.bat`

```bat
@echo off

echo ============================================
echo   Luck3D Project Setup (Windows)
echo ============================================
echo.

:: 检查 assimp 二进制是否已存在
if not exist "..\Lucky\Vendor\assimp-bin\windows\Debug\assimp-vc143-mtd.dll" (
    echo [信息] Assimp 预编译二进制未找到，开始自动构建...
    echo.
    call "%~dp0Build-Assimp-Windows.bat"
    if %ERRORLEVEL% neq 0 (
        echo [错误] Assimp 构建失败，请检查错误信息。
        pause
        exit /b 1
    )
    echo.
) else (
    echo [信息] Assimp 预编译二进制已存在，跳过构建。
    echo.
)

:: 生成项目文件
echo [信息] 生成 Visual Studio 2022 项目文件...
pushd ..
Vendor\Binaries\Premake\Windows\premake5.exe --file=Build.lua vs2022
popd

echo.
echo ============================================
echo   项目设置完成！
echo ============================================
pause
```

### 4.3 可选：在 `Build.lua` 中增加检测逻辑

```lua
-- Build.lua 开头增加 assimp 检测
local assimpDebugDll = "Lucky/Vendor/assimp-bin/windows/Debug/assimp-vc143-mtd.dll"
local assimpReleaseDll = "Lucky/Vendor/assimp-bin/windows/Release/assimp-vc143-mt.dll"

if os.host() == "windows" then
    if not os.isfile(assimpDebugDll) or not os.isfile(assimpReleaseDll) then
        print("")
        print("========================================================")
        print("  [警告] Assimp 预编译二进制未找到！")
        print("  请先运行 Scripts/Build-Assimp-Windows.bat 构建 assimp")
        print("  或运行 Scripts/Setup-Windows.bat 自动完成所有设置")
        print("========================================================")
        print("")
        -- 可选：自动触发构建
        -- os.execute("Scripts\\Build-Assimp-Windows.bat")
    end
end
```

### 4.4 `.gitignore` 配置

```gitignore
# Assimp 构建中间文件
Lucky/Vendor/assimp/build/

# Assimp 预编译二进制产物（方案二下不提交到仓库）
Lucky/Vendor/assimp-bin/windows/Debug/*.dll
Lucky/Vendor/assimp-bin/windows/Debug/*.lib
Lucky/Vendor/assimp-bin/windows/Release/*.dll
Lucky/Vendor/assimp-bin/windows/Release/*.lib
```

---

## 5. CMake 构建参数说明

| 参数 | 值 | 说明 |
|------|-----|------|
| `BUILD_SHARED_LIBS` | `ON` | 构建动态库（生成 DLL + 导入 LIB） |
| `ASSIMP_BUILD_TESTS` | `OFF` | 不构建测试，加快编译速度 |
| `ASSIMP_INSTALL` | `OFF` | 不生成安装目标（作为子模块使用） |
| `ASSIMP_BUILD_ASSIMP_TOOLS` | `OFF` | 不构建命令行工具 |
| `ASSIMP_BUILD_SAMPLES` | `OFF` | 不构建示例程序 |
| `ASSIMP_BUILD_DOCS` | `OFF` | 不构建文档 |
| `ASSIMP_WARNINGS_AS_ERRORS` | `OFF` | 不将警告视为错误（避免第三方代码编译失败） |
| `ASSIMP_NO_EXPORT` | `OFF` | 保留导出功能（如需要可设为 ON 减小体积） |
| `-G "Visual Studio 17 2022"` | - | 使用 VS2022 生成器 |
| `-A x64` | - | 64 位架构 |
| `--parallel` | - | 并行编译，加快构建速度 |

---

## 6. 构建产物映射

CMake 构建完成后，产物位于 `Lucky/Vendor/assimp/build/` 下：

```
assimp/build/
├── bin/
│   ├── Debug/
│   │   └── assimp-vc143-mtd.dll      → 复制到 assimp-bin/windows/Debug/
│   └── Release/
│       └── assimp-vc143-mt.dll        → 复制到 assimp-bin/windows/Release/
└── lib/
    ├── Debug/
    │   └── assimp-vc143-mtd.lib       → 复制到 assimp-bin/windows/Debug/
    └── Release/
        └── assimp-vc143-mt.lib        → 复制到 assimp-bin/windows/Release/
```

> **注意**：实际文件名中的 `vc143` 取决于编译器版本（VS2022 = vc143），如果使用其他版本的 VS，文件名会相应变化。

---

## 7. 与现有 premake 脚本的配合

方案二**不需要修改**现有的 premake 脚本（`Dependencies.lua`、`Build-Lucky.lua`、`Build-Luck3DApp.lua`），因为：

1. `Dependencies.lua` 中已定义了 `LibraryDir.assimp_Debug` 和 `LibraryDir.assimp_Release` 指向 `assimp-bin/windows/` 目录
2. `Build-Lucky.lua` 中已配置了对应的 `libdirs` 和 `links`
3. `Build-Luck3DApp.lua` 中已配置了 `postbuildcommands` 复制 DLL

唯一的区别是：方案一将二进制提交到 git，方案二通过脚本在本地生成二进制到相同目录。

---

## 8. 开发者使用流程

### 8.1 首次 clone 项目

```bash
# 1. 克隆项目（含子模块）
git clone --recursive https://github.com/xxx/Luck3D.git
cd Luck3D

# 2. 运行项目设置脚本（自动构建 assimp + 生成 VS 项目）
Scripts\Setup-Windows.bat
```

### 8.2 更新 assimp 版本

```bash
# 1. 更新子模块
cd Lucky/Vendor/assimp
git checkout <new-version-tag>
cd ../../..
git add Lucky/Vendor/assimp
git commit -m "Update assimp to <version>"

# 2. 清理旧构建并重新构建
rmdir /s /q Lucky\Vendor\assimp\build
rmdir /s /q Lucky\Vendor\assimp-bin\windows
Scripts\Build-Assimp-Windows.bat
```

### 8.3 强制重新构建

```bash
# 删除构建缓存后重新运行
rmdir /s /q Lucky\Vendor\assimp\build
Scripts\Build-Assimp-Windows.bat
```

---

## 9. 跨平台扩展（未来）

### 9.1 Linux 构建脚本：`Scripts/Build-Assimp-Linux.sh`

```bash
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/.."
ASSIMP_SOURCE="$PROJECT_ROOT/Lucky/Vendor/assimp"
BUILD_DIR="$ASSIMP_SOURCE/build"
OUTPUT_DIR="$PROJECT_ROOT/Lucky/Vendor/assimp-bin/linux"

echo "=== Building Assimp (Linux) ==="

# 配置
cmake -S "$ASSIMP_SOURCE" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DASSIMP_BUILD_TESTS=OFF \
    -DASSIMP_INSTALL=OFF \
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF

# 构建
cmake --build "$BUILD_DIR" --parallel

# 复制产物
mkdir -p "$OUTPUT_DIR"
cp "$BUILD_DIR/bin/libassimp.so"* "$OUTPUT_DIR/"

echo "=== Assimp build complete ==="
```

### 9.2 目录结构扩展

```
assimp-bin/
├── windows/
│   ├── Debug/
│   └── Release/
└── linux/
    └── libassimp.so
```

---

## 10. 方案对比总结

| 维度 | 方案一（预编译二进制提交到 git） | 方案二（脚本自动 CMake 构建） |
|------|------|------|
| **git 仓库体积** | +25MB（二进制文件） | 无额外体积 |
| **首次构建时间** | 即时（直接使用） | 约 2-5 分钟（需编译 assimp） |
| **环境依赖** | 无额外依赖 | 需要安装 CMake（≥3.22） |
| **版本更新** | 手动构建 + 提交二进制 | 运行脚本自动完成 |
| **跨平台** | 每个平台需单独提交二进制 | 每个平台运行对应脚本即可 |
| **CI/CD 友好度** | 高（无需构建步骤） | 中（需在 CI 中增加构建步骤） |
| **团队协作** | 简单（clone 即可用） | 需确保团队成员安装 CMake |
| **premake 脚本修改** | 已完成 | 无需额外修改 |

---

## 11. 注意事项与风险

1. **CMake 版本要求**：assimp 要求 CMake ≥ 3.22，需在文档中明确说明
2. **编译器版本**：产物文件名包含 `vc143`（VS2022），如果团队成员使用不同版本的 VS，文件名会不同，需要相应调整 `Dependencies.lua` 中的库名
3. **网络依赖**：assimp 的 CMake 构建过程中可能会下载 draco 等依赖（取决于配置），需确保网络可用
4. **磁盘空间**：assimp 的完整构建目录（`build/`）约 500MB-1GB，建议在 `.gitignore` 中忽略
5. **并行构建**：使用 `--parallel` 参数可显著加快构建速度，但会占用较多内存

---

## 12. 推荐策略

**建议采用混合策略**：

- **日常开发**：使用方案一（预编译二进制已提交到仓库），开发者 clone 后即可使用
- **版本更新时**：使用方案二的构建脚本生成新的二进制，然后提交到仓库
- **CI/CD**：直接使用仓库中的预编译二进制，无需在 CI 中构建 assimp

这样既保证了日常开发的便捷性，又提供了自动化构建工具用于版本更新。
