#pragma once

#include "Lucky/Core/Base.h"
#include "Framebuffer.h"

#include <glm/glm.hpp>
#include <vector>

namespace Lucky
{
    /// <summary>
    /// Shadow Atlas 中的一个 Tile（分配给某个光源的某个面/级联）
    /// 描述该 Tile 在 Atlas 纹理中的位置和对应的光源空间矩阵
    /// </summary>
    struct ShadowAtlasTile
    {
        int X = 0;                              // Atlas 中的像素 X 偏移
        int Y = 0;                              // Atlas 中的像素 Y 偏移
        int Width = 0;                          // Tile 宽度（像素）
        int Height = 0;                         // Tile 高度（像素）
        glm::vec4 ViewportScaleBias;            // xy = scale, zw = bias（用于 Shader 中 UV 变换）
        glm::mat4 LightSpaceMatrix;             // 光源空间 VP 矩阵
        bool Valid = false;                     // 是否有效（已分配且已计算矩阵）
    };

    /// <summary>
    /// 光源阴影信息：描述一个光源在 Atlas 中的所有 Tile
    /// </summary>
    struct ShadowLightInfo
    {
        /// <summary>
        /// 光源阴影类型
        /// </summary>
        enum class Type
        {
            Directional,    // 方向光（CSM，多个 Tile）
            Spot,           // 聚光灯（单个 Tile）
            Point           // 点光源（6 个 Tile）
        };

        Type LightType = Type::Directional;     // 光源类型
        int LightIndex = -1;                    // 在对应光源数组中的索引

        // ---- 阴影参数 ----
        float ShadowBias = 0.0003f;             // 阴影偏移
        float ShadowStrength = 1.0f;            // 阴影强度 [0, 1]
        int ShadowType = 1;                     // 1=Hard, 2=Soft

        // ---- 方向光 CSM 专用 ----
        int CascadeCount = 4;                   // 级联数量
        float CascadeFarPlanes[4] = { 0.0f };   // 每级远平面距离（视图空间）

        // ---- 点光源专用 ----
        float FarPlane = 25.0f;                 // 点光源阴影远平面（= Range）

        // ---- Tile 索引 ----
        int TileStartIndex = -1;                // 在 m_Tiles 中的起始索引
        int TileCount = 0;                      // Tile 数量（方向光=CascadeCount, 聚光灯=1, 点光源=6）
    };

    /// <summary>
    /// Shadow Atlas 管理器
    /// 将所有光源的 Shadow Map 打包到一张大的 2D 深度纹理中
    /// 使用固定分区布局：方向光区域 + 聚光灯区域 + 点光源区域
    /// </summary>
    class ShadowAtlas
    {
    public:
        // ---- 容量常量 ----
        static constexpr int s_MaxDirLightShadows = 2;      // 最多 2 个方向光投射阴影
        static constexpr int s_MaxSpotLightShadows = 4;     // 最多 4 个聚光灯投射阴影
        static constexpr int s_MaxPointLightShadows = 2;    // 最多 2 个点光源投射阴影
        static constexpr int s_MaxCascadeCount = 4;         // CSM 最大级联数

        /// <summary>
        /// 初始化 Shadow Atlas（创建 FBO 和分配固定 Tile 布局）
        /// </summary>
        /// <param name="atlasSize">Atlas 纹理尺寸（正方形，默认 4096）</param>
        void Init(uint32_t atlasSize = 4096);

        /// <summary>
        /// 每帧开始时重置所有 Tile 的有效状态
        /// 不重新分配位置（固定布局），仅清除 Valid 标志和矩阵数据
        /// </summary>
        void BeginFrame();

        /// <summary>
        /// 激活方向光阴影 Tile（设置 LightSpaceMatrix 和 Valid 标志）
        /// </summary>
        /// <param name="shadowIndex">方向光阴影索引 [0, s_MaxDirLightShadows)</param>
        /// <param name="cascadeIndex">级联索引 [0, cascadeCount)</param>
        /// <param name="lightSpaceMatrix">该级联的 Light Space Matrix</param>
        void ActivateDirectionalTile(int shadowIndex, int cascadeIndex, const glm::mat4& lightSpaceMatrix);

        /// <summary>
        /// 激活聚光灯阴影 Tile
        /// </summary>
        /// <param name="shadowIndex">聚光灯阴影索引 [0, s_MaxSpotLightShadows)</param>
        /// <param name="lightSpaceMatrix">聚光灯的 Light Space Matrix</param>
        void ActivateSpotTile(int shadowIndex, const glm::mat4& lightSpaceMatrix);

        /// <summary>
        /// 激活点光源阴影 Tile（指定面）
        /// </summary>
        /// <param name="shadowIndex">点光源阴影索引 [0, s_MaxPointLightShadows)</param>
        /// <param name="faceIndex">Cubemap 面索引 [0, 5]（+X, -X, +Y, -Y, +Z, -Z）</param>
        /// <param name="lightSpaceMatrix">该面的 Light Space Matrix</param>
        void ActivatePointTile(int shadowIndex, int faceIndex, const glm::mat4& lightSpaceMatrix);

        // ---- 查询接口 ----

        /// <summary>
        /// 获取 Atlas 深度纹理 ID（用于绑定到纹理单元）
        /// </summary>
        uint32_t GetAtlasTextureID() const;

        /// <summary>
        /// 获取 Atlas FBO（用于渲染时绑定）
        /// </summary>
        const Ref<Framebuffer>& GetFramebuffer() const { return m_AtlasFBO; }

        /// <summary>
        /// 获取 Atlas 纹理尺寸
        /// </summary>
        uint32_t GetAtlasSize() const { return m_AtlasSize; }

        /// <summary>
        /// 获取指定 Tile
        /// </summary>
        const ShadowAtlasTile& GetTile(int index) const { return m_Tiles[index]; }
        ShadowAtlasTile& GetTile(int index) { return m_Tiles[index]; }

        /// <summary>
        /// 获取所有 Tile（只读）
        /// </summary>
        const std::vector<ShadowAtlasTile>& GetTiles() const { return m_Tiles; }

        /// <summary>
        /// 获取所有光源阴影信息（只读）
        /// </summary>
        const std::vector<ShadowLightInfo>& GetLightInfos() const { return m_LightInfos; }

        /// <summary>
        /// 获取指定光源阴影信息（可写）
        /// </summary>
        ShadowLightInfo& GetLightInfo(int index) { return m_LightInfos[index]; }
        const ShadowLightInfo& GetLightInfo(int index) const { return m_LightInfos[index]; }

        // ---- 固定布局索引查询 ----

        /// <summary>
        /// 获取方向光 CSM 的 Tile 起始索引
        /// </summary>
        int GetDirLightTileStart(int shadowIndex) const;

        /// <summary>
        /// 获取聚光灯的 Tile 索引
        /// </summary>
        int GetSpotLightTileIndex(int shadowIndex) const;

        /// <summary>
        /// 获取点光源的 Tile 起始索引
        /// </summary>
        int GetPointLightTileStart(int shadowIndex) const;

    private:
        /// <summary>
        /// 初始化固定分区布局（计算所有 Tile 的位置和 ViewportScaleBias）
        /// </summary>
        void InitFixedLayout();

        Ref<Framebuffer> m_AtlasFBO;                    // Atlas 深度 FBO（单张大 2D 深度纹理）
        uint32_t m_AtlasSize = 4096;                    // Atlas 纹理尺寸

        std::vector<ShadowAtlasTile> m_Tiles;           // 所有预分配的 Tile
        std::vector<ShadowLightInfo> m_LightInfos;      // 所有光源阴影信息槽位

        // ---- 固定布局参数 ----
        static constexpr int s_DirTileSize = 1024;      // 方向光每级 Tile 尺寸
        static constexpr int s_SpotTileSize = 512;      // 聚光灯 Tile 尺寸
        static constexpr int s_PointTileSize = 512;     // 点光源每面 Tile 尺寸
    };
}
