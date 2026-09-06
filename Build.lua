-- premake5.lua
workspace "Luck3D"      -- 解决方案名称
    architecture "x64"  -- 体系结构
    configurations { "Debug", "Release", "Dist" }
    startproject "Luck3DApp"

    flags { "MultiProcessorCompile" }

outputdir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"     -- 输出目录

include "Dependencies.lua"

-- 依赖
group "Dependencies"
    include "Lucky/Vendor/GLFW"     -- 包含 GLFW 目录
    include "Lucky/Vendor/GLAD"     -- 包含 GLAD 目录
    include "Lucky/Vendor/imgui"    -- 包含 imgui 目录
    include "Lucky/Vendor/yaml-cpp" -- 包含 yaml-cpp 目录
group ""

group "Core"
    include "Lucky/Build-Lucky.lua"
group ""

group "Tools"
    include "Luck3DApp/Build-Luck3DApp.lua"
group ""