#include "lcpch.h"
#include "ScriptEngine.h"
#include "ScriptGlue.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/object.h>
#include <mono/metadata/tabledefs.h>

#include <fstream>

namespace Lucky
{
    /// <summary>
    /// 脚本引擎运行时数据
    /// </summary>
    struct ScriptEngineData
    {
        MonoDomain* RootDomain = nullptr;
        MonoDomain* AppDomain = nullptr;

        MonoAssembly* CoreAssembly = nullptr;
        MonoImage* CoreAssemblyImage = nullptr;

        MonoAssembly* AppAssembly = nullptr;
        MonoImage* AppAssemblyImage = nullptr;

        Ref<ScriptClass> EntityBaseClass;

        std::unordered_map<std::string, Ref<ScriptClass>> EntityClasses;
        std::unordered_map<UUID, Ref<ScriptInstance>> EntityInstances;

        Scene* SceneContext = nullptr;
    };

    static ScriptEngineData* s_Data = nullptr;

    namespace
    {
        /// <summary>
        /// 读取整个文件到 char*，调用方负责 delete[]
        /// </summary>
        char* ReadBytes(const std::filesystem::path& filepath, uint32_t* outSize)
        {
            std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
            if (!stream)
            {
                return nullptr;
            }

            std::streampos end = stream.tellg();
            stream.seekg(0, std::ios::beg);
            uint64_t size = end - stream.tellg();

            if (size == 0)
            {
                return nullptr;
            }

            char* buffer = new char[size];
            stream.read(buffer, size);
            stream.close();

            *outSize = static_cast<uint32_t>(size);
            return buffer;
        }

        /// <summary>
        /// 从磁盘加载 Mono 程序集（走内存 buffer，避免锁定文件）
        /// </summary>
        MonoAssembly* LoadMonoAssembly(const std::filesystem::path& assemblyPath)
        {
            uint32_t fileSize = 0;
            char* fileData = ReadBytes(assemblyPath, &fileSize);
            if (!fileData)
            {
                LF_CORE_ERROR("ScriptEngine: failed to read assembly '{}'", assemblyPath.string());
                return nullptr;
            }

            MonoImageOpenStatus status;
            MonoImage* image = mono_image_open_from_data_full(fileData, fileSize, 1, &status, 0);

            if (status != MONO_IMAGE_OK)
            {
                const char* errorMessage = mono_image_strerror(status);
                LF_CORE_ERROR("ScriptEngine: failed to open image '{}': {}", assemblyPath.string(), errorMessage);
                delete[] fileData;
                return nullptr;
            }

            std::string pathStr = assemblyPath.string();
            MonoAssembly* assembly = mono_assembly_load_from_full(image, pathStr.c_str(), &status, 0);

            mono_image_close(image);
            delete[] fileData;

            return assembly;
        }
    }

    // ======== ScriptEngine ========

    void ScriptEngine::Init()
    {
        s_Data = new ScriptEngineData();

        InitMono();

        LoadCoreAssembly("Resources/Scripts/Lucky-ScriptCore.dll");
        LoadAppAssembly("Assets/Scripts/Binaries/App.dll");
        LoadAssemblyClasses();

        s_Data->EntityBaseClass = CreateRef<ScriptClass>("Lucky", "Entity", true);

        ScriptGlue::RegisterFunctions();
        ScriptGlue::RegisterComponents();

        LF_CORE_INFO("ScriptEngine: initialized ({} user script classes loaded)", s_Data->EntityClasses.size());
    }

    void ScriptEngine::Shutdown()
    {
        ShutdownMono();
        delete s_Data;
        s_Data = nullptr;
    }

    void ScriptEngine::InitMono()
    {
        mono_set_assemblies_path("mono/lib");

        MonoDomain* rootDomain = mono_jit_init("LuckyJITRuntime");
        LF_CORE_ASSERT(rootDomain, "ScriptEngine: mono_jit_init failed");

        s_Data->RootDomain = rootDomain;
    }

    void ScriptEngine::ShutdownMono()
    {
        s_Data->AppDomain = nullptr;
        s_Data->RootDomain = nullptr;
    }

    void ScriptEngine::LoadCoreAssembly(const std::filesystem::path& filepath)
    {
        char domainName[] = "LuckyScriptRuntime";
        s_Data->AppDomain = mono_domain_create_appdomain(domainName, nullptr);
        mono_domain_set(s_Data->AppDomain, true);

        s_Data->CoreAssembly = LoadMonoAssembly(filepath);
        LF_CORE_ASSERT(s_Data->CoreAssembly, "ScriptEngine: core assembly is required");

        s_Data->CoreAssemblyImage = mono_assembly_get_image(s_Data->CoreAssembly);
    }

    void ScriptEngine::LoadAppAssembly(const std::filesystem::path& filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            LF_CORE_WARN("ScriptEngine: app assembly not found '{}'", filepath.string());
            return;
        }

        s_Data->AppAssembly = LoadMonoAssembly(filepath);
        if (!s_Data->AppAssembly)
        {
            return;
        }

        s_Data->AppAssemblyImage = mono_assembly_get_image(s_Data->AppAssembly);
    }

    void ScriptEngine::OnRuntimeStart(Scene* scene)
    {
        s_Data->SceneContext = scene;
    }

    void ScriptEngine::OnRuntimeStop()
    {
        s_Data->SceneContext = nullptr;
        s_Data->EntityInstances.clear();
    }

    bool ScriptEngine::EntityScriptClassExists(const std::string& fullClassName)
    {
        return s_Data->EntityClasses.find(fullClassName) != s_Data->EntityClasses.end();
    }

    void ScriptEngine::OnCreateEntityScript(Entity entity, const std::string& fullClassName)
    {
        auto it = s_Data->EntityClasses.find(fullClassName);
        if (it == s_Data->EntityClasses.end())
        {
            LF_CORE_WARN("ScriptEngine: entity script class '{}' not found", fullClassName);
            return;
        }

        Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(it->second, entity);
        s_Data->EntityInstances[entity.GetUUID()] = instance;

        instance->InvokeAwake();
    }

    void ScriptEngine::OnUpdateEntityScript(Entity entity, DeltaTime dt)
    {
        UUID id = entity.GetUUID();
        auto it = s_Data->EntityInstances.find(id);
        if (it == s_Data->EntityInstances.end())
        {
            return;
        }

        it->second->InvokeUpdate(static_cast<float>(dt));
    }

    Scene* ScriptEngine::GetSceneContext()
    {
        return s_Data->SceneContext;
    }

    MonoImage* ScriptEngine::GetCoreAssemblyImage()
    {
        return s_Data->CoreAssemblyImage;
    }

    const std::unordered_map<std::string, Ref<ScriptClass>>& ScriptEngine::GetEntityClasses()
    {
        return s_Data->EntityClasses;
    }

    void ScriptEngine::LoadAssemblyClasses()
    {
        s_Data->EntityClasses.clear();

        if (!s_Data->AppAssemblyImage)
        {
            return;
        }

        const MonoTableInfo* typeTable = mono_image_get_table_info(s_Data->AppAssemblyImage, MONO_TABLE_TYPEDEF);
        int32_t numTypes = mono_table_info_get_rows(typeTable);

        MonoClass* entityBaseClass = mono_class_from_name(s_Data->CoreAssemblyImage, "Lucky", "Entity");

        for (int32_t i = 0; i < numTypes; i++)
        {
            uint32_t cols[MONO_TYPEDEF_SIZE];
            mono_metadata_decode_row(typeTable, i, cols, MONO_TYPEDEF_SIZE);

            const char* nameSpace = mono_metadata_string_heap(s_Data->AppAssemblyImage, cols[MONO_TYPEDEF_NAMESPACE]);
            const char* className = mono_metadata_string_heap(s_Data->AppAssemblyImage, cols[MONO_TYPEDEF_NAME]);

            std::string fullName;
            if (strlen(nameSpace) != 0)
            {
                fullName = std::string(nameSpace) + "." + className;
            }
            else
            {
                fullName = className;
            }

            MonoClass* monoClass = mono_class_from_name(s_Data->AppAssemblyImage, nameSpace, className);

            if (monoClass == entityBaseClass)
            {
                continue;
            }

            if (!mono_class_is_subclass_of(monoClass, entityBaseClass, false))
            {
                continue;
            }

            Ref<ScriptClass> scriptClass = CreateRef<ScriptClass>(nameSpace, className, false);
            s_Data->EntityClasses[fullName] = scriptClass;

            LF_CORE_TRACE("ScriptEngine: found user script class '{}'", fullName);
        }
    }

    MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass)
    {
        MonoObject* instance = mono_object_new(s_Data->AppDomain, monoClass);
        mono_runtime_object_init(instance);
        return instance;
    }

    // ======== ScriptClass ========

    ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore)
        : m_ClassNamespace(classNamespace), m_ClassName(className)
    {
        MonoImage* image = isCore ? s_Data->CoreAssemblyImage : s_Data->AppAssemblyImage;
        m_MonoClass = mono_class_from_name(image, classNamespace.c_str(), className.c_str());
    }

    MonoObject* ScriptClass::Instantiate()
    {
        return ScriptEngine::InstantiateClass(m_MonoClass);
    }

    MonoMethod* ScriptClass::GetMethod(const std::string& name, int parameterCount)
    {
        return mono_class_get_method_from_name(m_MonoClass, name.c_str(), parameterCount);
    }

    MonoObject* ScriptClass::InvokeMethod(MonoObject* instance, MonoMethod* method, void** params)
    {
        return mono_runtime_invoke(method, instance, params, nullptr);
    }

    // ======== ScriptInstance ========

    ScriptInstance::ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity)
        : m_ScriptClass(scriptClass)
    {
        m_Instance = scriptClass->Instantiate();

        m_Constructor = s_Data->EntityBaseClass->GetMethod(".ctor", 1);
        m_AwakeMethod = scriptClass->GetMethod("Awake", 0);
        m_UpdateMethod = scriptClass->GetMethod("Update", 1);

        UUID id = entity.GetUUID();
        void* param = &id;
        m_ScriptClass->InvokeMethod(m_Instance, m_Constructor, &param);
    }

    void ScriptInstance::InvokeAwake()
    {
        if (m_AwakeMethod)
        {
            m_ScriptClass->InvokeMethod(m_Instance, m_AwakeMethod);
        }
    }

    void ScriptInstance::InvokeUpdate(float dt)
    {
        if (m_UpdateMethod)
        {
            void* param = &dt;
            m_ScriptClass->InvokeMethod(m_Instance, m_UpdateMethod, &param);
        }
    }
}
