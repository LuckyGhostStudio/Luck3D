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

-- Libs
Library = {}

-- Windows
Library["WinSock"] = "Ws2_32.lib"
Library["WinMM"] = "Winmm.lib"
Library["WinVersion"] = "Version.lib"
Library["BCrypt"] = "Bcrypt.lib"