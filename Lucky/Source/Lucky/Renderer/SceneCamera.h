#pragma once

#include "Camera.h"

namespace Lucky
{
    /// <summary>
    /// 场景相机：ECS 世界中作为 CameraComponent 的一部分
    /// 位置和朝向由同一实体上的 TransformComponent 提供；不响应输入（由脚本控制）
    /// 保留类型区分性以便未来添加 ECS 相机专有字段（如 ClearFlags / CullingMask / ViewportRect 等）
    /// </summary>
    class SceneCamera : public Camera
    {
    };
}
