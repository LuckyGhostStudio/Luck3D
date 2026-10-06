#include "lcpch.h"
#include "AssetPreviewCache.h"

#include "AssetPreviewRenderer.h"

#include "Lucky/Asset/AssetManager.h"
#include "Lucky/Renderer/Framebuffer.h"
#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/Mesh.h"

#include <glad/glad.h>

#include <unordered_map>

namespace Lucky
{
    namespace
    {
        struct PreviewEntry
        {
            Ref<Texture2D> Texture;     // 独立持有的 Texture2D，像素由 Blit 填入
        };

        static std::unordered_map<AssetHandle, PreviewEntry> s_Entries;
        static GLuint s_HelperFBO = 0;      // Blit 用的辅助 FBO，常驻复用

        /// <summary>
        /// 把源 FBO 的 Color Attachment 0 Blit 到目标 Texture2D（Y 翻转）
        /// Y 翻转通过 dstY0 > dstY1 实现，使目标纹理像素行序与 ImGui 常规纹理一致
        /// 辅助 FBO 常驻一张，复用 attach 不同目标纹理
        /// </summary>
        void BlitFramebufferToTexture(const Ref<Framebuffer>& src, const Ref<Texture2D>& dst, uint32_t size)
        {
            if (s_HelperFBO == 0)
            {
                glGenFramebuffers(1, &s_HelperFBO);
            }

            GLuint srcFB = src->GetRendererID();

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_HelperFBO);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst->GetRendererID(), 0);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFB);
            glReadBuffer(GL_COLOR_ATTACHMENT0);

            // Y 翻转：dstY0 = size, dstY1 = 0
            glBlitFramebuffer(
                0, 0, static_cast<GLint>(size), static_cast<GLint>(size),
                0, static_cast<GLint>(size), static_cast<GLint>(size), 0,
                GL_COLOR_BUFFER_BIT, GL_NEAREST);

            // 解绑辅助 FBO 的目标纹理，避免下次 attach 时读到脏引用
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    void AssetPreviewCache::Init()
    {
        // 当前无额外初始化状态，预留接口与 Renderer 对齐
    }

    void AssetPreviewCache::Shutdown()
    {
        Clear();
        if (s_HelperFBO != 0)
        {
            glDeleteFramebuffers(1, &s_HelperFBO);
            s_HelperFBO = 0;
        }
    }

    const Ref<Texture2D>& AssetPreviewCache::GetOrRender(AssetHandle handle, AssetType type)
    {
        static Ref<Texture2D> s_Null;

        if (!handle.IsValid())
        {
            return s_Null;
        }
        if (type != AssetType::Material && type != AssetType::Mesh)
        {
            return s_Null;
        }

        auto it = s_Entries.find(handle);
        if (it != s_Entries.end() && it->second.Texture)
        {
            return it->second.Texture;
        }

        // miss：按类型分派渲染
        Ref<Framebuffer> srcFB;
        if (type == AssetType::Material)
        {
            Ref<Material> material = AssetManager::GetAsset<Material>(handle);
            srcFB = AssetPreviewRenderer::RenderMaterial(material);
        }
        else // Mesh
        {
            Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(handle);
            srcFB = AssetPreviewRenderer::RenderMesh(mesh);
        }

        if (!srcFB)
        {
            return s_Null;
        }

        uint32_t size = AssetPreviewRenderer::GetPreviewSize();
        Ref<Texture2D> target = Texture2D::Create(size, size);
        BlitFramebufferToTexture(srcFB, target, size);

        PreviewEntry& entry = s_Entries[handle];
        entry.Texture = target;
        return entry.Texture;
    }

    void AssetPreviewCache::Invalidate(AssetHandle handle)
    {
        s_Entries.erase(handle);
    }

    void AssetPreviewCache::Clear()
    {
        s_Entries.clear();
    }
}
