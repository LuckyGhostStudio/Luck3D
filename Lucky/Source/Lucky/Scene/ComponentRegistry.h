#pragma once

#include "ComponentDescriptor.h"

#include <functional>
#include <vector>

namespace Lucky
{
    /// <summary>
    /// 组件注册表：集中管理所有组件类型的 ComponentDescriptor
    /// 
    /// 分三段填充：
    /// - RegisterCore：ECS 元信息（Type / Name / Copy / Has）
    ///   由 Scene/ComponentCores.cpp 中的 RegisterAllCores 提供
    /// - RegisterSerialization：序列化元信息（SerializedKey / Serialize / Deserialize）
    ///   由 Serialization/ComponentSerializers.cpp 中的 RegisterAllSerializations 提供
    /// - RegisterInspector：Inspector 元信息（Draw / GetIcon / AddMenuItems / Remove / 展示控制）
    ///   由 Editor/ComponentInspectors.cpp 中的 RegisterAllInspectors 提供
    /// 
    /// 生命周期：EditorLayer::OnAttach 中调用 RegisterAll() 完成一次性注册；OnDetach 中调用 Clear()
    /// 遍历顺序：与 RegisterAllCores 内注册顺序一致（保证 Inspector / Serializer 输出稳定）
    /// </summary>
    class ComponentRegistry
    {
    public:
        /// <summary>
        /// 一次性注册所有内置组件描述符
        /// 内部按顺序调用 RegisterAllCores + RegisterAllSerializations + RegisterAllInspectors
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
        /// 注册组件的 ECS 元信息（Type / Name / Copy / Has）
        /// 由 Scene/ComponentCores.cpp 中的 RegisterAllCores 调用
        /// </summary>
        /// <param name="desc">仅需填充 Type / Name / Copy / Has 字段的描述符</param>
        static void RegisterCore(ComponentDescriptor desc);

        /// <summary>
        /// 补充组件的序列化元信息（SerializedKey / Serialize / Deserialize）
        /// 必须在对应类型的 RegisterCore 之后调用
        /// </summary>
        /// <param name="type">组件类型</param>
        /// <param name="serializedKey">YAML 键</param>
        /// <param name="serialize">序列化回调</param>
        /// <param name="deserialize">反序列化回调</param>
        static void RegisterSerialization(ComponentType type,
                                          std::string serializedKey,
                                          ComponentDescriptor::SerializeFn serialize,
                                          ComponentDescriptor::DeserializeFn deserialize);

        /// <summary>
        /// 补充组件的 Inspector 元信息（Draw / GetIcon / AddMenuItems / Remove / 展示控制）
        /// 必须在对应类型的 RegisterCore 之后调用
        /// </summary>
        /// <param name="type">组件类型（必须已经 RegisterCore）</param>
        /// <param name="draw">Inspector 绘制回调（nullptr 表示不在 Inspector 显示）</param>
        /// <param name="getIcon">图标解析回调</param>
        /// <param name="addMenuItems">AddComponent 菜单项列表（空表示不出现在菜单）</param>
        /// <param name="remove">从 entity 移除该组件的回调（canRemove 为 false 时可为空）</param>
        /// <param name="showInHierarchyIcons">是否出现在 Hierarchy 右侧图标条</param>
        /// <param name="canRemove">Settings 弹窗是否显示 Remove 项</param>
        /// <param name="extraContextMenuItems">组件独有的 Settings 弹窗菜单项（空表示无）</param>
        static void RegisterInspector(ComponentType type,
                                      ComponentDescriptor::DrawFn draw,
                                      ComponentDescriptor::IconFn getIcon,
                                      std::vector<ComponentAddMenuItem> addMenuItems,
                                      ComponentDescriptor::RemoveFn remove,
                                      bool showInHierarchyIcons = true,
                                      bool canRemove = true,
                                      std::vector<ComponentContextMenuItem> extraContextMenuItems = {});
    private:
        /// <summary>
        /// 注册全部组件的 ECS 元信息
        /// 实现位于 Scene/ComponentCores.cpp
        /// </summary>
        static void RegisterAllCores();

        /// <summary>
        /// 注册全部组件的序列化元信息
        /// 实现位于 Serialization/ComponentSerializers.cpp
        /// </summary>
        static void RegisterAllSerializations();

        /// <summary>
        /// 注册全部组件的 Inspector 元信息
        /// 实现位于 Editor/ComponentInspectors.cpp
        /// </summary>
        static void RegisterAllInspectors();
    };
}
