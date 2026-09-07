#pragma once

#include "entt.hpp"

#include "Lucky/Core/DeltaTime.h"
#include "Lucky/Core/UUID.h"
#include "Lucky/Renderer/EditorCamera.h"
#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Asset/Asset.h"

#include <glm/glm.hpp>

namespace Lucky
{
    class Entity;

    /// <summary>
    /// 场景运行状态
    /// - Edit：编辑器态，世界推进走 OnUpdateEditor（不跑脚本/物理）
    /// - Play：运行态，世界推进走 OnUpdateRuntime（跑脚本/物理）
    /// - Pause：仍走 OnUpdateRuntime，但内部跳过脚本/物理 tick，仅保留 Transform 层级更新与渲染
    /// </summary>
    enum class SceneState : uint8_t
    {
        Edit = 0,
        Play,
        Pause
    };

    /// <summary>
    /// 场景
    /// </summary>
    class Scene : public Asset
    {
    public:
        static AssetType StaticAssetType() { return AssetType::Scene; }
        AssetType GetAssetType() const override { return AssetType::Scene; }

        Scene(const std::string& name = "New Scene");
        ~Scene();

        // ---- 运行状态 ----

        /// <summary>
        /// 获取当前运行状态
        /// </summary>
        SceneState GetState() const { return m_State; }

        /// <summary>
        /// 直接设置运行状态：仅用于内部/序列化场景，不会触发任何回调
        /// 业务层切换 Play/Stop 请使用 OnRuntimeStart / OnRuntimeStop
        /// </summary>
        /// <param name="state">目标状态</param>
        void SetState(SceneState state) { m_State = state; }

        /// <summary>
        /// 进入运行态：切换到 Play 前调用
        /// 内部会把 State 置为 Play
        /// </summary>
        void OnRuntimeStart();

        /// <summary>
        /// 退出运行态：切回 Edit 前调用
        /// 内部会把 State 置为 Edit
        /// </summary>
        void OnRuntimeStop();

        /// <summary>
        /// 创建实体（作为根节点）
        /// </summary>
        /// <param name="name">实体名</param>
        /// <returns>实体</returns>
        Entity CreateEntity(const std::string& name = "Entity");
        Entity CreateEntity(UUID uuid, const std::string& name = "Entity");

        /// <summary>
        /// 创建实体（指定父节点）
        /// 如果 parent 为无效 Entity，则创建为根节点（等价于无参版本）
        /// </summary>
        /// <param name="name">实体名</param>
        /// <param name="parent">父实体（无效 Entity 表示创建为根节点）</param>
        /// <returns>实体</returns>
        Entity CreateEntity(const std::string& name, Entity parent);
        Entity CreateEntity(UUID uuid, const std::string& name, Entity parent);

        /// <summary>
        /// 销毁实体
        /// </summary>
        /// <param name="entity">实体</param>
        void DestroyEntity(Entity entity);

        /// <summary>
        /// 深拷贝一份 Scene 副本
        /// 
        /// 语义边界：
        /// - 组件：值语义完整拷贝（新 registry 不与源共享任何组件存储）
        /// - 资产引用（Mesh / Material / Texture / SkyboxMaterial 等 Ref&lt;Asset&gt;）：共享，副本与源指向同一份资产
        /// - UUID / RootEntityOrder / EnvironmentSettings / ViewportSize / Name：完整拷贝
        /// - Asset Handle：副本为无效 Handle（副本不进入 AssetRegistry）
        /// - SceneState：副本一律初始为 Edit
        /// 
        /// 副本不会触发任何 OnComponentAdded 回调
        /// </summary>
        /// <param name="other">源场景（不为空）</param>
        /// <returns>与源等价的新 Scene 实例</returns>
        static Ref<Scene> Copy(const Ref<Scene>& other);

        // ---- 世界推进：每帧唯一一次，由 EditorLayer 驱动 ----

        /// <summary>
        /// 编辑器态世界推进：Edit 状态下每帧调用
        /// 只做 Transform 层级更新等状态数据准备，不做任何相机渲染
        /// </summary>
        /// <param name="dt">帧间隔</param>
        void OnUpdateEditor(DeltaTime dt);

        /// <summary>
        /// 运行时世界推进：Play / Pause 状态下每帧调用
        /// 会驱动脚本 OnUpdate、物理 tick；Pause 状态下跳过这些，仅保留 Transform 层级更新
        /// 与相机无关，不做渲染
        /// </summary>
        /// <param name="dt">帧间隔</param>
        void OnUpdateRuntime(DeltaTime dt);

        // ---- 相机渲染：每窗口一次，可被多个面板重复调用 ----

        /// <summary>
        /// 编辑器视角渲染：由 Scene 面板调用
        /// 使用外部传入的 EditorCamera 提交渲染。Gizmo / Grid / Outline 等 Overlay
        /// 由调用方在此调用之后自行绘制
        /// </summary>
        /// <param name="camera">编辑器相机</param>
        void OnRenderEditor(EditorCamera& camera);

        /// <summary>
        /// 游戏视角渲染：由 Game 面板调用
        /// 使用场景内 Primary CameraComponent 作为视图/投影来源
        /// </summary>
        void OnRenderRuntime();

        /// <summary>
        /// 重置视口大小：视口改变时调用
        /// </summary>
        /// <param name="width">宽</param>
        /// <param name="height">高</param>
        void OnViewportResize(uint32_t width, uint32_t height);

        /// <summary>
        /// 查找场景中的主相机实体
        /// 遍历所有拥有 CameraComponent 的实体，返回第一个 Primary=true 的
        /// 若不存在，返回无效 Entity
        /// </summary>
        Entity GetPrimaryCameraEntity();

        /// <summary>
        /// 获取 Entity
        /// </summary>
        /// <param name="id">UUID</param>
        /// <returns></returns>
        Entity GetEntityWithUUID(UUID id);
        
        /// <summary>
        /// 尝试获取 Entity
        /// </summary>
        /// <param name="id">UUID</param>
        /// <returns></returns>
        Entity TryGetEntityWithUUID(UUID id);

        /// <summary>
        /// 检查 entt::entity 是否仍然有效（未被销毁）
        /// </summary>
        /// <param name="entity">entt 实体句柄</param>
        /// <returns>是否有效</returns>
        bool IsEntityValid(entt::entity entity) const { return m_Registry.valid(entity); }

        // ---- 根节点顺序管理（Hierarchy 拖拽排序） ----

        /// <summary>
        /// 获取根节点的有序列表（用于 Hierarchy 面板按序绘制）
        /// </summary>
        const std::vector<UUID>& GetRootEntityOrder() const { return m_RootEntityOrder; }

        /// <summary>
        /// 将实体插入到根节点列表的指定位置
        /// 如果实体已存在于列表中，会先移除再插入（用于同级排序场景）
        /// </summary>
        /// <param name="entityID">实体 UUID</param>
        /// <param name="index">插入位置索引（-1 或越界表示追加到末尾）</param>
        void InsertRootEntity(UUID entityID, int index = -1);

        /// <summary>
        /// 从根节点列表中移除实体（非根节点调用为 no-op）
        /// </summary>
        /// <param name="entityID">实体 UUID</param>
        void RemoveRootEntity(UUID entityID);

        /// <summary>
        /// 获取实体在其父节点 Children 列表中的索引
        /// 如果实体是根节点，返回其在根节点列表 m_RootEntityOrder 中的索引
        /// </summary>
        /// <param name="entity">实体</param>
        /// <returns>索引；如果实体无效或未找到，返回 -1</returns>
        int GetEntityIndexInParent(Entity entity);

        /// <summary>
        /// 返回具有 TComponents 类型组件的所有 Entt
        /// </summary>
        /// <typeparam name="...TComponents">组件类型列表</typeparam>
        /// <returns>Entts</returns>
        template<typename... TComponents>
        auto GetAllEntitiesWith()
        {
            return m_Registry.view<TComponents...>();
        }
        
        void ClearAllEntities();

        /// <summary>
        /// 更新 Transform 层级：从根节点递归计算所有实体的世界变换矩阵
        /// 每帧在世界推进阶段调用
        /// </summary>
        void UpdateTransformHierarchy();
        
        // ---- 环境设置 ----
        EnvironmentSettings& GetEnvironmentSettings() { return m_EnvironmentSettings; }
        const EnvironmentSettings& GetEnvironmentSettings() const { return m_EnvironmentSettings; }
    private:
        /// <summary>
        /// entity 添加 TComponent 组件时调用
        /// </summary>
        /// <typeparam name="TComponent">组件类型</typeparam>
        /// <param name="entity">实体</param>
        /// <param name="component">组件</param>
        template<typename TComponent>
        void OnComponentAdded(Entity entity, TComponent& component);

        /// <summary>
        /// 递归更新实体及其子树的世界变换矩阵
        /// </summary>
        /// <param name="entity">当前实体</param>
        /// <param name="parentWorldTransform">父节点的世界变换矩阵</param>
        void UpdateWorldTransformRecursive(Entity entity, const glm::mat4& parentWorldTransform);

        /// <summary>
        /// 相机渲染实现：给定 CameraRenderData 后跑一遍完整的渲染流程
        /// OnRenderEditor 和 OnRenderRuntime 在拿到自己的相机数据后统一走这里
        /// 内部执行"收集光源 → BeginScene → 收集后处理 → 提交 Mesh/Sprite → EndScene"
        /// </summary>
        /// <param name="cam">相机渲染数据</param>
        void RenderSceneImpl(const CameraRenderData& cam);
    private:
        friend class Entity;                // 友元类 Entity
        friend class SceneHierarchyPanel;   // 友元类 SceneHierarchyPanel
        friend class SceneSerializer;       // 友元类 SceneSerializer

        std::unordered_map<UUID, Entity> m_EntityIDMap; // UUID - entt 映射表
        std::vector<UUID> m_RootEntityOrder;            // 根节点显示顺序（Hierarchy 面板按此顺序绘制）

        entt::registry m_Registry;          // 实体集合：实体 id 集合（unsigned int 集合）

        uint32_t m_ViewportWidth = 1280;    // 场景视口宽
        uint32_t m_ViewportHeight = 720;    // 场景视口高

        SceneState m_State = SceneState::Edit;
        
        EnvironmentSettings m_EnvironmentSettings;  // 环境设置参数
    };
}