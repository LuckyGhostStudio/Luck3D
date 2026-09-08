#pragma once

#include "Lucky/Scene/ComponentDescriptor.h"

#include <functional>
#include <vector>

namespace Lucky
{
    /// <summary>
    /// 组件设置弹窗（Inspector 组件头部齿轮按钮）的通用菜单项注册表
    ///
    /// 每个组件的 Settings 弹窗内容由两段组成：
    /// - 通用段：本注册表持有的项（Remove Component 等），跨所有组件复用
    /// - 私有段：ComponentDescriptor::ExtraContextMenuItems 中的项，仅属于该组件（如脚本的 Edit Script）
    ///
    /// 延后动作队列：Execute 中不应立即修改 Entity 的组件集合（会导致当前帧的 Draw 访问已删除的组件）
    /// 所有需要"修改 Entity 组件"的动作应通过 EnqueueDeferredAction 排队，由 InspectorPanel::DrawComponents
    /// 末尾统一调用 FlushDeferredActions 执行
    ///
    /// 生命周期：EditorLayer::OnAttach 中调用 RegisterBuiltins() 完成一次性注册；OnDetach 中调用 Clear()
    /// </summary>
    class ComponentContextMenuRegistry
    {
    public:
        /// <summary>
        /// 注册所有内置通用菜单项（当前包含 Remove Component）
        /// 必须在 ComponentRegistry::RegisterAll() 之后调用
        /// </summary>
        static void RegisterBuiltins();

        /// <summary>
        /// 清空通用项与延后动作队列
        /// </summary>
        static void Clear();

        /// <summary>
        /// 追加一项通用菜单项到末尾
        /// 供其他模块（Prefab / Undo / Clipboard 等）注入自身通用项
        /// </summary>
        static void Register(ComponentContextMenuItem item);

        /// <summary>
        /// 按注册顺序返回全部通用项
        /// </summary>
        static const std::vector<ComponentContextMenuItem>& All();

        /// <summary>
        /// 将一个动作排入延后队列，由 FlushDeferredActions 在帧末统一执行
        /// 用于在菜单 Execute 中安全修改 Entity 的组件集合
        /// </summary>
        static void EnqueueDeferredAction(std::function<void()> action);

        /// <summary>
        /// 执行并清空延后动作队列
        /// 由 InspectorPanel::DrawComponents 末尾调用
        /// </summary>
        static void FlushDeferredActions();
    };
}
