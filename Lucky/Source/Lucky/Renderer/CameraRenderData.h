#pragma once

#include "Camera.h"

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 相机渲染数据：SceneRenderer::BeginScene 的相机侧输入
    /// EditorCamera 与 SceneCamera 都被折算成此结构后进入统一渲染路径
    /// </summary>
    struct CameraRenderData
    {
        glm::mat4 ViewMatrix{ 1.0f };                       // 视图矩阵
        glm::mat4 ProjectionMatrix{ 1.0f };                 // 投影矩阵
        glm::vec3 Position{ 0.0f };                         // 相机世界坐标

        // ---- CSM 计算所需（仅透视投影下有意义） ----
        ProjectionType Projection = ProjectionType::Perspective;
        float NearClip = 0.01f;                             // 近裁剪面
        float FOV = 45.0f;                                  // 垂直张角（度）
        float AspectRatio = 1.0f;                           // 宽高比
    };
}
