#pragma once

#include <cstdint>

namespace Lucky
{
    /// <summary>
    /// 面剔除模式
    /// </summary>
    enum class CullMode : uint8_t
    {
        Back = 0,   // 剔除背面（默认，适用于大多数不透明物体）
        Front,      // 剔除正面（特殊效果，如描边 Pass 的膨胀法）
        Off         // 不剔除（双面渲染，适用于树叶、布料、纸片等薄物体）
    };

    /// <summary>
    /// 深度测试比较函数
    /// </summary>
    enum class DepthCompareFunc : uint8_t
    {
        Less = 0,       // <（OpenGL 默认）
        LessEqual,      // <=（Unity 默认）
        Greater,        // >（被遮挡时才绘制，用于轮廓效果）
        GreaterEqual,   // >=
        Equal,          // ==（贴花 Decal）
        NotEqual,       // !=
        Always,         // 总是通过（忽略深度，UI 叠加）
        Never           // 永不通过（调试用）
    };

    /// <summary>
    /// 混合模式
    /// </summary>
    enum class BlendMode : uint8_t
    {
        None = 0,                       // 不混合（不透明物体）
        SrcAlpha_OneMinusSrcAlpha,      // 标准 Alpha 混合（半透明，最常用）
        One_One,                        // 叠加混合（粒子、发光效果）
        SrcAlpha_One                    // 预乘 Alpha 叠加
    };

    /// <summary>
    /// 渲染模式预设（类似 Unity Standard Shader 的 RenderingMode）
    /// 选择后会自动设置 BlendMode、DepthWrite、RenderQueue 等状态
    /// </summary>
    enum class RenderingMode : uint8_t
    {
        Opaque = 0,     // 不透明（默认）
        Cutout,         // Alpha 裁剪（树叶、铁丝网）
        Fade,           // 整体淡入淡出（全息投影、幽灵）
        Transparent     // 半透明（玻璃、水面）
    };

    /// <summary>
    /// 渲染队列预定义值（与 Unity 一致）
    /// </summary>
    namespace RenderQueue
    {
        constexpr int32_t Background  = 1000;   // 天空盒等最先绘制的物体
        constexpr int32_t Geometry    = 2000;   // 默认不透明物体
        constexpr int32_t AlphaTest   = 2450;   // Alpha 测试物体
        constexpr int32_t Transparent = 3000;   // 透明物体
        constexpr int32_t Overlay     = 4000;   // UI 叠加层等最后绘制的物体
        
        /// <summary>
        /// 判断是否为透明区域（Queue >= 2500）
        /// </summary>
        constexpr bool IsTransparent(int32_t queue) { return queue >= 2500; }
    }

    /// <summary>
    /// DrawBuffer 目标常量（与 OpenGL GL_COLOR_ATTACHMENTx / GL_NONE 值一致）
    /// 用于 RenderCommand::SetDrawBuffers()，避免 Pass 中直接依赖 glad.h
    /// </summary>
    namespace DrawBuffer
    {
        constexpr uint32_t None         = 0x0;      // GL_NONE
        constexpr uint32_t Attachment0  = 0x8CE0;   // GL_COLOR_ATTACHMENT0
        constexpr uint32_t Attachment1  = 0x8CE1;   // GL_COLOR_ATTACHMENT1
        constexpr uint32_t Attachment2  = 0x8CE2;   // GL_COLOR_ATTACHMENT2
        constexpr uint32_t Attachment3  = 0x8CE3;   // GL_COLOR_ATTACHMENT3
    }

    /// <summary>
    /// 材质渲染状态（Per-Material）
    /// 每个材质持有一个 RenderState，控制该材质的 GPU 渲染状态
    /// </summary>
    struct RenderState
    {
        CullMode Cull = CullMode::Back;                             // 面剔除模式
        bool DepthWrite = true;                                     // 深度写入
        DepthCompareFunc DepthTest = DepthCompareFunc::Less;        // 深度测试比较函数
        BlendMode Blend = BlendMode::None;                          // 混合模式
        int32_t Queue = RenderQueue::Geometry;                      // 渲染队列值
        
        /// <summary>
        /// 判断是否为透明物体
        /// </summary>
        bool IsTransparent() const { return RenderQueue::IsTransparent(Queue); }
        
        /// <summary>
        /// 比较两个 RenderState 是否相同（用于状态跟踪，避免重复设置）
        /// </summary>
        bool operator==(const RenderState& other) const
        {
            return Cull == other.Cull
                && DepthWrite == other.DepthWrite
                && DepthTest == other.DepthTest
                && Blend == other.Blend
                && Queue == other.Queue;
        }
        
        bool operator!=(const RenderState& other) const { return !(*this == other); }
    };

    /// <summary>
    /// 根据 RenderingMode 预设自动配置 RenderState
    /// </summary>
    /// <param name="state">要配置的 RenderState</param>
    /// <param name="mode">渲染模式预设</param>
    inline void ApplyRenderingMode(RenderState& state, RenderingMode mode)
    {
        switch (mode)
        {
            case RenderingMode::Opaque:
                state.Blend = BlendMode::None;
                state.DepthWrite = true;
                state.Queue = RenderQueue::Geometry;
                break;
                
            case RenderingMode::Cutout:
                state.Blend = BlendMode::None;
                state.DepthWrite = true;
                state.Queue = RenderQueue::AlphaTest;
                break;
                
            case RenderingMode::Fade:
                state.Blend = BlendMode::SrcAlpha_OneMinusSrcAlpha;
                state.DepthWrite = false;
                state.Queue = RenderQueue::Transparent;
                break;
                
            case RenderingMode::Transparent:
                state.Blend = BlendMode::SrcAlpha_OneMinusSrcAlpha;
                state.DepthWrite = false;
                state.Queue = RenderQueue::Transparent;
                break;
        }
        // 注意：CullMode 不受 RenderingMode 影响，需要单独设置
    }
}
