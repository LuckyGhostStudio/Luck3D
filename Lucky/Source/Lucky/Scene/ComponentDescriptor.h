#pragma once

#include "Components/ComponentType.h"

#include "Lucky/Core/Base.h"

#include <entt.hpp>

#include <functional>
#include <string>
#include <vector>

namespace YAML
{
    class Emitter;
    class Node;
}

namespace Lucky
{
    class Entity;
    class Texture2D;
    struct ComponentDescriptor;

    /// <summary>
    /// AddComponent 菜单中的一项
    /// 允许一个 ComponentType 对应多个菜单项（如 LightComponent 的三种子类型）
    /// </summary>
    struct ComponentAddMenuItem
    {
        std::string Label;                                              // 菜单显示名（如 "Directional Light"）
        std::function<const Ref<Texture2D>&()> GetIcon;                 // 菜单项图标（无 entity 上下文，取默认图标）
        std::function<void(Entity)> AddFn;                              // 点击后执行（走 Entity::AddComponent，触发 OnComponentAdded）
    };

    /// <summary>
    /// 组件设置弹窗（齿轮按钮）中的一项菜单
    /// 通用项（Remove Component 等）与组件私有项（如脚本的 Edit Script）复用同一结构
    /// </summary>
    struct ComponentContextMenuItem
    {
        std::string Label;                                                                              // 菜单显示名（如 "Remove Component"）
        std::function<bool(Entity, const ComponentDescriptor&)> IsVisible;                              // 运行期可见性判定；nullptr 视为始终可见
        std::function<bool(Entity, const ComponentDescriptor&)> IsEnabled;                              // 运行期启用性判定；nullptr 视为始终启用
        std::function<void(Entity, const ComponentDescriptor&)> Execute;                                // 点击回调
        bool SeparatorAfter = false;                                                                    // 在此项之后画一条分隔线
    };

    /// <summary>
    /// 组件描述符：承载一种组件类型的所有跨系统元信息
    /// 由 ComponentRegistry 集中持有，供 Scene::Copy / SceneSerializer / InspectorPanel / SceneHierarchyPanel 消费
    /// </summary>
    struct ComponentDescriptor
    {
        // ---- 静态元信息 ----

        ComponentType Type = ComponentType::None;                       // 类型键（与 ComponentTrait<T>::Type 一致）
        std::string Name;                                               // 显示名（Inspector 标题，如 "Mesh Renderer"）
        std::string SerializedKey;                                      // YAML 键（如 "MeshRendererComponent"）

        // ---- 回调 ----

        /// <summary>
        /// 深拷贝：将 src 中的该组件（若存在）以值语义拷贝到 dst
        /// 直接走 entt::registry::emplace_or_replace，不触发 OnComponentAdded
        /// </summary>
        using CopyFn = std::function<void(entt::registry& dst, entt::entity dstEnt,
                                          entt::registry& src, entt::entity srcEnt)>;
        CopyFn Copy;

        /// <summary>
        /// 序列化：如果 entity 拥有该组件，向 out 写入完整 YAML 段（含 SerializedKey 顶层键）
        /// 若 entity 无该组件则不写任何内容
        /// </summary>
        using SerializeFn = std::function<void(YAML::Emitter& out, Entity entity)>;
        SerializeFn Serialize;

        /// <summary>
        /// 反序列化：从 entityNode 中读取该组件的 YAML 段并添加到 entity
        /// 若 entityNode 中不含该组件的键（!entityNode[SerializedKey]），实现方需自行无操作返回
        /// </summary>
        using DeserializeFn = std::function<void(Entity entity, const YAML::Node& entityNode)>;
        DeserializeFn Deserialize;

        /// <summary>
        /// Inspector 绘制：若 entity 拥有该组件，绘制组件面板内容
        /// 通用外壳（TreeNode / 图标 / Name / Settings 按钮 / Remove 菜单）由 InspectorPanel 统一绘制
        /// </summary>
        using DrawFn = std::function<void(Entity entity)>;
        DrawFn Draw;

        /// <summary>
        /// 图标解析：给定 entity（Entity 上必须拥有该组件），返回其显示图标
        /// 允许基于组件实例内部字段返回不同图标（LightComponent 依 LightType 分派）
        /// </summary>
        using IconFn = std::function<const Ref<Texture2D>&(Entity entity)>;
        IconFn GetIcon;

        /// <summary>
        /// 判定 entity 是否拥有该组件
        /// Registry 循环中类型信息已擦除，需要通过 lambda 封装 Entity::HasComponent&lt;T&gt;
        /// </summary>
        using HasFn = std::function<bool(Entity entity)>;
        HasFn Has;

        /// <summary>
        /// 从 entity 移除该组件
        /// CanRemove == false 的组件可留空
        /// </summary>
        using RemoveFn = std::function<void(Entity entity)>;
        RemoveFn Remove;

        // ---- AddComponent 菜单 ----

        std::vector<ComponentAddMenuItem> AddMenuItems;                 // 空数组表示不出现在 AddComponent 菜单

        // ---- Settings 弹窗私有菜单项 ----

        std::vector<ComponentContextMenuItem> ExtraContextMenuItems;    // 组件独有的 Settings 弹窗菜单项（空表示无）

        // ---- 展示控制 ----

        bool ShowInHierarchyIcons = true;                               // 是否出现在 Hierarchy 右侧图标条
        bool CanRemove = true;                                          // Settings 弹窗是否显示 Remove 项
    };
}
