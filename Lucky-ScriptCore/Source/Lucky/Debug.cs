namespace Lucky
{
    /// <summary>
    /// 日志：转发到引擎侧 spdlog
    /// </summary>
    public static class Debug
    {
        public static void Log(string message)
        {
            InternalCalls.Debug_Log(message);
        }

        public static void Warn(string message)
        {
            InternalCalls.Debug_Warn(message);
        }

        public static void Error(string message)
        {
            InternalCalls.Debug_Error(message);
        }
    }
}
