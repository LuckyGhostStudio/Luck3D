#include "lcpch.h"
#include "ImGuiLayer.h"

// Temp
#include <GLFW/glfw3.h>

#include <imgui_internal.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "ImGuizmo.h"

#include "Lucky/Core/Application.h"
#include "Lucky/Editor/EditorPreferences.h"
#include "Lucky/UI/Theme.h"

namespace Lucky
{
    constexpr static float s_StandardDPI = 120.0f;  // 基准 DPI

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

        io.ConfigDragClickToInputText = true;  // 启用单次点击进入输入模式
        
        float fontSize = 20.0f * app.GetWindow().GetDPI() / s_StandardDPI;
        io.Fonts->AddFontFromFileTTF("Resources/Fonts/Opensans/OpenSans-Bold.ttf", fontSize);   // 添加粗体（0号）
        
        // 默认字体 添加 TTF 字体
        io.FontDefault = io.Fonts->AddFontFromFileTTF("Resources/Fonts/Opensans/OpenSans-Regular.ttf", fontSize);   // 1号

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SetDefaultStyles();     // 设置常规样式
        }
        
        // 加载偏好设置并应用颜色
        EditorPreferences::Get().Load();                // 尝试加载配置文件（失败则使用默认值）
        EditorPreferences::Get().ApplyImGuiColors();    // 应用颜色到 ImGui 主题

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
        ImGuizmo::BeginFrame();
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
        
        style.Alpha = UI::Theme::Layout::Alpha;                                             // 全局透明度
        style.DisabledAlpha = UI::Theme::Layout::DisabledAlpha;                             // 禁用状态的透明度
        style.WindowPadding.x = UI::Theme::Layout::WindowPaddingX;                          // 窗口内边距 X
        style.WindowPadding.y = UI::Theme::Layout::WindowPaddingY;                          // 窗口内边距 Y
        style.WindowRounding = UI::Theme::Layout::WindowRounding;                           // 窗口圆角半径
        style.WindowBorderSize = UI::Theme::Layout::WindowBorderSize;                       // 窗口边框大小
        style.WindowMinSize.x = UI::Theme::Layout::WindowMinSizeX;                          // 窗口最小尺寸 X
        style.WindowMinSize.y = UI::Theme::Layout::WindowMinSizeY;                          // 窗口最小尺寸 Y
        style.WindowTitleAlign.x = UI::Theme::Layout::WindowTitleAlignX;                    // 窗口标题对齐 X
        style.WindowTitleAlign.y = UI::Theme::Layout::WindowTitleAlignY;                    // 窗口标题对齐 Y
        style.WindowMenuButtonPosition = UI::Theme::Layout::WindowMenuButtonPosition;       // 窗口菜单按钮位置 (-1=左, 0=无, 1=右)
        style.ChildRounding = UI::Theme::Layout::ChildRounding;                             // 子窗口圆角半径
        style.ChildBorderSize = UI::Theme::Layout::ChildBorderSize;                         // 子窗口边框大小
        style.PopupRounding = UI::Theme::Layout::PopupRounding;                             // 弹出窗口圆角半径
        style.PopupBorderSize = UI::Theme::Layout::PopupBorderSize;                         // 弹出窗口边框大小
        style.FramePadding.x = UI::Theme::Layout::FramePaddingX;                            // 框架内边距 X
        style.FramePadding.y = UI::Theme::Layout::FramePaddingY;                            // 框架内边距 Y
        style.FrameRounding = UI::Theme::Layout::FrameRounding;                             // 框架圆角半径
        style.FrameBorderSize = UI::Theme::Layout::FrameBorderSize;                         // 框架边框大小
        style.ItemSpacing.x = UI::Theme::Layout::ItemSpacingX;                              // 项目间距 X
        style.ItemSpacing.y = UI::Theme::Layout::ItemSpacingY;                              // 项目间距 Y
        style.ItemInnerSpacing.x = UI::Theme::Layout::ItemInnerSpacingX;                    // 项目内间距 X
        style.ItemInnerSpacing.y = UI::Theme::Layout::ItemInnerSpacingY;                    // 项目内间距 Y
        style.CellPadding.x = UI::Theme::Layout::CellPaddingX;                              // 单元格内边距 X
        style.CellPadding.y = UI::Theme::Layout::CellPaddingY;                              // 单元格内边距 Y
        style.TouchExtraPadding.x = UI::Theme::Layout::TouchExtraPaddingX;                  // 触摸额外内边距 X
        style.TouchExtraPadding.y = UI::Theme::Layout::TouchExtraPaddingY;                  // 触摸额外内边距 Y
        style.IndentSpacing = UI::Theme::Layout::IndentSpacing;                             // 缩进间距
        style.ColumnsMinSpacing = UI::Theme::Layout::ColumnsMinSpacing;                     // 列最小间距
        style.ScrollbarSize = UI::Theme::Layout::ScrollbarSize;                             // 滚动条大小
        style.ScrollbarRounding = UI::Theme::Layout::ScrollbarRounding;                     // 滚动条圆角半径
        style.GrabMinSize = UI::Theme::Layout::GrabMinSize;                                 // 抓取最小大小
        style.GrabRounding = UI::Theme::Layout::GrabRounding;                               // 抓取圆角半径
        style.LogSliderDeadzone = UI::Theme::Layout::LogSliderDeadzone;                     // 对数滑块死区
        style.TabRounding = UI::Theme::Layout::TabRounding;                                 // 标签页圆角半径
        style.TabBorderSize =UI::Theme::Layout::TabBorderSize;                              // 标签页边框大小
        style.TabMinWidthForCloseButton = UI::Theme::Layout::TabMinWidthForCloseButton;     // 显示关闭按钮的标签页最小宽度
        style.ColorButtonPosition = UI::Theme::Layout::ColorButtonPosition;                 // 颜色按钮位置 (0=左, 1=右)
        style.ButtonTextAlign.x = UI::Theme::Layout::ButtonTextAlignX;                      // 按钮文本对齐 X
        style.ButtonTextAlign.y = UI::Theme::Layout::ButtonTextAlignY;                      // 按钮文本对齐 Y
        style.SelectableTextAlign.x = UI::Theme::Layout::SelectableTextAlignX;              // 可选文本对齐 X
        style.SelectableTextAlign.y = UI::Theme::Layout::SelectableTextAlignY;              // 可选文本对齐 Y
        style.DisplayWindowPadding.x = UI::Theme::Layout::DisplayWindowPaddingX;            // 显示窗口内边距 X
        style.DisplayWindowPadding.y = UI::Theme::Layout::DisplayWindowPaddingY;            // 显示窗口内边距 Y
        style.DisplaySafeAreaPadding.x = UI::Theme::Layout::DisplaySafeAreaPaddingX;        // 显示安全区内边距 X
        style.DisplaySafeAreaPadding.y = UI::Theme::Layout::DisplaySafeAreaPaddingY;        // 显示安全区内边距 Y
        style.MouseCursorScale = UI::Theme::Layout::MouseCursorScale;                       // 鼠标光标缩放
        style.AntiAliasedLines = UI::Theme::Layout::AntiAliasedLines;                       // 抗锯齿线条
        style.AntiAliasedLinesUseTex = UI::Theme::Layout::AntiAliasedLinesUseTex;           // 使用纹理抗锯齿线条
        style.AntiAliasedFill = UI::Theme::Layout::AntiAliasedFill;                         // 抗锯齿填充
        style.CurveTessellationTol = UI::Theme::Layout::CurveTessellationTol;               // 曲线细分容差
        style.CircleTessellationMaxError = UI::Theme::Layout::CircleTessellationMaxError;   // 圆细分最大误差
    }
}
