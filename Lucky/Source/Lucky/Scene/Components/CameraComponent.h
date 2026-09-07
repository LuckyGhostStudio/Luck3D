#pragma once

#include "Lucky/Renderer/SceneCamera.h"

namespace Lucky
{
    /// <summary>
    /// 相机组件：使实体成为一个可渲染的视角
    /// 位置和朝向由同一实体上的 TransformComponent 提供
    /// </summary>
    struct CameraComponent
    {
        SceneCamera Camera;                     // 场景相机（投影参数）
        bool Primary = false;                   // 是否为主相机（一个场景中最多一个 Primary=true）
        bool FixedAspectRatio = false;          // 是否固定宽高比（true 时 Scene::OnViewportResize 不改动此相机）

        CameraComponent() = default;
        CameraComponent(const CameraComponent& other) = default;
    };
}
