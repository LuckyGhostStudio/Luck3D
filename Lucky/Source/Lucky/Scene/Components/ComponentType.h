#pragma once

#include <cstdint>

namespace Lucky
{
    /// <summary>
    /// 组件类型枚举
    /// 用于图标查找、序列化标识、运行时反射等场景
    /// </summary>
    enum class ComponentType : uint8_t
    {
        None = 0,
        Transform,
        Light,
        MeshFilter,
        MeshRenderer,
        PostProcessVolume,
        
        // TODO 其他组件
    };

    /// <summary>
    /// 组件类型 Trait 模板（基础模板，默认为 None）
    /// 每个组件通过特化此模板来关联其 ComponentType
    /// </summary>
    template<typename T>
    struct ComponentTrait
    {
        static constexpr ComponentType Type = ComponentType::None;
    };
}
