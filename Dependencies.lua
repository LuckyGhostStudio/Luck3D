-- 包含目录
IncludeDir = {}
IncludeDir["GLFW"] = "%{wks.location}/Lucky/Vendor/GLFW/include"
IncludeDir["GLAD"] = "%{wks.location}/Lucky/Vendor/GLAD/include"
IncludeDir["ImGui"] = "%{wks.location}/Lucky/Vendor/imgui"
IncludeDir["glm"] = "%{wks.location}/Lucky/Vendor/glm"
IncludeDir["stb_image"] = "%{wks.location}/Lucky/Vendor/stb_image"
IncludeDir["entt"] = "%{wks.location}/Lucky/Vendor/entt/include"
IncludeDir["yaml_cpp"] = "%{wks.location}/Lucky/Vendor/yaml-cpp/include"
IncludeDir["ImGuizmo"] = "%{wks.location}/Lucky/Vendor/ImGuizmo"
IncludeDir["assimp"] = "%{wks.location}/Lucky/Vendor/assimp/include"

-- 库目录
LibraryDir = {}
LibraryDir["assimp_Debug"] = "%{wks.location}/Lucky/Vendor/assimp/bin/windows/Debug"
LibraryDir["assimp_Release"] = "%{wks.location}/Lucky/Vendor/assimp/bin/windows/Release"

-- Libs
Library = {}
Library["assimp_Debug"] = "assimp-vc143-mtd"
Library["assimp_Release"] = "assimp-vc143-mt"

-- Windows
Library["WinSock"] = "Ws2_32.lib"
Library["WinMM"] = "Winmm.lib"
Library["WinVersion"] = "Version.lib"
Library["BCrypt"] = "Bcrypt.lib"