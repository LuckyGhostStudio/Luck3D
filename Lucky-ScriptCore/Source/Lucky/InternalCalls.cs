using System;
using System.Runtime.CompilerServices;

namespace Lucky
{
    /// <summary>
    /// Internal Call 声明表：C# 侧声明由 C++ 侧 ScriptGlue 注册的 native 方法
    /// 命名规范：目标类_操作，例如 TransformComponent_GetPosition
    /// </summary>
    public static class InternalCalls
    {
        // ---- Debug ----
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Debug_Log(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Debug_Warn(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Debug_Error(string message);

        // ---- Entity ----
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Entity_HasComponent(ulong entityID, Type componentType);

        // ---- TransformComponent ----
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetPosition(ulong entityID, out Vector3 outPosition);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_SetPosition(ulong entityID, ref Vector3 inPosition);
    }
}
