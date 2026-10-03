#include "lcpch.h"
#include "ScriptEngine.h"
#include "ScriptGlue.h"

#include "Lucky/Core/FileSystem.h"
#include "Lucky/Project/Project.h"

#include "Lucky/Asset/AssetManager.h"
#include "Lucky/Asset/Script.h"
#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/Mesh.h"
#include "Lucky/Renderer/Texture.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/class.h>
#include <mono/metadata/object.h>
#include <mono/metadata/tabledefs.h>

#include <cstring>
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

        /// <summary>
        /// 取托管异常的字符串属性（Message / StackTrace），取不到返回空串
        /// </summary>
        std::string GetExceptionStringProperty(MonoObject* exception, const char* propertyName)
        {
            MonoClass* exceptionClass = mono_object_get_class(exception);
            MonoProperty* property = mono_class_get_property_from_name(exceptionClass, propertyName);
            if (!property)
            {
                return {};
            }

            MonoMethod* getter = mono_property_get_get_method(property);
            if (!getter)
            {
                return {};
            }

            MonoObject* getterException = nullptr;
            MonoString* value = (MonoString*)mono_runtime_invoke(getter, exception, nullptr, &getterException);
            if (getterException || !value)
            {
                return {};
            }

            char* cstr = mono_string_to_utf8(value);
            if (!cstr)
            {
                return {};
            }

            std::string result(cstr);
            mono_free(cstr);
            return result;
        }

        /// <summary>
        /// 把托管异常打到日志：类型名 + Message + StackTrace，自身不抛异常
        /// </summary>
        void LogScriptException(MonoObject* exception)
        {
            MonoClass* exceptionClass = mono_object_get_class(exception);

            const char* nameSpace = mono_class_get_namespace(exceptionClass);
            const char* className = mono_class_get_name(exceptionClass);
            std::string typeName = (nameSpace && strlen(nameSpace) > 0)
                ? std::string(nameSpace) + "." + className
                : std::string(className);

            std::string message = GetExceptionStringProperty(exception, "Message");
            std::string stackTrace = GetExceptionStringProperty(exception, "StackTrace");

            // 反射取不到属性时退回 ToString，至少保留类型与消息
            if (message.empty() && stackTrace.empty())
            {
                MonoObject* toStringException = nullptr;
                MonoString* text = mono_object_to_string(exception, &toStringException);
                if (!toStringException && text)
                {
                    char* cstr = mono_string_to_utf8(text);
                    if (cstr)
                    {
                        message = cstr;
                        mono_free(cstr);
                    }
                }
            }

            LF_CORE_ERROR("Script exception: {}: {}", typeName, message);

            if (!stackTrace.empty())
            {
                LF_CORE_ERROR("{}", stackTrace);
            }
        }

        /// <summary>
        /// 把 mono 字段类型解析为本引擎支持的 ScriptFieldType；不支持时返回 None
        /// 数组的托管全名带 "[]" 后缀，匹配不上即自动跳过，无需特判
        /// </summary>
        ScriptFieldType ResolveScriptFieldType(MonoType* fieldType)
        {
            if (!fieldType)
            {
                return ScriptFieldType::None;
            }

            // mono_type_get_name 返回堆上新分配的字符串，调用方必须 mono_free
            char* managedName = mono_type_get_name(fieldType);

            ScriptFieldType fieldTypeResult = ScriptFieldType::None;
            const bool found = TryGetScriptFieldTypeByManagedName(managedName, fieldTypeResult);
            mono_free(managedName);

            return found ? fieldTypeResult : ScriptFieldType::None;
        }

        /// <summary>
        /// 字段类型是否为 Lucky.Entity 的派生类（脚本互引，如 public PlayerController Other;）
        /// 这类字段按 ScriptFieldType::Entity 处理
        /// </summary>
        bool IsEntityDerivedFieldType(MonoType* fieldType)
        {
            if (!fieldType || mono_type_get_type(fieldType) != MONO_TYPE_CLASS)
            {
                return false;
            }

            MonoClass* fieldClass = mono_class_from_mono_type(fieldType);
            MonoClass* entityClass = mono_class_from_name(s_Data->CoreAssemblyImage, "Lucky", "Entity");
            if (!fieldClass || !entityClass || fieldClass == entityClass)
            {
                return false;
            }

            return mono_class_is_subclass_of(fieldClass, entityClass, false);
        }

        /// <summary>
        /// 读取托管引用对象里的 ulong 句柄字段（Entity 用 "ID"，资产包装类型用 "Handle"）
        /// </summary>
        /// <returns>是否读取成功</returns>
        bool TryGetReferenceHandle(MonoObject* referenceObject, const char* handleFieldName, uint64_t& outHandle)
        {
            outHandle = 0;
            if (!referenceObject)
            {
                return false;
            }

            // mono_class_get_field_from_name 会沿继承链搜索，Entity 派生类实例也能找到基类的 ID
            MonoClass* referenceClass = mono_object_get_class(referenceObject);
            MonoClassField* handleField = mono_class_get_field_from_name(referenceClass, handleFieldName);
            if (!handleField)
            {
                return false;
            }

            mono_field_get_value(referenceObject, handleField, &outHandle);
            return true;
        }

        /// <summary>
        /// 把 ulong 句柄写进托管引用对象（对象不存在时按字段声明类型新建一个）
        /// </summary>
        /// <returns>是否写入成功</returns>
        bool SetReferenceHandle(MonoObject* ownerInstance, MonoClassField* ownerField, const char* handleFieldName, uint64_t handle)
        {
            if (!ownerInstance || !ownerField)
            {
                return false;
            }

            MonoClass* referenceClass = mono_class_from_mono_type(mono_field_get_type(ownerField));
            MonoClassField* handleField = mono_class_get_field_from_name(referenceClass, handleFieldName);
            if (!handleField)
            {
                return false;
            }

            MonoObject* referenceObject = nullptr;
            mono_field_get_value(ownerInstance, ownerField, &referenceObject);
            if (!referenceObject)
            {
                referenceObject = mono_object_new(mono_domain_get(), referenceClass);
            }

            // handleField 是 ulong（值类型）：传值的地址
            mono_field_set_value(referenceObject, handleField, &handle);

            // ownerField 是引用类型：mono_field_set_value 直接收对象指针（语义不对称，值类型才传 &值）
            // 传 &referenceObject 会把栈地址写进字段 -> 野指针
            mono_field_set_value(ownerInstance, ownerField, referenceObject);
            return true;
        }

        /// <summary>
        /// 读取值类型字段：按字节块直接拷贝到 T 再装进 outValue（布局契约见 Phase1.5 决策点 9）
        /// </summary>
        template <typename T>
        bool GetScalarField(MonoObject* instance, MonoClassField* field, ScriptFieldValue& outValue)
        {
            T rawValue{};
            mono_field_get_value(instance, field, &rawValue);
            outValue.Data = rawValue;
            return true;
        }

        /// <summary>
        /// 写入值类型字段：取 variant 载荷后按地址交给 mono_field_set_value
        /// </summary>
        template <typename T>
        bool SetScalarField(MonoObject* instance, MonoClassField* field, const ScriptFieldValue& value)
        {
            T rawValue = std::get<T>(value.Data);
            mono_field_set_value(instance, field, &rawValue);
            return true;
        }
    }

    // ======== ScriptEngine ========

    void ScriptEngine::Init()
    {
        s_Data = new ScriptEngineData();

        InitMono();

        const std::filesystem::path exeDir = FileSystem::GetEditorExecutableDirectory();
        LoadCoreAssembly(exeDir / "Resources" / "Scripts" / "Lucky-ScriptCore.dll");

        // 用户 C# 程序集：归属项目资产
        if (const Ref<Project>& project = Project::GetActive())
        {
            LoadAppAssembly(project->GetScriptModulePath());
        }

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
        // Mono 库目录跟随 exe 分发
        const std::filesystem::path monoLibDir = FileSystem::GetEditorExecutableDirectory() / "mono" / "lib";
        mono_set_assemblies_path(monoLibDir.string().c_str());

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
        // OnDestroy 期间脚本仍可访问场景上下文，故先调用再清空
        for (auto& [entityID, instance] : s_Data->EntityInstances)
        {
            instance->InvokeDestroy();
        }

        s_Data->SceneContext = nullptr;
        s_Data->EntityInstances.clear();
    }

    bool ScriptEngine::EntityScriptClassExists(const std::string& fullClassName)
    {
        return s_Data->EntityClasses.find(fullClassName) != s_Data->EntityClasses.end();
    }

    Ref<ScriptClass> ScriptEngine::ResolveScriptClass(const std::string& className)
    {
        return ResolveScriptClassImpl(className, true);
    }

    Ref<ScriptClass> ScriptEngine::TryResolveScriptClass(const std::string& className)
    {
        return ResolveScriptClassImpl(className, false);
    }

    Ref<ScriptClass> ScriptEngine::ResolveScriptClassImpl(const std::string& className, bool logDiagnostics)
    {
        if (className.empty())
        {
            return nullptr;
        }

        Ref<ScriptClass> matchedClass = nullptr;
        bool hasMultipleMatches = false;
        std::string matchedFullNames;

        for (const auto& [fullName, scriptClass] : s_Data->EntityClasses)
        {
            if (scriptClass->GetName() != className)
            {
                continue;
            }

            if (matchedClass)
            {
                // 已经命中过一个：记为冲突，继续收集候选名用于日志
                hasMultipleMatches = true;
                matchedFullNames += ", " + fullName;
                continue;
            }

            matchedClass = scriptClass;
            matchedFullNames = fullName;
        }

        if (hasMultipleMatches)
        {
            if (logDiagnostics)
            {
                LF_CORE_ERROR("ScriptEngine::ResolveScriptClass - Class name '{0}' matches multiple script classes: {1}. Please rename the file or adjust its namespace to make it unique.", className, matchedFullNames);
            }
            return nullptr;
        }

        if (!matchedClass && logDiagnostics)
        {
            LF_CORE_ERROR("ScriptEngine::ResolveScriptClass - Script class '{0}' not found. Please make sure the script has been compiled and its class name matches the file name.", className);
        }

        return matchedClass;
    }

    void ScriptEngine::SyncScriptFieldMap(const Ref<Script>& scriptAsset, ScriptFieldMap& fieldMap)
    {
        if (!scriptAsset)
        {
            fieldMap.clear();
            return;
        }

        Ref<ScriptClass> scriptClass = ResolveScriptClass(scriptAsset->GetClassName());
        if (!scriptClass)
        {
            // ResolveScriptClass 已经报过错；保留 fieldMap 现有内容，避免用户在脚本没编译时丢掉已调的值
            return;
        }

        const std::vector<ScriptField>& fields = scriptClass->GetFields();

        // ---- 第一遍：缺失字段补默认值，类型不符的用默认值覆盖 ----
        for (const ScriptField& field : fields)
        {
            auto it = fieldMap.find(field.Name);
            if (it == fieldMap.end())
            {
                fieldMap[field.Name] = field.DefaultValue;
                continue;
            }

            if (it->second.Type != field.Type)
            {
                LF_CORE_WARN("ScriptEngine::SyncScriptFieldMap - Type of field '{0}.{1}' changed from the saved type to the script type, value reset to the script default", scriptClass->GetName(), field.Name);
                it->second = field.DefaultValue;
            }
        }

        // ---- 第二遍：清掉脚本里已经不存在的字段 ----
        for (auto it = fieldMap.begin(); it != fieldMap.end();)
        {
            const bool stillExists = std::any_of(fields.begin(), fields.end(),
                [&it](const ScriptField& field) { return field.Name == it->first; });

            if (stillExists)
            {
                ++it;
            }
            else
            {
                LF_CORE_WARN("ScriptEngine::SyncScriptFieldMap - Field '{0}' no longer exists in the script, stored value removed", it->first);
                it = fieldMap.erase(it);
            }
        }
    }

    void ScriptEngine::OnCreateEntityScript(Entity entity, const Ref<ScriptClass>& scriptClass)
    {
        LF_CORE_ASSERT(scriptClass, "ScriptEngine::OnCreateEntityScript - scriptClass must not be null");

        Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(scriptClass, entity);

        // 灌值必须在 Awake 之前：脚本的 Awake 通常会直接读取字段
        if (entity.HasComponent<ScriptComponent>())
        {
            instance->SetFieldValues(entity.GetComponent<ScriptComponent>().Fields);
        }

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

    void ScriptEngine::OnDestroyEntityScript(Entity entity)
    {
        if (!entity)
        {
            return;
        }

        auto it = s_Data->EntityInstances.find(entity.GetUUID());
        if (it == s_Data->EntityInstances.end())
        {
            return;
        }

        it->second->InvokeDestroy();
        s_Data->EntityInstances.erase(it);
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

    Ref<Asset> ScriptEngine::ResolveAssetByFieldType(ScriptFieldType type, AssetHandle handle)
    {
        if (!handle.IsValid())
        {
            return nullptr;
        }

        switch (type)
        {
            case ScriptFieldType::Material:
            {
                return AssetManager::GetAsset<Material>(handle);
            }
            case ScriptFieldType::Mesh:
            {
                return AssetManager::GetAsset<Mesh>(handle);
            }
            case ScriptFieldType::Texture2D:
            {
                return AssetManager::GetAsset<Texture2D>(handle);
            }
            case ScriptFieldType::Script:
            {
                return AssetManager::GetAsset<Script>(handle);
            }
            default:
            {
                return nullptr;
            }
        }
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
        MonoObject* exception = nullptr;
        MonoObject* result = mono_runtime_invoke(method, instance, params, &exception);

        if (exception)
        {
            LogScriptException(exception);
        }

        return result;
    }

    const std::vector<ScriptField>& ScriptClass::GetFields()
    {
        if (m_FieldsInitialized)
        {
            return m_Fields;
        }
        m_FieldsInitialized = true;
        m_Fields.clear();

        if (!m_MonoClass)
        {
            return m_Fields;
        }

        // 默认值不是元数据（它被编译进构造函数体），必须在一个实例上读
        // 这里造一个临时实例，读完即丢弃；Awake 不会被调用，只有构造函数会跑
        MonoObject* defaultInstance = Instantiate();

        // mono_class_get_fields 只枚举本类声明的字段、不含父类 -> 沿继承链逐级枚举
        // 到 System.Object 自然结束；Lucky.Entity 的 ID 会被 INIT_ONLY 过滤自然挡掉，无需特判
        for (MonoClass* currentClass = m_MonoClass; currentClass != nullptr; currentClass = mono_class_get_parent(currentClass))
        {
            void* iterator = nullptr;
            MonoClassField* field = nullptr;
            while ((field = mono_class_get_fields(currentClass, &iterator)) != nullptr)
            {
                const uint32_t flags = mono_field_get_flags(field);

                if ((flags & MONO_FIELD_ATTR_FIELD_ACCESS_MASK) != MONO_FIELD_ATTR_PUBLIC)
                {
                    continue;
                }
                if ((flags & MONO_FIELD_ATTR_STATIC) != 0)
                {
                    continue;
                }
                if ((flags & MONO_FIELD_ATTR_INIT_ONLY) != 0)
                {
                    continue;
                }

                const char* fieldName = mono_field_get_name(field);
                if (fieldName && std::strchr(fieldName, '<') != nullptr)
                {
                    continue;
                }

                // 同名去重：子类先枚举，子类用 new 隐藏基类字段时子类优先；被过滤掉的字段不参与遮挡
                bool isShadowed = false;
                for (const ScriptField& existing : m_Fields)
                {
                    if (existing.Name == fieldName)
                    {
                        isShadowed = true;
                        break;
                    }
                }
                if (isShadowed)
                {
                    continue;
                }

                MonoType* fieldMonoType = mono_field_get_type(field);
                ScriptFieldType fieldType = ResolveScriptFieldType(fieldMonoType);
                if (fieldType == ScriptFieldType::None && IsEntityDerivedFieldType(fieldMonoType))
                {
                    fieldType = ScriptFieldType::Entity;    // 脚本互引：Entity 派生类按实体引用处理
                }
                if (fieldType == ScriptFieldType::None)
                {
                    LF_CORE_WARN("ScriptClass::GetFields - Unsupported type for field '{0}.{1}', ignored", m_ClassName, fieldName);
                    continue;
                }

                ScriptField scriptField;
                scriptField.Name = fieldName;
                scriptField.Type = fieldType;
                scriptField.Field = field;

                if (defaultInstance)
                {
                    GetFieldValue(defaultInstance, scriptField, scriptField.DefaultValue);
                }

                m_Fields.push_back(scriptField);
            }
        }

        return m_Fields;
    }

    bool ScriptClass::GetFieldValue(MonoObject* instance, const ScriptField& field, ScriptFieldValue& outValue) const
    {
        if (!instance || !field.Field)
        {
            return false;
        }

        outValue.Type = field.Type;

        switch (field.Type)
        {
            case ScriptFieldType::Bool:
            {
                // C# bool 在托管侧是 1 字节，用 uint8_t 中转，避免依赖 sizeof(bool) == 1
                uint8_t rawValue = 0;
                mono_field_get_value(instance, field.Field, &rawValue);
                outValue.Data = (rawValue != 0);
                return true;
            }
            case ScriptFieldType::SByte:  { return GetScalarField<int8_t>(instance, field.Field, outValue); }
            case ScriptFieldType::Byte:   { return GetScalarField<uint8_t>(instance, field.Field, outValue); }
            case ScriptFieldType::Short:  { return GetScalarField<int16_t>(instance, field.Field, outValue); }
            case ScriptFieldType::UShort: { return GetScalarField<uint16_t>(instance, field.Field, outValue); }
            case ScriptFieldType::Int:    { return GetScalarField<int32_t>(instance, field.Field, outValue); }
            case ScriptFieldType::UInt:   { return GetScalarField<uint32_t>(instance, field.Field, outValue); }
            case ScriptFieldType::Long:   { return GetScalarField<int64_t>(instance, field.Field, outValue); }
            case ScriptFieldType::ULong:  { return GetScalarField<uint64_t>(instance, field.Field, outValue); }
            case ScriptFieldType::Float:  { return GetScalarField<float>(instance, field.Field, outValue); }
            case ScriptFieldType::Double: { return GetScalarField<double>(instance, field.Field, outValue); }
            case ScriptFieldType::Vector2:    { return GetScalarField<glm::vec2>(instance, field.Field, outValue); }
            case ScriptFieldType::Vector3:    { return GetScalarField<glm::vec3>(instance, field.Field, outValue); }
            case ScriptFieldType::Vector4:
            case ScriptFieldType::Color:      { return GetScalarField<glm::vec4>(instance, field.Field, outValue); }
            case ScriptFieldType::Quaternion: { return GetScalarField<glm::quat>(instance, field.Field, outValue); }
            case ScriptFieldType::String:
            {
                // System.String 是引用类型：mono_field_get_value 写出的是 MonoString*
                MonoString* rawValue = nullptr;
                mono_field_get_value(instance, field.Field, &rawValue);
                if (!rawValue)
                {
                    outValue.Data = std::string();
                    return true;
                }

                char* utf8 = mono_string_to_utf8(rawValue);
                outValue.Data = utf8 ? std::string(utf8) : std::string();
                if (utf8)
                {
                    mono_free(utf8);
                }
                return true;
            }
            case ScriptFieldType::Entity:
            {
                MonoObject* rawObject = nullptr;
                mono_field_get_value(instance, field.Field, &rawObject);

                uint64_t handle = 0;
                TryGetReferenceHandle(rawObject, "ID", handle);
                outValue.Data = UUID(handle);
                return true;
            }
            case ScriptFieldType::Material:
            case ScriptFieldType::Mesh:
            case ScriptFieldType::Texture2D:
            case ScriptFieldType::Script:
            {
                MonoObject* rawObject = nullptr;
                mono_field_get_value(instance, field.Field, &rawObject);

                uint64_t handle = 0;
                if (!TryGetReferenceHandle(rawObject, "Handle", handle))
                {
                    outValue.Data = Ref<Asset>(nullptr);
                    return true;
                }

                outValue.Data = ScriptEngine::ResolveAssetByFieldType(field.Type, AssetHandle(handle));
                return true;
            }
            default:
            {
                return false;
            }
        }
    }

    bool ScriptClass::SetFieldValue(MonoObject* instance, const ScriptField& field, const ScriptFieldValue& value) const
    {
        if (!instance || !field.Field)
        {
            return false;
        }

        if (field.Type != value.Type)
        {
            LF_CORE_ERROR("ScriptClass::SetFieldValue - Type mismatch for field '{0}': field is {1}, value is {2}",
                field.Name, GetScriptFieldTypeInfo(field.Type).Name, GetScriptFieldTypeInfo(value.Type).Name);
            return false;
        }

        switch (value.Type)
        {
            case ScriptFieldType::Bool:
            {
                uint8_t rawValue = std::get<bool>(value.Data) ? 1 : 0;
                mono_field_set_value(instance, field.Field, &rawValue);
                return true;
            }
            case ScriptFieldType::SByte:  { return SetScalarField<int8_t>(instance, field.Field, value); }
            case ScriptFieldType::Byte:   { return SetScalarField<uint8_t>(instance, field.Field, value); }
            case ScriptFieldType::Short:  { return SetScalarField<int16_t>(instance, field.Field, value); }
            case ScriptFieldType::UShort: { return SetScalarField<uint16_t>(instance, field.Field, value); }
            case ScriptFieldType::Int:    { return SetScalarField<int32_t>(instance, field.Field, value); }
            case ScriptFieldType::UInt:   { return SetScalarField<uint32_t>(instance, field.Field, value); }
            case ScriptFieldType::Long:   { return SetScalarField<int64_t>(instance, field.Field, value); }
            case ScriptFieldType::ULong:  { return SetScalarField<uint64_t>(instance, field.Field, value); }
            case ScriptFieldType::Float:  { return SetScalarField<float>(instance, field.Field, value); }
            case ScriptFieldType::Double: { return SetScalarField<double>(instance, field.Field, value); }
            case ScriptFieldType::Vector2:    { return SetScalarField<glm::vec2>(instance, field.Field, value); }
            case ScriptFieldType::Vector3:    { return SetScalarField<glm::vec3>(instance, field.Field, value); }
            case ScriptFieldType::Vector4:
            case ScriptFieldType::Color:      { return SetScalarField<glm::vec4>(instance, field.Field, value); }
            case ScriptFieldType::Quaternion: { return SetScalarField<glm::quat>(instance, field.Field, value); }
            case ScriptFieldType::String:
            {
                const std::string& text = std::get<std::string>(value.Data);
                MonoString* rawValue = mono_string_new(mono_domain_get(), text.c_str());
                // mono_field_set_value 对引用类型字段直接传对象指针（语义不对称：值类型才传 &值）
                // 传 &rawValue 会把栈地址写进字段 -> 读回时解引用栈内存崩溃
                mono_field_set_value(instance, field.Field, rawValue);
                return true;
            }
            case ScriptFieldType::Entity:
            {
                const UUID id = std::get<UUID>(value.Data);
                SetReferenceHandle(instance, field.Field, "ID", static_cast<uint64_t>(id));
                return true;
            }
            case ScriptFieldType::Material:
            case ScriptFieldType::Mesh:
            case ScriptFieldType::Texture2D:
            case ScriptFieldType::Script:
            {
                const Ref<Asset> asset = std::get<Ref<Asset>>(value.Data);
                const AssetHandle handle = asset ? asset->GetHandle() : AssetHandle{};
                SetReferenceHandle(instance, field.Field, "Handle", static_cast<uint64_t>(handle));
                return true;
            }
            default:
            {
                return false;
            }
        }
    }

    // ======== ScriptInstance ========

    ScriptInstance::ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity)
        : m_ScriptClass(scriptClass)
    {
        m_Instance = scriptClass->Instantiate();

        m_Constructor = s_Data->EntityBaseClass->GetMethod(".ctor", 1);
        m_AwakeMethod = scriptClass->GetMethod("Awake", 0);
        m_UpdateMethod = scriptClass->GetMethod("Update", 1);
        m_DestroyMethod = scriptClass->GetMethod("OnDestroy", 0);

        UUID id = entity.GetUUID();
        void* param = &id;
        m_ScriptClass->InvokeMethod(m_Instance, m_Constructor, &param);
    }

    void ScriptInstance::SetFieldValues(const ScriptFieldMap& fieldMap)
    {
        if (!m_Instance || !m_ScriptClass)
        {
            return;
        }

        const std::vector<ScriptField>& fields = m_ScriptClass->GetFields();
        for (const ScriptField& field : fields)
        {
            auto it = fieldMap.find(field.Name);
            if (it == fieldMap.end())
            {
                // 字段表里没有这一项：保留托管对象的脚本内初始值
                continue;
            }

            if (it->second.Type != field.Type)
            {
                // 类型不匹配：写下去会按错误的位宽/位模式覆盖内存，必须拦下
                LF_CORE_WARN("ScriptInstance::SetFieldValues - Value type of field '{0}.{1}' does not match the script type, skipped", m_ScriptClass->GetName(), field.Name);
                continue;
            }

            m_ScriptClass->SetFieldValue(m_Instance, field, it->second);
        }
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

    void ScriptInstance::InvokeDestroy()
    {
        if (m_DestroyCalled)
        {
            return;
        }
        m_DestroyCalled = true;

        if (m_DestroyMethod)
        {
            m_ScriptClass->InvokeMethod(m_Instance, m_DestroyMethod);
        }
    }
}
