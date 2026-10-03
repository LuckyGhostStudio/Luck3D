#pragma once

#include "Lucky/Scripting/ScriptFieldValue.h"

#include "Lucky/Asset/Script.h"

#include "Lucky/Core/Base.h"

namespace Lucky
{
    /// <summary>
    /// 脚本组件：把用户 C# 脚本挂到实体上
    /// 保存脚本资产引用与该脚本各字段的取值；托管对象本身由 ScriptEngine 统一管理
    /// </summary>
    struct ScriptComponent
    {
        Ref<Script> ScriptAsset;

        ScriptFieldMap Fields;

        ScriptComponent() = default;
        ScriptComponent(const ScriptComponent& other) = default;
        ScriptComponent(const Ref<Script>& scriptAsset)
            : ScriptAsset(scriptAsset) {}
    };
}
