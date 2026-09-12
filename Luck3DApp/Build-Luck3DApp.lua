-- 反推仓库根：本 lua 文件位于 <repo>/Luck3DApp/Build-Luck3DApp.lua
local ThisFile = debug.getinfo(1, "S").source:sub(2)
local ThisDir = ThisFile:match("(.*/)") or ThisFile:match("(.*\\)") or "./"
local LuckyRepoRoot = string.gsub(path.getabsolute(ThisDir .. ".."), "\\", "/")

project "Luck3DApp"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("%{wks.location}/Binaries/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/Binaries/Intermediates/" .. outputdir .. "/%{prj.name}")

    files 
    { 
        "Source/**.h",
        "Source/**.cpp"
    }

    includedirs
    {
        "Source",
        "%{wks.location}/Lucky/Vendor/spdlog/include",
        "%{wks.location}/Lucky/Source",
        "%{wks.location}/Lucky/Vendor",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.yaml_cpp}",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.assimp}",
    }

    links
    {
        "Lucky"
    }

    filter "system:windows"
        systemversion "latest"
        defines
        {
            "WINDOWS",
            'LF_REPO_ROOT="' .. LuckyRepoRoot .. '"'
        }

    filter "configurations:Debug"
        defines { "LF_DEBUG" }
        runtime "Debug"
        symbols "On"

        postbuildcommands
        {
            '{COPY} "%{wks.location}/Lucky/Vendor/assimp/bin/windows/Debug/assimp-vc143-mtd.dll" "%{cfg.targetdir}"',
            'xcopy /D /E /I /Y /Q "%{wks.location}/Luck3DApp/mono" "%{cfg.targetdir}/mono\\" >NUL',
            'xcopy /D /E /I /Y /Q "%{wks.location}/Luck3DApp/Resources" "%{cfg.targetdir}/Resources\\" >NUL',
        }

    filter "configurations:Release"
        defines { "LF_RELEASE" }
        runtime "Release"
        optimize "On"
        symbols "On"

        postbuildcommands
        {
            '{COPY} "%{wks.location}/Lucky/Vendor/assimp/bin/windows/Release/assimp-vc143-mt.dll" "%{cfg.targetdir}"',
            'xcopy /D /E /I /Y /Q "%{wks.location}/Luck3DApp/mono" "%{cfg.targetdir}/mono\\" >NUL',
            'xcopy /D /E /I /Y /Q "%{wks.location}/Luck3DApp/Resources" "%{cfg.targetdir}/Resources\\" >NUL',
        }

    filter "configurations:Dist"
        defines { "LF_DIST" }
        runtime "Release"
        optimize "On"
        symbols "Off"

        postbuildcommands
        {
            '{COPY} "%{wks.location}/Lucky/Vendor/assimp/bin/windows/Release/assimp-vc143-mt.dll" "%{cfg.targetdir}"',
            'xcopy /D /E /I /Y /Q "%{wks.location}/Luck3DApp/mono" "%{cfg.targetdir}/mono\\" >NUL',
            'xcopy /D /E /I /Y /Q "%{wks.location}/Luck3DApp/Resources" "%{cfg.targetdir}/Resources\\" >NUL',
        }