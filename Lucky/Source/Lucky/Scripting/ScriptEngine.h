#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Core/DeltaTime.h"
#include "Lucky/Core/UUID.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Scene/Entity.h"

#include <filesystem>
#include <string>
#include <unordered_map>

extern "C"
{
    typedef struct _MonoAssembly MonoAssembly;
    typedef struct _MonoClass MonoClass;
    typedef struct _MonoMethod MonoMethod;
    typedef struct _MonoObject MonoObject;
    typedef struct _MonoImage MonoImage;
    typedef struct _MonoDomain MonoDomain;
}

namespace Lucky
{
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
        /// 释放所有脚本实例的 Mono 引用，清空场景上下文
        /// </summary>
        static void OnRuntimeStop();

        /// <summary>
        /// 为一个实体实例化托管对象并调用 Awake
        /// 由 Scene::OnRuntimeStart 在遍历 ScriptComponent 时调用；ClassName 由调用方从组件读取
        /// 若 fullClassName 未在用户程序集中登记，则跳过（打印警告）
        /// </summary>
        /// <param name="entity">目标实体</param>
        /// <param name="fullClassName">脚本类全名（"Namespace.ClassName"）</param>
        static void OnCreateEntityScript(Entity entity, const std::string& fullClassName);

        /// <summary>
        /// 对指定实体上已实例化的脚本对象调用 Update(dt)
        /// 由 Scene::OnUpdateRuntime 在 Play 状态下调用；实体无脚本实例时静默跳过
        /// </summary>
        /// <param name="entity">目标实体</param>
        /// <param name="dt">帧间隔</param>
        static void OnUpdateEntityScript(Entity entity, DeltaTime dt);

        /// <summary>
        /// 用户程序集中是否存在指定全名的 Entity 派生类
        /// </summary>
        /// <param name="fullClassName">"Namespace.ClassName" 形式的类全名</param>
        static bool EntityScriptClassExists(const std::string& fullClassName);

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
    private:
        friend class ScriptClass;
        friend class ScriptInstance;

        static void InitMono();
        static void ShutdownMono();

        static MonoObject* InstantiateClass(MonoClass* monoClass);
        static void LoadAssemblyClasses();
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

        const std::string& GetNamespace() const { return m_ClassNamespace; }
        const std::string& GetName() const { return m_ClassName; }
    private:
        std::string m_ClassNamespace;
        std::string m_ClassName;

        MonoClass* m_MonoClass = nullptr;
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

        Ref<ScriptClass> GetScriptClass() const { return m_ScriptClass; }
    private:
        Ref<ScriptClass> m_ScriptClass;

        MonoObject* m_Instance = nullptr;

        MonoMethod* m_Constructor = nullptr;
        MonoMethod* m_AwakeMethod = nullptr;
        MonoMethod* m_UpdateMethod = nullptr;
    };
}
