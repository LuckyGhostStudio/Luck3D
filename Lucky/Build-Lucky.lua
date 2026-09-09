project "Lucky"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("%{wks.location}/Binaries/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/Binaries/Intermediates/" .. outputdir .. "/%{prj.name}")

    pchheader "lcpch.h"
    pchsource "Source/lcpch.cpp"

    files
    {
        "Source/**.h",
        "Source/**.cpp",
        "Vendor/stb_image/**.h",
        "Vendor/stb_image/**.cpp",
        "Vendor/glm/glm/**.hpp",
        "Vendor/glm/glm/**.inl",
        "Vendor/ImGuizmo/ImGuizmo.h",
        "Vendor/ImGuizmo/ImGuizmo.cpp"
    }

    includedirs
    {
        "Source",
        "Vendor",
        "Vendor/spdlog/include",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.GLAD}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.stb_image}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.yaml_cpp}",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.assimp}",
        "%{IncludeDir.mono}",
    }

    links
    {
        "GLFW",
        "GLAD",
        "ImGui",
        "yaml-cpp",
        "opengl32.lib",
        "%{Library.mono}",
    }

    filter "files:Vendor/ImGuizmo/**.cpp"
    flags {"NoPCH"}      -- 该 cpp 文件不使用预编译头文件

    filter "system:windows"
        systemversion "latest"

        defines
        {
            "GLFW_INCLUDE_NONE"
        }

        links
        {
            "%{Library.WinSock}",
            "%{Library.WinMM}",
            "%{Library.WinVersion}",
            "%{Library.BCrypt}",
        }

    filter "configurations:Debug"
        libdirs { "%{LibraryDir.assimp_Debug}" }
        links { "%{Library.assimp_Debug}" }

        defines { "LF_DEBUG" }
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        libdirs { "%{LibraryDir.assimp_Release}" }
        links { "%{Library.assimp_Release}" }

        defines { "LF_RELEASE" }
        runtime "Release"
        optimize "On"
        symbols "On"

    filter "configurations:Dist"
        libdirs { "%{LibraryDir.assimp_Release}" }
        links { "%{Library.assimp_Release}" }

        defines { "LF_DIST" }
        runtime "Release"
        optimize "On"
        symbols "Off"