-- 反推仓库根：本 lua 文件位于 <repo>/Lucky-ScriptCore/Build-Lucky-ScriptCore.lua
local ThisFile = debug.getinfo(1, "S").source:sub(2)                            -- 去掉开头的 '@'
local ThisDir = ThisFile:match("(.*/)") or ThisFile:match("(.*\\)") or "./"     -- 当前脚本所在目录
local LuckyRepoRoot = string.gsub(path.getabsolute(ThisDir .. ".."), "\\", "/") -- 上一级即仓库根

project "Lucky-ScriptCore"
    kind "SharedLib"
    language "C#"
    dotnetframework "4.7.2"

    targetdir (LuckyRepoRoot .. "/Luck3DApp/Resources/Scripts")
    objdir (LuckyRepoRoot .. "/Luck3DApp/Resources/Scripts/Intermediates")

    files
    {
        "Source/**.cs",
        "Properties/**.cs"
    }

    filter "configurations:Debug"
        optimize "Off"
        symbols "Default"

    filter "configurations:Release"
        optimize "On"
        symbols "Default"

    filter "configurations:Dist"
        optimize "Full"
        symbols "Off"
