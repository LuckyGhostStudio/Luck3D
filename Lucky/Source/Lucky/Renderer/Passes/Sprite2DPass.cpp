#include "lcpch.h"
#include "Sprite2DPass.h"

#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/RenderState.h"
#include "Lucky/Renderer/Renderer2D.h"

namespace Lucky
{
    void Sprite2DPass::Execute(const RenderContext& context)
    {
        // ---- 提前退出：无 Sprite 命令 ----
        if (!context.SpriteDrawCommands || context.SpriteDrawCommands->empty())
        {
            return;
        }

        // ---- 保证 HDR FBO 已绑定（Main 组内 OpaquePass 已绑定过，兜底再确认一次） ----
        // 与 TransparentPass 的做法一致：不重复绑定，避免多余的 FBO 切换
        // （Pipeline 每个 Pass 后会 ResetDefaultRenderState，但不会解绑 FBO）
        if (context.HDR_FBO)
        {
            context.HDR_FBO->Bind();
        }

        // ---- 设置 2D 渲染状态：半透明混合 + 深度测试 + 不写深度 + 双面可见 ----
        RenderCommand::SetDepthTest(true);
        RenderCommand::SetDepthWrite(false);                            // Sprite 半透明，不写深度
        RenderCommand::SetDepthFunc(DepthCompareFunc::Less);
        RenderCommand::SetBlendMode(BlendMode::SrcAlpha_OneMinusSrcAlpha);
        RenderCommand::SetCullMode(CullMode::Off);                      // Sprite 双面可见

        // ---- 调用 Renderer2D 执行批处理 ----
        // Renderer2D 通过 UBO(binding=0) 读取相机数据，Renderer3D::BeginScene 已上传
        // 此处传入 View/Projection 仅用于 Renderer2D 内部缓存与潜在的 non-UBO 路径
        // 注意：不在此处 ResetStats，由 Scene::OnUpdate 每帧统一 reset（与 Renderer3D 一致）
        Renderer2D::BeginScene(context.CameraViewMatrix, context.CameraProjectionMatrix);

        for (const SpriteDrawCommand& cmd : *context.SpriteDrawCommands)
        {
            Renderer2D::DrawSprite(cmd.Transform, cmd.Sprite, cmd.EntityID);
        }

        Renderer2D::EndScene();

        // ---- 累加 2D 统计到 RenderContext.Stats（Renderer3D 全局统计） ----
        // Quad = 2 三角形，便于与 3D 统计口径对齐（顶点/索引由 Renderer2D::Statistics 内部提供）
        if (context.Stats)
        {
            const Renderer2D::Statistics stats2D = Renderer2D::GetStats();
            context.Stats->DrawCalls     += stats2D.DrawCalls;
            context.Stats->TriangleCount += stats2D.QuadCount * 2;
        }
    }
}
