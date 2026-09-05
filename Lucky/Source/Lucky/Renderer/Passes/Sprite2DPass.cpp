#include "lcpch.h"
#include "Sprite2DPass.h"

#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
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
        if (context.HDR_FBO)
        {
            context.HDR_FBO->Bind();
        }

        // ---- 调用 Renderer2D 执行批处理 ----
        // 渲染状态（Blend/Cull/DepthWrite/DepthTest）由 Renderer2D::Flush 根据当前批次 Material 的 RenderState 设置
        Renderer2D::BeginScene(context.CameraViewMatrix, context.CameraProjectionMatrix);

        for (const SpriteDrawCommand& cmd : *context.SpriteDrawCommands)
        {
            // 按 Material 断批：同 Material 合批，不同 Material 自动 Flush
            Renderer2D::SetBatchMaterial(cmd.MaterialData);

            // 统一走带纹理版 DrawQuad：Texture 为 nullptr 时 Renderer2D 内部会使用槽 0（白色纹理），等价于纯色
            Renderer2D::DrawQuad(cmd.Transform, cmd.Texture, cmd.Color, cmd.UVRect, cmd.TilingFactor, cmd.EntityID);
        }

        Renderer2D::EndScene();

        // ---- 累加 2D 统计到 RenderContext.Stats（Renderer3D 全局统计） ----
        if (context.Stats)
        {
            const Renderer2D::Statistics stats2D = Renderer2D::GetStats();
            context.Stats->DrawCalls     += stats2D.DrawCalls;
            context.Stats->TriangleCount += stats2D.QuadCount * 2;
        }
    }
}
