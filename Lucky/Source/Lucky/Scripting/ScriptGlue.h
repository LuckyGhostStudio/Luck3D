#pragma once

namespace Lucky
{
    /// <summary>
    /// 脚本桥：把 C# 侧 [MethodImpl(InternalCall)] 桩函数与 C++ native 实现挂接
    /// 由 ScriptEngine::Init 在加载完程序集后调用一次
    /// </summary>
    class ScriptGlue
    {
    public:
        /// <summary>
        /// 注册所有 InternalCall 函数到 Mono JIT
        /// 前置条件：InitMono 已完成
        /// </summary>
        static void RegisterFunctions();

        /// <summary>
        /// 注册托管组件类型 → C++ HasComponent 分发的映射表
        /// 前置条件：LoadCoreAssembly 已完成
        /// </summary>
        static void RegisterComponents();
    };
}
