#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Asset/AssetHandle.h"

#include <filesystem>
#include <functional>

namespace Lucky
{
    class Scene;

    /// <summary>
    /// 场景管理器：维护"当前活动场景"的唯一真源，并向所有订阅方广播场景切换事件
    /// 
    /// 设计目标（对齐 Unity SceneManagement 的最小子集）：
    /// - ActiveScene：全编辑器共享的唯一当前场景，避免各面板/Layer 之间手动 SetScene 造成的耦合
    /// - Subscribe / Unsubscribe：面板在 ctor 订阅、dtor 取消订阅，场景切换时自动同步
    /// - OpenScene / NewScene：所有创建/打开场景的入口（菜单 New/Open... 命令行加载 拖拽 双击）都归到这里
    /// 
    /// 未来扩展（预留，未实现）：
    /// - Additive 加载 / 多场景（GetSceneAt / sceneCount）
    /// - PIE 支持：拆分 EditorSceneManager / RuntimeSceneManager
    /// - 事件细分：Loaded / Unloaded / ActiveChanged 三通道
    /// </summary>
    class SceneManager
    {
    public:
        /// <summary>
        /// 场景切换回调签名：参数为切换后的新活动场景
        /// </summary>
        using SceneChangedCallback = std::function<void(const Ref<Scene>&)>;

        /// <summary>
        /// 订阅句柄：Subscribe 返回，用于 Unsubscribe
        /// 0 表示无效句柄
        /// </summary>
        using SubscriptionHandle = uint32_t;

        // ---- 生命周期 ----

        /// <summary>
        /// 初始化：清空订阅表和 ActiveScene
        /// 在 EditorLayer::OnAttach 中调用
        /// </summary>
        static void Init();

        /// <summary>
        /// 关闭：释放订阅表和 ActiveScene 引用
        /// 在 EditorLayer::OnDetach 中调用
        /// </summary>
        static void Shutdown();

        // ---- ActiveScene 访问 ----

        /// <summary>
        /// 获取当前活动场景
        /// </summary>
        static const Ref<Scene>& GetActiveScene();

        /// <summary>
        /// 设置当前活动场景（唯一底层接口，所有 Open/New 最终都归到这里）
        /// 触发所有订阅者的 SceneChanged 回调
        /// </summary>
        /// <param name="scene">新场景（可以为 nullptr，表示当前无场景）</param>
        static void SetActiveScene(const Ref<Scene>& scene);

        // ---- 场景打开 / 新建 ----

        /// <summary>
        /// 通过 AssetHandle 打开场景（拖拽 / Project 面板双击的入口）
        /// 内部会清除该 Handle 的缓存并重新加载，确保每次打开都是新实例
        /// </summary>
        /// <param name="handle">场景资产 Handle（必须是 AssetType::Scene）</param>
        /// <returns>是否成功切换</returns>
        static bool OpenScene(AssetHandle handle);

        /// <summary>
        /// 通过文件路径打开场景（命令行加载 / File → Open... 菜单入口）
        /// 内部会 ImportAsset 注册到 Registry，然后走 OpenScene(handle)
        /// </summary>
        /// <param name="filepath">相对或绝对路径，扩展名必须为 .luck3d</param>
        /// <returns>是否成功切换</returns>
        static bool OpenScene(const std::filesystem::path& filepath);

        /// <summary>
        /// 新建场景并保存为资产（File → New 菜单入口）
        /// 弹出保存对话框选择路径后创建 .luck3d 文件并切换为活动场景
        /// </summary>
        /// <returns>是否成功新建（用户取消返回 false）</returns>
        static bool NewSceneWithDialog();

        /// <summary>
        /// 将当前活动场景另存为新的 .luck3d 文件（File → Save As 菜单入口）
        /// 
        /// 行为：
        /// - 弹出保存对话框选择新路径
        /// - 调用 CreateAsset 序列化当前场景到新路径并注册为新资产（Handle 会变更）
        /// - 将 ActiveScene 切换为新场景实例（后续 Save 保存到新路径）
        /// 
        /// 注意：原路径的旧场景文件保留不变（对齐 Unity Save As 行为）
        /// </summary>
        /// <returns>是否成功（无活动场景 / 用户取消 / 写盘失败均返回 false）</returns>
        static bool SaveSceneAs();

        // ---- 事件订阅 ----

        /// <summary>
        /// 订阅场景切换事件
        /// 面板通常在 ctor 中订阅，在 dtor 中取消订阅
        /// 
        /// 注意：Subscribe 后不会立即回调；如需拿到当前场景，请在 Subscribe 后再调用 GetActiveScene()
        /// </summary>
        /// <param name="callback">回调函数</param>
        /// <returns>订阅句柄（用于取消订阅），失败返回 0</returns>
        static SubscriptionHandle Subscribe(SceneChangedCallback callback);

        /// <summary>
        /// 取消订阅
        /// </summary>
        /// <param name="handle">Subscribe 返回的句柄</param>
        static void Unsubscribe(SubscriptionHandle handle);
    };
}
