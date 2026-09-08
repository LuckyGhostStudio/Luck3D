#pragma once

#include "Lucky/Scene/Components/LightComponent.h"

#include <glm/glm.hpp>

namespace Lucky
{
    constexpr static int s_MaxDirectionalLights = 4;
    constexpr static int s_MaxPointLights = 8;
    constexpr static int s_MaxSpotLights = 4;
    constexpr static int s_MaxCascadeCount = 4;     // CSM 最大级联数

    /// <summary>
    /// 方向光 GPU 数据
    /// </summary>
    struct DirectionalLightData
    {
        glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f); // 光照方向（世界空间）
        float Intensity = 1.0f;                             // 光照强度
        glm::vec3 Color = glm::vec3(1.0f);                  // 光照颜色
        char padding[4];                                    // 填充到 16 字节对齐
    };

    /// <summary>
    /// 点光源 GPU 数据
    /// </summary>
    struct PointLightData
    {
        glm::vec3 Position = glm::vec3(0.0f);   // 位置
        float Intensity = 1.0f;                 // 强度
        glm::vec3 Color = glm::vec3(1.0f);      // 颜色
        float Range = 10.0f;                    // 范围
    };

    /// <summary>
    /// 聚光灯 GPU 数据
    /// </summary>
    struct SpotLightData
    {
        glm::vec3 Position = glm::vec3(0.0f);               // 位置
        float Intensity = 1.0f;                             // 强度
        glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f); // 方向
        float Range = 10.0f;                                // 范围
        glm::vec3 Color = glm::vec3(1.0f);                  // 颜色
        float InnerCutoff = 0.9763f;                        // 内锥角（全亮区域）cos(12.5°)
        float OuterCutoff = 0.9537f;                        // 外锥角（衰减到 0 的边界）cos(17.5°)
        char padding[12];                                   // 填充到 16 字节对齐
    };

    /// <summary>
    /// 光照渲染数据：从 Scene 收集后传递给 SceneRenderer
    /// </summary>
    struct LightRenderData
    {
        int DirectionalLightCount = 0;
        DirectionalLightData DirectionalLights[s_MaxDirectionalLights]; // 方向光数组

        int PointLightCount = 0;
        PointLightData PointLights[s_MaxPointLights];                   // 点光源数组

        int SpotLightCount = 0;
        SpotLightData SpotLights[s_MaxSpotLights];                      // 聚光灯数组

        // ======== 每光源阴影参数（支持多光源阴影） ========

        /// <summary>
        /// 方向光阴影参数（每个方向光独立）
        /// </summary>
        struct DirLightShadowParams
        {
            ShadowType Shadows = ShadowType::None;                              // 阴影类型
            float ShadowBias = 0.0003f;                                         // 阴影偏移
            float ShadowStrength = 1.0f;                                        // 阴影强度 [0, 1]
            int CascadeCount = 4;                                               // 级联数量 [1, 4]
            float ShadowDistance = 150.0f;                                      // 阴影最大距离
            float CascadeSplits[s_MaxCascadeCount] = { 0.067f, 0.2f, 0.467f, 1.0f };  // 级联分割比例
            int ShadowMapResolution = 1024;                                     // Atlas 中每级 Tile 分辨率
        };
        DirLightShadowParams DirLightShadows[s_MaxDirectionalLights];

        /// <summary>
        /// 聚光灯阴影参数（每个聚光灯独立）
        /// </summary>
        struct SpotLightShadowParams
        {
            ShadowType Shadows = ShadowType::None;      // 阴影类型
            float ShadowBias = 0.001f;                  // 阴影偏移
            float ShadowStrength = 1.0f;                // 阴影强度 [0, 1]
            int ShadowMapResolution = 512;              // Atlas 中 Tile 分辨率
        };
        SpotLightShadowParams SpotLightShadows[s_MaxSpotLights];

        /// <summary>
        /// 点光源阴影参数（每个点光源独立）
        /// </summary>
        struct PointLightShadowParams
        {
            ShadowType Shadows = ShadowType::None;      // 阴影类型
            float ShadowBias = 0.05f;                   // 阴影偏移（点光源需要更大的 bias）
            float ShadowStrength = 1.0f;                // 阴影强度 [0, 1]
            int ShadowMapResolution = 512;              // Atlas 中每面 Tile 分辨率
        };
        PointLightShadowParams PointLightShadows[s_MaxPointLights];

        // ---- 向后兼容（过渡期保留，R29 完成后删除） ----
        ShadowType DirLightShadowType = ShadowType::None;  // 方向光阴影类型
        float DirLightShadowBias = 0.005f;                  // 方向光阴影偏移
        float DirLightShadowStrength = 1.0f;                // 方向光阴影强度 [0, 1]

        // ---- CSM 参数（过渡期保留，R29 完成后删除） ----
        int CascadeCount = 4;                                                       // 级联数量
        float ShadowDistance = 150.0f;                                              // 阴影最大距离
        float CascadeSplits[s_MaxCascadeCount] = { 0.067f, 0.2f, 0.467f, 1.0f };    // 级联分割比例
        int ShadowMapResolution = 2048;                                             // 每级 Shadow Map 分辨率
    };
}
