#include "lcpch.h"
#include "ScriptGlue.h"
#include "ScriptEngine.h"

#include "Lucky/Scene/Scene.h"
#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/TransformComponent.h"

#include <glm/glm.hpp>

#include <mono/jit/jit.h>
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

#include <format>
#include <functional>
#include <string_view>
#include <unordered_map>

namespace Lucky
{
    // glm::vec3 与 C# Lucky.Vector3 的内存布局必须一致（3 个连续 float）
    static_assert(sizeof(glm::vec3) == 3 * sizeof(float), "glm::vec3 layout mismatch with C# Lucky.Vector3");

    static std::unordered_map<MonoType*, std::function<bool(Entity)>> s_EntityHasComponentFuncs;

    // ======== Debug ========

    static void Debug_Log(MonoString* message)
    {
        if (!message)
        {
            LF_CORE_INFO("");
            return;
        }

        char* cstr = mono_string_to_utf8(message);
        std::string s(cstr);
        mono_free(cstr);
        LF_CORE_INFO("{}", s);
    }

    static void Debug_Warn(MonoString* message)
    {
        if (!message)
        {
            LF_CORE_WARN("");
            return;
        }

        char* cstr = mono_string_to_utf8(message);
        std::string s(cstr);
        mono_free(cstr);
        LF_CORE_WARN("{}", s);
    }

    static void Debug_Error(MonoString* message)
    {
        if (!message)
        {
            LF_CORE_ERROR("");
            return;
        }

        char* cstr = mono_string_to_utf8(message);
        std::string s(cstr);
        mono_free(cstr);
        LF_CORE_ERROR("{}", s);
    }

    // ======== Entity ========

    static bool Entity_HasComponent(UUID entityID, MonoReflectionType* componentType)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        if (!scene)
        {
            LF_CORE_WARN("ScriptGlue::Entity_HasComponent: no active scene context");
            return false;
        }

        Entity entity = scene->TryGetEntityWithUUID(entityID);
        if (!entity)
        {
            LF_CORE_WARN("ScriptGlue::Entity_HasComponent: entity {} not found", static_cast<uint64_t>(entityID));
            return false;
        }

        MonoType* managedType = mono_reflection_type_get_type(componentType);
        auto it = s_EntityHasComponentFuncs.find(managedType);
        if (it == s_EntityHasComponentFuncs.end())
        {
            LF_CORE_WARN("ScriptGlue::Entity_HasComponent: managed type not registered");
            return false;
        }

        return it->second(entity);
    }

    // ======== TransformComponent ========

    static void TransformComponent_GetPosition(UUID entityID, glm::vec3* outPosition)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        if (!scene)
        {
            LF_CORE_WARN("ScriptGlue::TransformComponent_GetPosition: no active scene context");
            *outPosition = glm::vec3(0.0f);
            return;
        }

        Entity entity = scene->TryGetEntityWithUUID(entityID);
        if (!entity || !entity.HasComponent<TransformComponent>())
        {
            LF_CORE_WARN("ScriptGlue::TransformComponent_GetPosition: entity {} invalid or no TransformComponent", static_cast<uint64_t>(entityID));
            *outPosition = glm::vec3(0.0f);
            return;
        }

        *outPosition = entity.GetComponent<TransformComponent>().Translation;
    }

    static void TransformComponent_SetPosition(UUID entityID, glm::vec3* inPosition)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        if (!scene)
        {
            LF_CORE_WARN("ScriptGlue::TransformComponent_SetPosition: no active scene context");
            return;
        }

        Entity entity = scene->TryGetEntityWithUUID(entityID);
        if (!entity || !entity.HasComponent<TransformComponent>())
        {
            LF_CORE_WARN("ScriptGlue::TransformComponent_SetPosition: entity {} invalid or no TransformComponent", static_cast<uint64_t>(entityID));
            return;
        }

        entity.GetComponent<TransformComponent>().Translation = *inPosition;
    }

    // ======== 组件类型分发注册 ========

    template<typename TComponent>
    static void RegisterComponent()
    {
        // MSVC 下 typeid(T).name() 形如 "struct Lucky::TransformComponent"，取 ':' 之后即为纯类名
        std::string_view typeName = typeid(TComponent).name();
        size_t pos = typeName.find_last_of(':');
        std::string_view structName = (pos == std::string_view::npos) ? typeName : typeName.substr(pos + 1);
        std::string managedTypeName = std::format("Lucky.{}", structName);

        MonoType* managedType = mono_reflection_type_from_name(managedTypeName.data(), ScriptEngine::GetCoreAssemblyImage());
        if (!managedType)
        {
            LF_CORE_ERROR("ScriptGlue::RegisterComponent: managed type '{}' not found in Core assembly", managedTypeName);
            return;
        }

        s_EntityHasComponentFuncs[managedType] = [](Entity e) { return e.HasComponent<TComponent>(); };
    }

    // ======== Registry ========

    void ScriptGlue::RegisterFunctions()
    {
#define LF_ADD_INTERNAL_CALL(Name) mono_add_internal_call("Lucky.InternalCalls::" #Name, (const void*)Name)

        LF_ADD_INTERNAL_CALL(Debug_Log);
        LF_ADD_INTERNAL_CALL(Debug_Warn);
        LF_ADD_INTERNAL_CALL(Debug_Error);

        LF_ADD_INTERNAL_CALL(Entity_HasComponent);

        LF_ADD_INTERNAL_CALL(TransformComponent_GetPosition);
        LF_ADD_INTERNAL_CALL(TransformComponent_SetPosition);

#undef LF_ADD_INTERNAL_CALL
    }

    void ScriptGlue::RegisterComponents()
    {
        s_EntityHasComponentFuncs.clear();
        RegisterComponent<TransformComponent>();
    }
}
