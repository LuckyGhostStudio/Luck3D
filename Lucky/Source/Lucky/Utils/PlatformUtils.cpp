#include "lcpch.h"
#include "Lucky/Utils/PlatformUtils.h"

#include <commdlg.h>
#include <shellapi.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "Lucky/Core/Application.h"

namespace Lucky
{
    std::string FileDialogs::OpenFile(const char* filter)
    {
        OPENFILENAMEA ofn;
        CHAR szFile[260] = { 0 };

        ZeroMemory(&ofn, sizeof(OPENFILENAME));

        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::GetInstance().GetWindow().GetNativeWindow());  // 对话框父窗口
        ofn.lpstrFile = szFile;         // 文件路径
        ofn.nMaxFile = sizeof(szFile);  // 最大文件大小
        ofn.lpstrFilter = filter;       // 文件过滤器
        ofn.nFilterIndex = 1;           // 默认过滤索引
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;    // 路径存在|文件存在|没有更改目录

        if (GetOpenFileNameA(&ofn) == TRUE) //打开文件名存在
        {
            return ofn.lpstrFile;       // 文件路径
        }

        return std::string();
    }

    std::string FileDialogs::SaveFile(const char* filter)
    {
        OPENFILENAMEA ofn;
        CHAR szFile[260] = { 0 };

        ZeroMemory(&ofn, sizeof(OPENFILENAME));

        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::GetInstance().GetWindow().GetNativeWindow());
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.lpstrDefExt = strchr(filter, '\0') + 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetSaveFileNameA(&ofn) == TRUE) // 保存文件名存在
        {
            return ofn.lpstrFile;
        }

        return std::string();
    }

    // ======== PlatformShell ========

    void PlatformShell::RevealInExplorer(const std::filesystem::path& path)
    {
#ifdef _WIN32
        if (!std::filesystem::exists(path))
        {
            LF_CORE_WARN("PlatformShell::RevealInExplorer - Path does not exist: {0}", path.generic_string());
            return;
        }

        std::wstring wPath = std::filesystem::absolute(path).wstring();

        if (std::filesystem::is_directory(path))
        {
            // 目录：直接打开该目录
            ShellExecuteW(nullptr, L"open", wPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        else
        {
            // 文件：打开父目录并选中该文件
            std::wstring param = L"/select,\"" + wPath + L"\"";
            ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
        }
#else
        LF_CORE_WARN("PlatformShell::RevealInExplorer - Not implemented on this platform.");
#endif
    }
}