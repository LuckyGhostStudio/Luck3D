#include "lcpch.h"
#include "FileSystem.h"

#ifdef _WIN32
    #include <Windows.h>
#endif

namespace Lucky
{
    const std::filesystem::path& FileSystem::GetEditorExecutableDirectory()
    {
        static std::filesystem::path s_ExeDir = []
        {
#ifdef _WIN32
            wchar_t buffer[MAX_PATH] = { 0 };
            DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            LF_CORE_ASSERT(length > 0 && length < MAX_PATH, "GetModuleFileNameW failed");
            return std::filesystem::path(buffer).parent_path().lexically_normal();
#else
            LF_CORE_ASSERT(false, "GetEditorExecutableDirectory not implemented for this platform");
            return std::filesystem::current_path();
#endif
        }();

        return s_ExeDir;
    }
}
