#include "lcpch.h"
#include "RenderCommand.h"

#include <glad/glad.h>

namespace Lucky
{
    void RenderCommand::Init()
    {
        // 设置默认渲染状态
        ResetDefaultRenderState();

        // 一次性初始化设置（不属于每帧恢复的渲染状态）
        glEnable(GL_LINE_SMOOTH);
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);     // 启用 Cubemap 无缝采样（消除 Cubemap 六面接缝）
    }

    void RenderCommand::SetClearColor(const glm::vec4& color)
    {
        glClearColor(color.r, color.g, color.b, color.a);   // 设置清屏颜色
    }

    void RenderCommand::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        glViewport(x, y, width, height);    // 设置视口大小
    }

    void RenderCommand::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // 清除 颜色缓冲区 | 深度缓冲区
    }

    void RenderCommand::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount)
    {
        vertexArray->Bind();
        uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();

        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);  // 顶点数组索引绘制三角形
    }

    void RenderCommand::DrawIndexedRange(const Ref<VertexArray>& vertexArray, uint32_t indexOffset, uint32_t indexCount)
    {
        vertexArray->Bind();
        
        void* byteOffset = (void*)(indexOffset * sizeof(uint32_t));   // 计算字节偏移：indexOffset 是索引单位，乘以 sizeof(uint32_t) 得到字节偏移
        
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, byteOffset);
    }

    void RenderCommand::DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
    {
        vertexArray->Bind();

        glDrawArrays(GL_LINES, 0, vertexCount); // 绘制直线
    }

    void RenderCommand::DrawArrays(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
    {
        vertexArray->Bind();

        glDrawArrays(GL_TRIANGLES, 0, vertexCount); // 绘制三角形（无索引）
    }

    void RenderCommand::SetLineWidth(float width)
    {
        glLineWidth(width);
    }

    // ---- 渲染状态控制 ----

    void RenderCommand::SetCullMode(CullMode mode)
    {
        if (mode == CullMode::Off)
        {
            glDisable(GL_CULL_FACE);
        }
        else
        {
            glEnable(GL_CULL_FACE);
            glCullFace(mode == CullMode::Back ? GL_BACK : GL_FRONT);
        }
    }

    void RenderCommand::SetDepthTest(bool enable)
    {
        if (enable)
        {
            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
    }

    void RenderCommand::SetDepthWrite(bool enable)
    {
        glDepthMask(enable ? GL_TRUE : GL_FALSE);
    }

    void RenderCommand::SetDepthFunc(DepthCompareFunc func)
    {
        GLenum glFunc;
        switch (func)
        {
            case DepthCompareFunc::Less:         glFunc = GL_LESS;     break;
            case DepthCompareFunc::LessEqual:    glFunc = GL_LEQUAL;   break;
            case DepthCompareFunc::Greater:      glFunc = GL_GREATER;  break;
            case DepthCompareFunc::GreaterEqual: glFunc = GL_GEQUAL;   break;
            case DepthCompareFunc::Equal:        glFunc = GL_EQUAL;    break;
            case DepthCompareFunc::NotEqual:     glFunc = GL_NOTEQUAL; break;
            case DepthCompareFunc::Always:       glFunc = GL_ALWAYS;   break;
            case DepthCompareFunc::Never:        glFunc = GL_NEVER;    break;
            default:                             glFunc = GL_LESS;     break;
        }
        glDepthFunc(glFunc);
    }

    void RenderCommand::SetBlendMode(BlendMode mode)
    {
        if (mode == BlendMode::None)
        {
            glDisable(GL_BLEND);
        }
        else
        {
            glEnable(GL_BLEND);
            switch (mode)
            {
                case BlendMode::SrcAlpha_OneMinusSrcAlpha:
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    break;
                case BlendMode::One_One:
                    glBlendFunc(GL_ONE, GL_ONE);
                    break;
                case BlendMode::SrcAlpha_One:
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                    break;
                case BlendMode::Zero_SrcColor:
                    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
                    break;
                default:
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    break;
            }
        }
    }

    void RenderCommand::SetColorMask(bool r, bool g, bool b, bool a)
    {
        glColorMask(r ? GL_TRUE : GL_FALSE, g ? GL_TRUE : GL_FALSE, b ? GL_TRUE : GL_FALSE, a ? GL_TRUE : GL_FALSE);
    }

    void RenderCommand::SetDrawBuffers(const uint32_t* attachments, uint32_t count)
    {
        glDrawBuffers(count, attachments);
    }

    void RenderCommand::BindTextureUnit(uint32_t slot, uint32_t textureID)
    {
        // 使用 OpenGL 4.5 DSA API：自动识别纹理类型（GL_TEXTURE_2D / GL_TEXTURE_2D_ARRAY 等）
        // 无需手动指定纹理目标，解决 Texture2DArray 绑定失败的问题
        glBindTextureUnit(slot, textureID);
    }

    void RenderCommand::SetScissorTest(bool enable)
    {
        if (enable)
        {
            glEnable(GL_SCISSOR_TEST);
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
        }
    }

    void RenderCommand::SetScissor(int x, int y, int width, int height)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, width, height);
    }

    void RenderCommand::ResetDefaultRenderState()
    {
        SetCullMode(CullMode::Back);            // 背面剔除
        SetDepthTest(true);                     // 开启深度测试
        SetDepthWrite(true);                    // 开启深度写入
        SetDepthFunc(DepthCompareFunc::Less);   // 深度比较函数：Less
        SetBlendMode(BlendMode::None);          // 关闭混合
        SetColorMask(true, true, true, true);   // 开启所有颜色通道写入
        SetScissorTest(false);                  // 关闭裁剪测试
    }
}