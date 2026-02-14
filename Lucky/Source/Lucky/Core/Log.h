#pragma once

#include <spdlog/spdlog.h>

#include "Base.h"

namespace Lucky
{
    /// <summary>
    /// 日志：用于输出和记录日志信息
    /// </summary>
    class Log
    {
    public:
        /// <summary>
        /// 初始化
        /// </summary>
        static void Init();

        /// <summary>
        /// 返回内核日志
        /// </summary>
        /// <returns>内核日志</returns>
        inline static Ref<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }

        /// <summary>
        /// 返回客户端日志
        /// </summary>
        /// <returns>客户端日志</returns>
        inline static Ref<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
    private:
        static Ref<spdlog::logger> s_CoreLogger;    // 内核日志
        static Ref<spdlog::logger> s_ClientLogger;  // 客户端日志
    };
}

// 内核日志
#define LF_CORE_TRACE(...)  ::Lucky::Log::GetCoreLogger()->trace(__VA_ARGS__)      // 提示：详细调试信息（函数调用、循环状态等）
#define LF_CORE_INFO(...)   ::Lucky::Log::GetCoreLogger()->info(__VA_ARGS__)       // 信息：程序状态或重要事件的日志信息（模块初始化完成等）
#define LF_CORE_WARN(...)   ::Lucky::Log::GetCoreLogger()->warn(__VA_ARGS__)       // 警告：可能导致问题的情况，但程序能继续运行（配置缺失等）
#define LF_CORE_ERROR(...)  ::Lucky::Log::GetCoreLogger()->error(__VA_ARGS__)      // 错误：可恢复错误或异常，但程序未崩溃（文件读取失败、网络超时等）
#define LF_CORE_FATAL(...)  ::Lucky::Log::GetCoreLogger()->critical(__VA_ARGS__)   // 严重错误：不可恢复错误，需终止程序（内存分配失败、关键资源初始化失败等）

// 客户端日志
#define LF_TRACE(...)       ::Lucky::Log::GetClientLogger()->trace(__VA_ARGS__)    // 提示：详细调试信息（函数调用、循环状态等）
#define LF_INFO(...)        ::Lucky::Log::GetClientLogger()->info(__VA_ARGS__)     // 信息：程序状态或重要事件的日志信息（模块初始化完成等）
#define LF_WARN(...)        ::Lucky::Log::GetClientLogger()->warn(__VA_ARGS__)     // 警告：可能导致问题的情况，但程序能继续运行（配置缺失等）
#define LF_ERROR(...)       ::Lucky::Log::GetClientLogger()->error(__VA_ARGS__)    // 错误：可恢复错误或异常，但程序未崩溃（文件读取失败、网络超时等）
#define LF_FATAL(...)       ::Lucky::Log::GetClientLogger()->critical(__VA_ARGS__) // 严重错误：不可恢复错误，需终止程序（内存分配失败、关键资源初始化失败等）