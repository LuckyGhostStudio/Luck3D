#pragma once

#include "Lucky/Asset/Script.h"

#include "Lucky/Core/Base.h"

namespace Lucky
{
    /// <summary>
    /// 脚本组件：把用户 C# 脚本挂到实体上
    /// 只保存脚本资产引用，实际的托管对象由 ScriptEngine 统一管理
    /// </summary>
    struct ScriptComponent
    {
        Ref<Script> ScriptAsset;

        ScriptComponent() = default;
        ScriptComponent(const ScriptComponent& other) = default;
        ScriptComponent(const Ref<Script>& scriptAsset)
            : ScriptAsset(scriptAsset) {}
    };
}
