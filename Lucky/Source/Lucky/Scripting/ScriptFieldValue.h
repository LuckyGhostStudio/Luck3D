#pragma once

#include "Lucky/Scripting/ScriptFieldType.h"

#include "Lucky/Core/Base.h"            // Ref<T> 的出处；必须显式加，不依赖 PCH 传递包含
#include "Lucky/Core/UUID.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>       // glm::quat 不在 glm/glm.hpp 里，必须显式加（YamlHelpers.h / TransformComponent.h 都是这么做的）
#include <variant>
#include <string>

namespace Lucky
{
    class Asset;    // 前向声明即可：Ref<Asset>（shared_ptr）不需要完整类型；避免拖入 Asset.h（它的 AssetHandle.h 带着 yaml-cpp）

    /// <summary>
    /// 字段值的载荷：受支持类型的并集
    /// Color 与 Vector4 共用 glm::vec4、Entity 落在 UUID，
    /// 因此载荷本身不足以区分类型，必须结合 ScriptFieldValue::Type 使用
    /// </summary>
    using ScriptFieldScalar = std::variant<
        bool,
        int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t,
        float, double,
        std::string,
        glm::vec2, glm::vec3, glm::vec4, glm::quat,
        UUID,
        Ref<Asset>
    >;

    /// <summary>
    /// 脚本字段值：一个字段的取值
    /// 使用约定：Type 是权威 —— 先读 Type 判断有效载荷种类，再用 std::get<T> 取对应的值
    /// 不要依赖"载荷里当前恰好是哪个 alternative"
    /// </summary>
    struct ScriptFieldValue
    {
        ScriptFieldType  Type = ScriptFieldType::None;
        ScriptFieldScalar Data;
    };
}
