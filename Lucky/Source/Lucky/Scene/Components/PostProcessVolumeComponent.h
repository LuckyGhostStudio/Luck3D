#pragma once

#include "Lucky/Renderer/RenderContext.h"

namespace Lucky
{
    /// <summary>
    /// 后处理 Volume 组件
    /// 挂载到场景实体上，控制后处理效果的参数
    /// 类似 Unity 的 Volume 组件
    /// </summary>
    struct PostProcessVolumeComponent
    {
        // ---- Volume 设置 ----
        bool IsGlobal = true;       // 是否全局生效（当前仅支持 Global）
        float Priority = 0.0f;      // 优先级（多 Volume 时使用，值越大优先级越高）

        // ---- Tonemapping 参数 ----
        TonemapMode Tonemap = TonemapMode::ACES;    // Tonemapping 模式
        float Exposure = 1.0f;                      // 曝光值

        // ---- Bloom 参数 ----
        bool BloomEnabled = false;      // 是否启用 Bloom
        float BloomThreshold = 1.0f;    // 亮度阈值
        float BloomIntensity = 1.0f;    // 泛光强度
        int BloomIterations = 5;        // 模糊迭代次数

        // ---- FXAA 参数 ----
        bool FXAAEnabled = false;       // 是否启用 FXAA

        // ---- Vignette 参数 ----
        bool VignetteEnabled = false;       // 是否启用暗角
        float VignetteIntensity = 0.5f;     // 暗角强度
        float VignetteSmoothness = 2.0f;    // 暗角平滑度

        PostProcessVolumeComponent() = default;
        PostProcessVolumeComponent(const PostProcessVolumeComponent& other) = default;
    };
}