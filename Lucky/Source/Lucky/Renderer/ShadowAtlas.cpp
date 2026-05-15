#include "lcpch.h"
#include "ShadowAtlas.h"

#include "Lucky/Renderer/RenderCommand.h"

namespace Lucky
{
    void ShadowAtlas::Init(uint32_t atlasSize)
    {
        m_AtlasSize = atlasSize;

        // 创建 Atlas FBO（单张大 2D 深度纹理）
        FramebufferSpecification atlasSpec;
        atlasSpec.Width = m_AtlasSize;
        atlasSpec.Height = m_AtlasSize;
        atlasSpec.Attachments = {
            FramebufferTextureFormat::DEPTH_COMPONENT
        };
        m_AtlasFBO = Framebuffer::Create(atlasSpec);

        // 初始化固定分区布局
        InitFixedLayout();
    }

    void ShadowAtlas::InitFixedLayout()
    {
        // 计算总 Tile 数量
        // 方向光：s_MaxDirLightShadows * s_MaxCascadeCount = 2 * 4 = 8
        // 聚光灯：s_MaxSpotLightShadows = 4
        // 点光源：s_MaxPointLightShadows * 6 = 2 * 6 = 12
        // 总计：24 个 Tile
        int totalTiles = s_MaxDirLightShadows * s_MaxCascadeCount
                       + s_MaxSpotLightShadows
                       + s_MaxPointLightShadows * 6;
        m_Tiles.resize(totalTiles);

        float invAtlasSize = 1.0f / static_cast<float>(m_AtlasSize);

        // ======== 方向光区域 ========
        // 布局：2 列 × 4 行，每个 Tile 1024×1024
        // 方向光 0：列 0（x=0），方向光 1：列 1（x=1024）
        int tileIndex = 0;
        for (int dirIdx = 0; dirIdx < s_MaxDirLightShadows; ++dirIdx)
        {
            for (int cascade = 0; cascade < s_MaxCascadeCount; ++cascade)
            {
                ShadowAtlasTile& tile = m_Tiles[tileIndex];
                tile.X = dirIdx * s_DirTileSize;
                tile.Y = cascade * s_DirTileSize;
                tile.Width = s_DirTileSize;
                tile.Height = s_DirTileSize;
                tile.ViewportScaleBias = glm::vec4(
                    tile.Width * invAtlasSize,      // scaleX
                    tile.Height * invAtlasSize,     // scaleY
                    tile.X * invAtlasSize,          // biasX
                    tile.Y * invAtlasSize           // biasY
                );
                tile.Valid = false;
                ++tileIndex;
            }
        }

        // ======== 聚光灯区域 ========
        // 布局：4 个 Tile 水平排列，放在 x=2048~4096, y=0~512 区域
        for (int spotIdx = 0; spotIdx < s_MaxSpotLightShadows; ++spotIdx)
        {
            ShadowAtlasTile& tile = m_Tiles[tileIndex];
            tile.X = 2048 + spotIdx * s_SpotTileSize;
            tile.Y = 0;
            tile.Width = s_SpotTileSize;
            tile.Height = s_SpotTileSize;
            tile.ViewportScaleBias = glm::vec4(
                tile.Width * invAtlasSize,
                tile.Height * invAtlasSize,
                tile.X * invAtlasSize,
                tile.Y * invAtlasSize
            );
            tile.Valid = false;
            ++tileIndex;
        }

        // ======== 点光源区域 ========
        // 布局：每个点光源 6 面，每行放 4 个面，需要 2 行
        // 点光源 0：x=2048~4096, y=512~1536
        // 点光源 1：x=2048~4096, y=1536~2560
        for (int pointIdx = 0; pointIdx < s_MaxPointLightShadows; ++pointIdx)
        {
            for (int face = 0; face < 6; ++face)
            {
                ShadowAtlasTile& tile = m_Tiles[tileIndex];
                int col = face % 4;
                int row = face / 4;
                tile.X = 2048 + col * s_PointTileSize;
                tile.Y = 512 + pointIdx * s_PointTileSize * 2 + row * s_PointTileSize;
                tile.Width = s_PointTileSize;
                tile.Height = s_PointTileSize;
                tile.ViewportScaleBias = glm::vec4(
                    tile.Width * invAtlasSize,
                    tile.Height * invAtlasSize,
                    tile.X * invAtlasSize,
                    tile.Y * invAtlasSize
                );
                tile.Valid = false;
                ++tileIndex;
            }
        }

        // ======== 初始化光源阴影信息槽位 ========
        int totalLightInfos = s_MaxDirLightShadows + s_MaxSpotLightShadows + s_MaxPointLightShadows;
        m_LightInfos.resize(totalLightInfos);

        int infoIndex = 0;
        int currentTileStart = 0;

        // 方向光槽位
        for (int i = 0; i < s_MaxDirLightShadows; ++i)
        {
            m_LightInfos[infoIndex].LightType = ShadowLightInfo::Type::Directional;
            m_LightInfos[infoIndex].TileStartIndex = currentTileStart;
            m_LightInfos[infoIndex].TileCount = s_MaxCascadeCount;
            currentTileStart += s_MaxCascadeCount;
            ++infoIndex;
        }

        // 聚光灯槽位
        for (int i = 0; i < s_MaxSpotLightShadows; ++i)
        {
            m_LightInfos[infoIndex].LightType = ShadowLightInfo::Type::Spot;
            m_LightInfos[infoIndex].TileStartIndex = currentTileStart;
            m_LightInfos[infoIndex].TileCount = 1;
            currentTileStart += 1;
            ++infoIndex;
        }

        // 点光源槽位
        for (int i = 0; i < s_MaxPointLightShadows; ++i)
        {
            m_LightInfos[infoIndex].LightType = ShadowLightInfo::Type::Point;
            m_LightInfos[infoIndex].TileStartIndex = currentTileStart;
            m_LightInfos[infoIndex].TileCount = 6;
            currentTileStart += 6;
            ++infoIndex;
        }
    }

    void ShadowAtlas::BeginFrame()
    {
        // 重置所有 Tile 的有效状态（位置不变，固定布局）
        for (auto& tile : m_Tiles)
        {
            tile.Valid = false;
            tile.LightSpaceMatrix = glm::mat4(1.0f);
        }

        // 重置所有光源信息的索引
        for (auto& info : m_LightInfos)
        {
            info.LightIndex = -1;
        }
    }

    void ShadowAtlas::ActivateDirectionalTile(int shadowIndex, int cascadeIndex, const glm::mat4& lightSpaceMatrix)
    {
        LF_CORE_ASSERT(shadowIndex >= 0 && shadowIndex < s_MaxDirLightShadows, "shadowIndex 越界！");
        LF_CORE_ASSERT(cascadeIndex >= 0 && cascadeIndex < s_MaxCascadeCount, "cascadeIndex 越界！");

        int tileIdx = GetDirLightTileStart(shadowIndex) + cascadeIndex;
        m_Tiles[tileIdx].LightSpaceMatrix = lightSpaceMatrix;
        m_Tiles[tileIdx].Valid = true;
    }

    void ShadowAtlas::ActivateSpotTile(int shadowIndex, const glm::mat4& lightSpaceMatrix)
    {
        LF_CORE_ASSERT(shadowIndex >= 0 && shadowIndex < s_MaxSpotLightShadows, "shadowIndex 越界！");

        int tileIdx = GetSpotLightTileIndex(shadowIndex);
        m_Tiles[tileIdx].LightSpaceMatrix = lightSpaceMatrix;
        m_Tiles[tileIdx].Valid = true;
    }

    void ShadowAtlas::ActivatePointTile(int shadowIndex, int faceIndex, const glm::mat4& lightSpaceMatrix)
    {
        LF_CORE_ASSERT(shadowIndex >= 0 && shadowIndex < s_MaxPointLightShadows, "shadowIndex 越界！");
        LF_CORE_ASSERT(faceIndex >= 0 && faceIndex < 6, "faceIndex 越界！");

        int tileIdx = GetPointLightTileStart(shadowIndex) + faceIndex;
        m_Tiles[tileIdx].LightSpaceMatrix = lightSpaceMatrix;
        m_Tiles[tileIdx].Valid = true;
    }

    uint32_t ShadowAtlas::GetAtlasTextureID() const
    {
        return m_AtlasFBO->GetDepthAttachmentRendererID();
    }

    int ShadowAtlas::GetDirLightTileStart(int shadowIndex) const
    {
        return shadowIndex * s_MaxCascadeCount;
    }

    int ShadowAtlas::GetSpotLightTileIndex(int shadowIndex) const
    {
        return s_MaxDirLightShadows * s_MaxCascadeCount + shadowIndex;
    }

    int ShadowAtlas::GetPointLightTileStart(int shadowIndex) const
    {
        return s_MaxDirLightShadows * s_MaxCascadeCount + s_MaxSpotLightShadows + shadowIndex * 6;
    }
}
