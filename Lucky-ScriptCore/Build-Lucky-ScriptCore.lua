project "Lucky-ScriptCore"
    kind "SharedLib"
    language "C#"
    dotnetframework "4.7.2"

    targetdir ("%{wks.location}/Luck3DApp/Resources/Scripts")
    objdir ("%{wks.location}/Luck3DApp/Resources/Scripts/Intermediates")

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
