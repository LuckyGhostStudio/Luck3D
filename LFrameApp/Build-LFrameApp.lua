project "LFrameApp"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    targetdir "Binaries/%{cfg.buildcfg}"
    staticruntime "off"

    targetdir ("../Binaries/" .. outputdir .. "/%{prj.name}")
    objdir ("../Binaries/Intermediates/" .. outputdir .. "/%{prj.name}")

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
    }

    links
    {
        "Lucky"
    }

    filter "system:windows"
        systemversion "latest"
        defines { "WINDOWS" }

    filter "configurations:Debug"
        defines { "LF_DEBUG" }
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        defines { "LF_RELEASE" }
        runtime "Release"
        optimize "On"
        symbols "On"

    filter "configurations:Dist"
        defines { "LF_DIST" }
        runtime "Release"
        optimize "On"
        symbols "Off"