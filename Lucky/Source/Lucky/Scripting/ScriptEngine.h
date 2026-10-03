#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Core/DeltaTime.h"
#include "Lucky/Core/UUID.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Scene/Entity.h"
#include "Lucky/Asset/AssetHandle.h"
#include "Lucky/Scripting/ScriptFieldType.h"
#include "Lucky/Scripting/ScriptFieldValue.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

extern "C"
{
    typedef struct _MonoAssembly MonoAssembly;
    typedef struct _MonoClass MonoClass;
    typedef struct _MonoClassField MonoClassField;
    typedef struct _MonoMethod MonoMethod;
    typedef struct _MonoObject MonoObject;
    typedef struct _MonoImage MonoImage;
    typedef struct _MonoDomain MonoDomain;
}

namespace Lucky
{
    class Asset;
    class ScriptClass;
    class ScriptInstance;

    /// <summary>
    /// 脚本引擎：管理 Mono 运行时、加载程序集、驱动脚本生命周期
    /// </summary>
    class ScriptEngine
    {
    public:
        /// <summary>
        /// 初始化 Mono 运行时并加载 Core / App 程序集
        /// </summary>
        static void Init();

        /// <summary>
        /// 关闭 Mono 运行时
        /// </summary>
        static void Shutdown();

        /// <summary>
        /// 从磁盘加载核心程序集 Lucky-ScriptCore.dll
        /// </summary>
        /// <param name="filepath">程序集路径</param>
        static void LoadCoreAssembly(const std::filesystem::path& filepath);

        /// <summary>
        /// 从磁盘加载用户程序集 App.dll
        /// 文件不存在时打印警告并返回，不视为错误
        /// </summary>
        /// <param name="filepath">程序集路径</param>
        static void LoadAppAssembly(const std::filesystem::path& filepath);

        /// <summary>
        /// 进入运行态：由 Scene::OnRuntimeStart 调用
        /// </summary>
        /// <param name="scene">当前进入运行态的场景</param>
        static void OnRuntimeStart(Scene* scene);

        /// <summary>
        /// 退出运行态：由 Scene::OnRuntimeStop 调用
        /// 先对所有脚本实例调用 OnDestroy，再释放实例、清空场景上下文
        /// </summary>
        static void OnRuntimeStop();

        /// <summary>
        /// 为一个实体实例化托管对象并调用 Awake
        /// 由 Scene::OnRuntimeStart 在遍历 ScriptComponent 时调用；脚本类由调用方解析后传入
        /// </summary>
        /// <param name="entity">目标实体</param>
        /// <param name="scriptClass">已解析出的脚本类（不为空）</param>
        static void OnCreateEntityScript(Entity entity, const Ref<ScriptClass>& scriptClass);

        /// <summary>
        /// 对指定实体上已实例化的脚本对象调用 Update(dt)
        /// 由 Scene::OnUpdateRuntime 在 Play 状态下调用；实体无脚本实例时静默跳过
        /// </summary>
        /// <param name="entity">目标实体</param>
        /// <param name="dt">帧间隔</param>
        static void OnUpdateEntityScript(Entity entity, DeltaTime dt);

        /// <summary>
        /// 丢弃实体的脚本实例：先调用 OnDestroy，再从实例表中移除
        /// 由 Scene::DestroyEntity 与 ScriptComponent 被移除时调用；无实例时静默跳过
        /// 每个实例只会收到一次 OnDestroy
        /// </summary>
        /// <param name="entity">目标实体</param>
        static void OnDestroyEntityScript(Entity entity);

        /// <summary>
        /// 用户程序集中是否存在指定全名的 Entity 派生类
        /// </summary>
        /// <param name="fullClassName">"Namespace.ClassName" 形式的类全名</param>
        static bool EntityScriptClassExists(const std::string& fullClassName);

        /// <summary>
        /// 按简单类名在用户程序集里解析脚本类
        /// 匹配规则：ScriptClass::GetName() 等于 className（文件名必须与类名一致）
        /// 由脚本资产（.cs）解析到可实例化的类；找到多个同名类时视为失败
        /// </summary>
        /// <param name="className">简单类名（通常是 .cs 的文件名词干，如 "PlayerController"）</param>
        /// <returns>脚本类；未找到或有多个同名类时返回 nullptr</returns>
        static Ref<ScriptClass> ResolveScriptClass(const std::string& className);

        /// <summary>
        /// 把脚本字段表与脚本类的当前字段列表对齐
        /// - 类里新增的字段：用 ScriptField::DefaultValue 补进 fieldMap
        /// - 已存在但类型与脚本不符的字段：用 DefaultValue 覆盖（脚本改了字段类型时的必然结果）
        /// - fieldMap 里多出来的字段（脚本已删）：移除
        /// 默认值取 GetFields() 已读好的 ScriptField::DefaultValue，纯数据操作，不创建托管对象
        /// </summary>
        /// <param name="scriptAsset">脚本资产（为空时清空 fieldMap）</param>
        /// <param name="fieldMap">待对齐的字段表（原地修改）</param>
        static void SyncScriptFieldMap(const Ref<Script>& scriptAsset, ScriptFieldMap& fieldMap);

        /// <summary>
        /// 获取当前 Runtime 场景上下文（非 Runtime 期为 nullptr）
        /// </summary>
        static Scene* GetSceneContext();

        /// <summary>
        /// 获取核心程序集 Image（供 ScriptGlue / Component 反射使用）
        /// </summary>
        static MonoImage* GetCoreAssemblyImage();

        /// <summary>
        /// 获取当前用户程序集中所有 Entity 派生类列表
        /// </summary>
        static const std::unordered_map<std::string, Ref<ScriptClass>>& GetEntityClasses();

        /// <summary>
        /// 按字段类型从资产句柄解析资产对象；类型不匹配或句柄无效时返回空引用
        /// </summary>
        static Ref<Asset> ResolveAssetByFieldType(ScriptFieldType type, AssetHandle handle);
    private:
        friend class ScriptClass;
        friend class ScriptInstance;

        static void InitMono();
        static void ShutdownMono();

        static MonoObject* InstantiateClass(MonoClass* monoClass);
        static void LoadAssemblyClasses();
    };

    /// <summary>
    /// 脚本字段元信息：字段名、受支持的类型、默认值、以及 mono 侧字段句柄
    /// 仅供 Scripting 层内部使用；组件层只需要 ScriptFieldValue
    /// </summary>
    struct ScriptField
    {
        std::string       Name;
        ScriptFieldType   Type = ScriptFieldType::None;
        ScriptFieldValue  DefaultValue;
        MonoClassField*   Field = nullptr;
    };

    /// <summary>
    /// 脚本类：包装 MonoClass，提供实例化 / 方法查找 / 方法调用能力
    /// </summary>
    class ScriptClass
    {
    public:
        ScriptClass() = default;
        ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore = false);

        /// <summary>
        /// 创建当前类的 Mono 实例（会调用无参构造）
        /// </summary>
        MonoObject* Instantiate();

        /// <summary>
        /// 按方法名 + 参数个数查找 MonoMethod
        /// </summary>
        /// <param name="name">方法名</param>
        /// <param name="parameterCount">参数个数</param>
        MonoMethod* GetMethod(const std::string& name, int parameterCount);

        /// <summary>
        /// 调用给定实例上的方法
        /// </summary>
        /// <param name="instance">Mono 对象实例</param>
        /// <param name="method">方法</param>
        /// <param name="params">参数指针数组</param>
        MonoObject* InvokeMethod(MonoObject* instance, MonoMethod* method, void** params = nullptr);

        /// <summary>
        /// 获取该类的可编辑字段列表（public 实例字段、类型受支持，含继承链上的字段）
        /// 首次调用时枚举并缓存，同时读取每个字段的默认值（会临时实例化一个对象）
        /// 程序集重新加载时随 EntityClasses 一起重建
        /// </summary>
        /// <returns>字段列表的常量引用</returns>
        const std::vector<ScriptField>& GetFields();

        /// <summary>
        /// 读取指定实例上某个字段的值
        /// </summary>
        /// <param name="instance">托管对象实例</param>
        /// <param name="field">字段元信息（必须来自本类的 GetFields()）</param>
        /// <param name="outValue">输出：字段值</param>
        /// <returns>是否读取成功</returns>
        bool GetFieldValue(MonoObject* instance, const ScriptField& field, ScriptFieldValue& outValue) const;

        /// <summary>
        /// 写入指定实例上某个字段的值
        /// </summary>
        /// <param name="instance">托管对象实例</param>
        /// <param name="field">字段元信息（必须来自本类的 GetFields()）</param>
        /// <param name="value">要写入的值（按 value.Type 决定写入哪个载荷）</param>
        /// <returns>是否写入成功</returns>
        bool SetFieldValue(MonoObject* instance, const ScriptField& field, const ScriptFieldValue& value) const;

        const std::string& GetNamespace() const { return m_ClassNamespace; }
        const std::string& GetName() const { return m_ClassName; }
    private:
        std::string m_ClassNamespace;
        std::string m_ClassName;

        MonoClass* m_MonoClass = nullptr;

        std::vector<ScriptField> m_Fields;
        bool m_FieldsInitialized = false;
    };

    /// <summary>
    /// 脚本实例：一个 ScriptClass 的运行时对象，缓存 Awake / Update 方法句柄
    /// </summary>
    class ScriptInstance
    {
    public:
        ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity);

        /// <summary>
        /// 调用 Awake 方法（若脚本类未定义则跳过）
        /// </summary>
        void InvokeAwake();

        /// <summary>
        /// 调用 Update(float dt) 方法（若脚本类未定义则跳过）
        /// </summary>
        /// <param name="dt">帧间隔</param>
        void InvokeUpdate(float dt);

        /// <summary>
        /// 调用 OnDestroy 方法（若脚本类未定义则跳过）
        /// </summary>
        void InvokeDestroy();

        Ref<ScriptClass> GetScriptClass() const { return m_ScriptClass; }
    private:
        Ref<ScriptClass> m_ScriptClass;

        MonoObject* m_Instance = nullptr;

        MonoMethod* m_Constructor = nullptr;
        MonoMethod* m_AwakeMethod = nullptr;
        MonoMethod* m_UpdateMethod = nullptr;
        MonoMethod* m_DestroyMethod = nullptr;

        bool m_DestroyCalled = false;
    };
}
