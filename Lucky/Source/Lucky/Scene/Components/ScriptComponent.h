#pragma once

#include <string>

namespace Lucky
{
    /// <summary>
    /// 脚本组件：把用户 C# 脚本类挂到实体上
    /// 只保存类的全名（Namespace.ClassName），实际的托管对象由 ScriptEngine 统一管理
    /// </summary>
    struct ScriptComponent
    {
        std::string ClassName;

        ScriptComponent() = default;
        ScriptComponent(const ScriptComponent& other) = default;
        ScriptComponent(const std::string& className)
            : ClassName(className) {}
    };
}
