#include "lpch.h"
#include "ImGuiLayer.h"

// Temp
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui_internal.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "Lucky/Core/Application.h"

namespace Lucky
{
    const static float s_StandardDPI = 120.0f;  // 基准 DPI

    ImGuiLayer::ImGuiLayer()
        : Layer("ImGuiLayer")
    {

    }

    ImGuiLayer::~ImGuiLayer()
    {

    }

    void ImGuiLayer::OnAttach()
    {
        LF_CORE_TRACE("ImGuiLayer::OnAttach");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();     // 创建 ImGui 上下文

        Application& app = Application::GetInstance();

        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;     // Enable Multi-Viewport / Platform Windows

        float fontSize = 20.0f * app.GetWindow().GetDPI() / s_StandardDPI;
        io.Fonts->AddFontFromFileTTF("Resources/Fonts/Opensans/OpenSans-Bold.ttf", fontSize);   // 添加粗体（0号）
        
        // 默认字体 添加 TTF 字体
        io.FontDefault = io.Fonts->AddFontFromFileTTF("Resources/Fonts/Opensans/OpenSans-Regular.ttf", fontSize);   // 1号

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SetDefaultStyles();     // 设置常规样式
        }

        GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 410");
    }

    void ImGuiLayer::OnDetach()
    {
        LF_CORE_TRACE("ImGuiLayer::OnDetach");

        ImGui_ImplOpenGL3_Shutdown();   // 关闭 ImGui 
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();        // 销毁上下文
    }

    void ImGuiLayer::OnEvent(Event& event)
    {
        // 阻止接收事件
        if (m_BlockEvents)
        {
            ImGuiIO& io = ImGui::GetIO();

            //event.m_Handled |= event.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;        // 捕获鼠标事件
            //event.m_Handled |= event.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;  // 捕获键盘事件
        }
    }

    void ImGuiLayer::Begin()
    {
        // 开启新帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::End()
    {
        ImGuiIO& io = ImGui::GetIO();
        Application& app = Application::GetInstance();
        io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight()); // 设置窗口大小

        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }

    void ImGuiLayer::SetDefaultStyles()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        // TODO 设置 ImGuiStyle 的所有参数
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.TabBorderSize = 1.0f;

        style.WindowMinSize.x = 50.0f;          // 窗口最小尺寸

        style.FrameRounding = 4.0f;             // 控件边框圆度 [0, 12] 4.8 <=> 0.4f
        style.FrameBorderSize = 1.0f;           // 边框尺寸
        style.FramePadding.y = 4.0f;
 
        style.WindowRounding = 4.0f;            // 窗口边框圆度
        style.GrabRounding = 4.0f;              // 拖动条 handle 圆度
        style.PopupRounding = 4.0f;             // 弹出窗口圆度
        style.ChildRounding = 4.0f;             // 子窗口圆度
        style.TabRounding = 2.0f;               // Tab 圆度

        style.ScrollbarRounding = 12.0f;        // 滚动条圆度
        style.ScrollbarSize = 18.0f;

        style.ButtonTextAlign = { 0.5f, 0.5f }; // 按钮文字居中
    }
}