#pragma once

#include "ComponentDescriptor.h"

#include <functional>
#include <vector>

namespace Lucky
{
    /// <summary>
    /// 组件注册表：集中管理所有组件类型的 ComponentDescriptor
    /// 
    /// 生命周期：EditorLayer::OnAttach 中调用 RegisterAll() 完成一次性注册；OnDetach 中调用 Clear()
    /// 遍历顺序：与 RegisterAll 内注册顺序一致（保证 Inspector / Serializer 输出稳定）
    /// </summary>
    class ComponentRegistry
    {
    public:
        /// <summary>
        /// 一次性注册所有内置组件描述符
        /// </summary>
        static void RegisterAll();

        /// <summary>
        /// 清空注册表
        /// </summary>
        static void Clear();

        /// <summary>
        /// 通过 ComponentType 查描述符
        /// </summary>
        /// <param name="type">组件类型</param>
        /// <returns>描述符指针；未注册时返回 nullptr</returns>
        static const ComponentDescriptor* GetByType(ComponentType type);

        /// <summary>
        /// 按注册顺序遍历所有描述符
        /// </summary>
        /// <param name="callback">遍历回调</param>
        static void ForEach(const std::function<void(const ComponentDescriptor&)>& callback);

        /// <summary>
        /// 按注册顺序返回全部描述符
        /// </summary>
        static const std::vector<ComponentDescriptor>& All();

        /// <summary>
        /// 单个描述符注册（RegisterAll 内部使用）
        /// </summary>
        /// <param name="desc">描述符</param>
        static void Register(ComponentDescriptor desc);
    };
}
