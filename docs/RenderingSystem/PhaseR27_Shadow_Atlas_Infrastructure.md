# Phase R27：Shadow Atlas 基础架构

> **创建日期**：2026-05-15
> **前置依赖**：R4（基础阴影）、R18（CSM）、R19（统一 LightComponent）、R21（Translucent Shadow Map）
> **目标**：建立统一的 Shadow Atlas 架构，将所有光源的 Shadow Map 打包到一张大深度纹理中，为多光源阴影提供基础设施
> **后续文档**：R28（聚光灯阴影）、R29（点光源阴影）

---

## 一、背景与动机

### 1.1 当前系统现状

当前阴影系统仅支持**单个方向光**的 CSM 阴影投射：

| 能力 | 状态 | 实现位置 |
|------|------|----------|
| 方向光 CSM（4 级联） | ? 已实现 | `ShadowPass.cpp` + `Shadow.glsl` |
| Hard/Soft 阴影切换 | ? 已实现 | `Shadow.glsl`（PCF 5×5） |
| Translucent Shadow Map | ? 已实现 | `ShadowPass.cpp`（RGBA8 Array） |
| 多方向光阴影 | ? 仅第一个方向光 | `Lighting.glsl` 中 `i == 0` 硬编码 |
| 聚光灯阴影 | ? 未实现 | — |
| 点光源阴影 | ? 未实现 | — |

### 1.2 当前架构限制

1. **单光源绑定**：`RenderContext` 中只有一组 CSM 数据（`CascadeLightSpaceMatrices[4]`）
2. **硬编码索引**：`Lighting.glsl` 中 `if (i == 0 && u_ShadowEnabled != 0)` 仅对第一个方向光应用阴影
3. **独立 FBO**：CSM 使用独立的 `Texture2DArray` FBO，无法扩展到多光源
4. **纹理单元紧张**：如果每个光源独立绑定 Shadow Map，纹理单元很快耗尽

### 1.3 Shadow Atlas 方案优势

**Shadow Atlas**（阴影图集）将所有光源的 Shadow Map 打包到一张大的 2D 深度纹理中：

- 只需绑定 **1 个纹理单元**即可支持所有光源的阴影
- 统一的阴影数据管理和分配策略
- 便于实现动态分辨率分配（重要光源分配更大区域）
- 扩展性好，新增光源类型无需额外纹理单元

### 1.4 当前纹理单元使用情况

| 纹理单元 | 用途 | 使用者 |
|----------|------|--------|
| 0-7 | 材质纹理（Albedo、Normal、Metallic 等） | `Material::Apply()` |
| 8 | Translucent Shadow Map Array | `OpaquePass` / `TransparentPass` |
| 10 | Irradiance Cubemap（IBL） | `OpaquePass` / `TransparentPass` |
| 11 | Prefilter Cubemap（IBL） | `OpaquePass` / `TransparentPass` |
| 12 | BRDF LUT（IBL） | `OpaquePass` / `TransparentPass` |
| 15 | CSM Depth Texture2DArray | `OpaquePass` / `TransparentPass` |

**变更计划**：纹理单元 15 从 `sampler2DArray`（CSM）变为 `sampler2D`（Shadow Atlas）。

---

## 二、Atlas 布局设计

### 2.1 方案对比

#### 方案 A：固定分区布局（推荐 ?????）

将 Atlas 划分为固定的功能区域，每种光源类型占据预定义的区域。

```
┌─────────────────────────────────────────────────────────────────┐
│                    Shadow Atlas (4096×4096)                       │
├───────────────────────────────┬───────────────────────────────────┤
│                               │                                   │
│   Directional Light 0         │   Directional Light 1             │
│   CSM Cascade 0 (1024×1024)   │   CSM Cascade 0 (1024×1024)      │
│                               │                                   │
├───────────────────────────────┼───────────────────────────────────┤
│                               │                                   │
│   CSM Cascade 1 (1024×1024)   │   CSM Cascade 1 (1024×1024)      │
│                               │                                   │
├───────────────────────────────┼───────────────────────────────────┤
│                               │                                   │
│   CSM Cascade 2 (1024×1024)   │   CSM Cascade 2 (1024×1024)      │
│                               │                                   │
├───────────────────────────────┼───────────────────────────────────┤
│                               │                                   │
│   CSM Cascade 3 (1024×1024)   │   CSM Cascade 3 (1024×1024)      │
│                               │                                   │
├───────────────┬───────────────┼───────────┬───────────┬───────────┤
│  Spot 0       │  Spot 1       │  Spot 2   │  Spot 3   │  (空闲)   │
│  (512×512)    │  (512×512)    │  (512×512)│  (512×512)│           │
├───────────────┼───────────────┼───────────┼───────────┼───────────┤
│  Point 0 (+X) │  Point 0 (-X) │  Pt0 (+Y) │  Pt0 (-Y) │  Pt0 (+Z)│
│  (512×512)    │  (512×512)    │  (512×512)│  (512×512)│  (512×512)│
├───────────────┼───────────────┼───────────┼───────────┼───────────┤
│  Point 0 (-Z) │  Point 1 (+X) │  Pt1 (-X) │  Pt1 (+Y) │  Pt1 (-Y)│
│  (512×512)    │  (512×512)    │  (512×512)│  (512×512)│  (512×512)│
├───────────────┼───────────────┼───────────┼───────────┼───────────┤
│  Point 1 (+Z) │  Point 1 (-Z) │           │           │           │
│  (512×512)    │  (512×512)    │  (空闲)    │  (空闲)   │  (空闲)   │
└───────────────┴───────────────┴───────────┴───────────┴───────────┘
```

**优点**：
- 实现简单，无需复杂的分配算法
- 每帧 Tile 位置固定，无需重新计算 UV 变换
- 调试直观，可以直接查看 Atlas 纹理

**缺点**：
- 空间利用率不是最优（未使用的 Tile 浪费空间）
- 不够灵活（光源数量超出预设时无法动态扩展）

#### 方案 B：动态 Shelf 分配

每帧根据实际需要动态分配 Tile，使用 Shelf-First-Fit 算法。

**优点**：
- 空间利用率高
- 灵活适应不同场景

**缺点**：
- 实现复杂
- 每帧分配位置可能变化，增加 GPU 开销
- 碎片化问题

#### 方案 C：混合方案（推荐 ????）

方向光使用固定分区（因为 CSM 每帧都需要），聚光灯和点光源使用动态分配。

**优点**：
- 兼顾稳定性和灵活性

**缺点**：
- 实现复杂度介于 A 和 B 之间

### 2.2 推荐方案

**推荐方案 A（固定分区布局）**，理由：
1. 实现最简单，适合首次引入 Atlas 架构
2. 当前光源数量限制已经明确（2 方向光 + 4 聚光灯 + 2 点光源）
3. 4096×4096 的 Atlas 空间足够容纳所有预设 Tile
4. 后续如需优化空间利用率，可以升级为方案 C

### 2.3 容量规划

| 光源类型 | 最大数量 | 每光源 Tile 数 | Tile 尺寸 | 总占用 |
|----------|---------|---------------|-----------|--------|
| 方向光（CSM） | 2 | 4（级联） | 1024×1024 | 2048×4096 |
| 聚光灯 | 4 | 1 | 512×512 | 2048×512 |
| 点光源 | 2 | 6（面） | 512×512 | 3072×1024 |

**Atlas 总大小**：4096×4096 = 16M 像素
**深度格式**：`GL_DEPTH_COMPONENT24`（24 位）= 48MB 显存

---

## 三、ShadowAtlas 类设计

### 3.1 头文件

```cpp
// Lucky/Source/Lucky/Renderer/ShadowAtlas.h
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
```

### 3.2 实现文件

```cpp
// Lucky/Source/Lucky/Renderer/ShadowAtlas.cpp
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

        // ======== 方向光区域（上半部分：0~4096 x 0~4096 的上方） ========
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

        // ======== 聚光灯区域（方向光区域右侧或下方） ========
        // 布局：4 个 Tile 水平排列，y = 4096（方向光区域下方）
        // 实际位置：x = 0~2048, y = 4096 处（但 Atlas 只有 4096 高）
        // 修正：放在 x=2048~4096, y=0~512 区域
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
        // 布局：每个点光源 6 面，水平排列
        // 点光源 0：x=2048~4096+, y=512~1024
        // 点光源 1：x=2048~4096+, y=1024~1536
        for (int pointIdx = 0; pointIdx < s_MaxPointLightShadows; ++pointIdx)
        {
            for (int face = 0; face < 6; ++face)
            {
                ShadowAtlasTile& tile = m_Tiles[tileIndex];
                // 每行放 4 个面，需要 2 行
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
```

---

## 四、RenderCommand 扩展（Scissor Test）

### 4.1 需求说明

Shadow Atlas 中多个 Tile 共享同一张纹理。渲染某个 Tile 时，必须确保深度写入不会溢出到相邻 Tile 区域。使用 OpenGL Scissor Test 可以精确限制渲染区域。

### 4.2 方案对比

#### 方案 A：在 RenderCommand 中封装 Scissor（推荐 ?????）

与现有 `SetCullMode()`、`SetDepthWrite()` 等接口风格一致。

```cpp
// RenderCommand.h 新增
static void SetScissorTest(bool enable);
static void SetScissor(int x, int y, int width, int height);
```

**优点**：
- 与现有 API 风格完全一致
- 封装 OpenGL 细节
- 可在 `ResetDefaultRenderState()` 中统一重置

**缺点**：无

#### 方案 B：直接在 ShadowPass 中调用 OpenGL

```cpp
glEnable(GL_SCISSOR_TEST);
glScissor(x, y, width, height);
```

**优点**：
- 无需修改 RenderCommand

**缺点**：
- 破坏封装，违反项目架构原则
- 无法在 `ResetDefaultRenderState()` 中统一管理

### 4.3 推荐实现（方案 A）

```cpp
// RenderCommand.h 新增接口

/// <summary>
/// 设置裁剪测试开关
/// </summary>
/// <param name="enable">是否启用裁剪测试</param>
static void SetScissorTest(bool enable);

/// <summary>
/// 设置裁剪矩形区域（同时启用裁剪测试）
/// 仅在裁剪矩形内的像素会被写入帧缓冲
/// </summary>
/// <param name="x">裁剪区域左下角 X（像素）</param>
/// <param name="y">裁剪区域左下角 Y（像素）</param>
/// <param name="width">裁剪区域宽度（像素）</param>
/// <param name="height">裁剪区域高度（像素）</param>
static void SetScissor(int x, int y, int width, int height);
```

```cpp
// RenderCommand.cpp 实现

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
```

```cpp
// ResetDefaultRenderState() 中新增

void RenderCommand::ResetDefaultRenderState()
{
    // ... 现有代码 ...

    // 关闭裁剪测试（默认关闭）
    glDisable(GL_SCISSOR_TEST);
}
```

---

## 五、SceneLightData 重构

### 5.1 需求说明

当前 `SceneLightData` 仅有单个方向光的阴影参数。需要扩展为每个光源独立的阴影参数。

### 5.2 方案对比

#### 方案 A：每光源类型独立的阴影参数结构体（推荐 ?????）

```cpp
struct SceneLightData
{
    // ... 现有光源数据 ...

    struct DirLightShadowParams { ... };
    DirLightShadowParams DirLightShadows[s_MaxDirectionalLights];

    struct SpotLightShadowParams { ... };
    SpotLightShadowParams SpotLightShadows[s_MaxSpotLights];

    struct PointLightShadowParams { ... };
    PointLightShadowParams PointLightShadows[s_MaxPointLights];
};
```

**优点**：
- 类型安全，每种光源有专属参数
- 方向光保留 CSM 专属字段（CascadeCount、ShadowDistance 等）
- 清晰的数据所有权

**缺点**：
- 结构体较大

#### 方案 B：统一的阴影参数结构体

```cpp
struct LightShadowParams
{
    ShadowType Shadows = ShadowType::None;
    float ShadowBias = 0.0003f;
    float ShadowStrength = 1.0f;
    // CSM 专用（仅方向光使用）
    int CascadeCount = 4;
    float ShadowDistance = 150.0f;
    // ...
};
```

**优点**：
- 代码更简洁

**缺点**：
- 不同光源类型共享不相关的字段，语义不清晰
- 浪费内存（聚光灯不需要 CSM 字段）

### 5.3 推荐实现（方案 A）

```cpp
// Renderer3D.h 中 SceneLightData 修改

struct SceneLightData
{
    int DirectionalLightCount = 0;
    DirectionalLightData DirectionalLights[s_MaxDirectionalLights];

    int PointLightCount = 0;
    PointLightData PointLights[s_MaxPointLights];

    int SpotLightCount = 0;
    SpotLightData SpotLights[s_MaxSpotLights];

    // ======== 每光源阴影参数（替代原有的单一 DirLightShadowType） ========

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

    // ---- 向后兼容（过渡期保留，R28 完成后删除） ----
    ShadowType DirLightShadowType = ShadowType::None;
    float DirLightShadowBias = 0.005f;
    float DirLightShadowStrength = 1.0f;
    int CascadeCount = 4;
    float ShadowDistance = 150.0f;
    float CascadeSplits[s_MaxCascadeCount] = { 0.067f, 0.2f, 0.467f, 1.0f };
    int ShadowMapResolution = 2048;
};
```

---

## 六、RenderContext 扩展

### 6.1 新增 Shadow Atlas 数据

```cpp
// RenderContext.h 中新增字段（在现有阴影数据之后）

// ======== Shadow Atlas 数据 ========
uint32_t ShadowAtlasTextureID = 0;          // Shadow Atlas 深度纹理 ID
uint32_t ShadowAtlasSize = 4096;            // Atlas 纹理尺寸

/// <summary>
/// Shader 端需要的每光源阴影数据
/// 由 Renderer3D::EndScene() 从 ShadowAtlas 中提取，传递给 OpaquePass/TransparentPass
/// </summary>
struct ShaderShadowData
{
    // ---- 方向光阴影（最多 2 个方向光 × 4 级联） ----
    int DirLightShadowCount = 0;
    struct DirLightShadow
    {
        int CascadeCount = 4;
        glm::mat4 LightSpaceMatrices[4];        // 每级 Light Space Matrix
        glm::vec4 AtlasScaleBias[4];            // 每级在 Atlas 中的 UV 变换
        float CascadeFarPlanes[4];              // 每级远平面距离（视图空间）
        float ShadowBias = 0.0003f;
        float ShadowStrength = 1.0f;
        int ShadowType = 1;                     // 1=Hard, 2=Soft
    };
    DirLightShadow DirLights[2];

    // ---- 聚光灯阴影（最多 4 个） ----
    int SpotLightShadowCount = 0;
    struct SpotLightShadow
    {
        int LightIndex = -1;                    // 在 SpotLights[] 中的索引
        glm::mat4 LightSpaceMatrix;             // 光源空间 VP 矩阵
        glm::vec4 AtlasScaleBias;               // 在 Atlas 中的 UV 变换
        float ShadowBias = 0.001f;
        float ShadowStrength = 1.0f;
        int ShadowType = 1;
    };
    SpotLightShadow SpotLights[4];

    // ---- 点光源阴影（最多 2 个 × 6 面） ----
    int PointLightShadowCount = 0;
    struct PointLightShadow
    {
        int LightIndex = -1;                    // 在 PointLights[] 中的索引
        glm::mat4 LightSpaceMatrices[6];        // 6 面 Light Space Matrix
        glm::vec4 AtlasScaleBias[6];            // 6 面在 Atlas 中的 UV 变换
        float ShadowBias = 0.05f;
        float ShadowStrength = 1.0f;
        int ShadowType = 1;
        float FarPlane = 25.0f;                 // 点光源阴影远平面（= Range）
    };
    PointLightShadow PointLights[2];
};
ShaderShadowData ShadowData;
```

---

## 七、Atlas UV 变换原理

### 7.1 Scale-Bias 公式

每个 Tile 在 Atlas 中的位置通过 `vec4(scaleX, scaleY, biasX, biasY)` 描述：

```
atlasUV.xy = localUV.xy * scale.xy + bias.xy
```

其中：
- `localUV`：片段在光源空间投影后的 [0,1] 坐标
- `scale.xy = vec2(tileWidth / atlasSize, tileHeight / atlasSize)`
- `bias.xy = vec2(tileX / atlasSize, tileY / atlasSize)`

### 7.2 Shader 中的变换

```glsl
/// <summary>
/// 将世界空间坐标变换到 Atlas UV 空间
/// </summary>
vec3 WorldToAtlasUV(vec3 worldPos, mat4 lightSpaceMatrix, vec4 atlasScaleBias)
{
    // 1. 变换到光源裁剪空间
    vec4 lightSpacePos = lightSpaceMatrix * vec4(worldPos, 1.0);

    // 2. 透视除法 + 映射到 [0,1]
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;

    // 3. 应用 Atlas 缩放和偏移
    projCoords.xy = projCoords.xy * atlasScaleBias.xy + atlasScaleBias.zw;

    return projCoords;
}
```

### 7.3 PCF 采样边界保护

PCF 采样时需要确保不会采样到相邻 Tile 的深度值：

```glsl
/// <summary>
/// Atlas 软阴影采样（PCF 5×5，带边界保护）
/// </summary>
float ShadowSampleSoftClamped(vec3 atlasUV, float bias, vec4 atlasScaleBias)
{
    float currentDepth = atlasUV.z;
    float shadow = 0.0;

    // texel size 基于 Tile 实际大小计算
    vec2 atlasTexelSize = 1.0 / vec2(textureSize(u_ShadowAtlas, 0));
    vec2 texelSize = atlasTexelSize;

    // Tile 边界（留 1 像素边距防止采样溢出）
    vec2 tileMin = atlasScaleBias.zw + atlasTexelSize;
    vec2 tileMax = atlasScaleBias.zw + atlasScaleBias.xy - atlasTexelSize;

    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            vec2 sampleUV = clamp(
                atlasUV.xy + vec2(x, y) * texelSize,
                tileMin, tileMax
            );
            float pcfDepth = texture(u_ShadowAtlas, sampleUV).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 25.0;
}
```

---

## 八、向后兼容与迁移策略

### 8.1 过渡期设计

本阶段（R27）建立 Atlas 基础设施，但**不修改现有的方向光 CSM 渲染逻辑**。现有系统继续使用 `Texture2DArray` 方式工作。

**R27 完成后的状态**：
- `ShadowAtlas` 类已创建并可初始化
- `RenderCommand` 已扩展 Scissor Test
- `SceneLightData` 已扩展每光源阴影参数（旧字段保留）
- `RenderContext` 已扩展 `ShaderShadowData`（旧字段保留）
- 现有 CSM 渲染**不受影响**

**R28（聚光灯阴影）完成后**：
- 聚光灯使用 Atlas 系统
- 方向光仍使用旧的 Texture2DArray

**R29（点光源阴影）完成后**：
- 方向光 CSM 迁移到 Atlas
- 删除旧的 CSM Texture2DArray 相关代码
- 删除 `SceneLightData` 和 `RenderContext` 中的旧字段

### 8.2 旧字段废弃计划

| 旧字段 | 替代方案 | 删除时机 |
|--------|---------|---------|
| `SceneLightData::DirLightShadowType` | `DirLightShadows[0].Shadows` | R29 完成后 |
| `SceneLightData::DirLightShadowBias` | `DirLightShadows[0].ShadowBias` | R29 完成后 |
| `SceneLightData::DirLightShadowStrength` | `DirLightShadows[0].ShadowStrength` | R29 完成后 |
| `SceneLightData::CascadeCount` | `DirLightShadows[0].CascadeCount` | R29 完成后 |
| `SceneLightData::ShadowDistance` | `DirLightShadows[0].ShadowDistance` | R29 完成后 |
| `SceneLightData::CascadeSplits` | `DirLightShadows[0].CascadeSplits` | R29 完成后 |
| `SceneLightData::ShadowMapResolution` | `DirLightShadows[0].ShadowMapResolution` | R29 完成后 |
| `RenderContext::ShadowEnabled` | `ShadowData.DirLightShadowCount > 0` | R29 完成后 |
| `RenderContext::ShadowBias` | `ShadowData.DirLights[0].ShadowBias` | R29 完成后 |
| `RenderContext::CascadeShadowMapArrayTextureID` | `ShadowAtlasTextureID` | R29 完成后 |

---

## 九、涉及的文件清单

### 需要新增的文件

| 文件路径 | 内容 |
|---------|------|
| `Lucky/Source/Lucky/Renderer/ShadowAtlas.h` | Shadow Atlas 管理器头文件 |
| `Lucky/Source/Lucky/Renderer/ShadowAtlas.cpp` | Shadow Atlas 管理器实现 |

### 需要修改的文件

| 文件路径 | 修改内容 |
|---------|----------|
| `Lucky/Source/Lucky/Renderer/RenderCommand.h` | 新增 `SetScissorTest()`、`SetScissor()` |
| `Lucky/Source/Lucky/Renderer/RenderCommand.cpp` | Scissor Test 实现 + `ResetDefaultRenderState()` 更新 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.h` | `SceneLightData` 扩展每光源阴影参数 |
| `Lucky/Source/Lucky/Renderer/RenderContext.h` | 新增 `ShaderShadowData`、`ShadowAtlasTextureID` |

### 不需要修改的文件（本阶段）

| 文件路径 | 原因 |
|---------|------|
| `ShadowPass.h/cpp` | R28 中修改 |
| `OpaquePass.cpp` | R28 中修改 |
| `TransparentPass.cpp` | R28 中修改 |
| `Shadow.glsl` | R28 中修改 |
| `Lighting.glsl` | R28 中修改 |
| `Framebuffer.h/cpp` | Atlas 使用现有 `DEPTH_COMPONENT` 格式，无需扩展 |

---

## 十、实施步骤

| 步骤 | 内容 | 验证方式 |
|------|------|---------|
| 1 | 创建 `ShadowAtlas.h` | 编译通过 |
| 2 | 创建 `ShadowAtlas.cpp`（`Init()`、`InitFixedLayout()`、`BeginFrame()`） | 编译通过 |
| 3 | 实现 `ActivateDirectionalTile()`、`ActivateSpotTile()`、`ActivatePointTile()` | 编译通过 |
| 4 | 扩展 `RenderCommand`（`SetScissorTest()`、`SetScissor()`） | 编译通过 |
| 5 | 更新 `ResetDefaultRenderState()` 关闭 Scissor | 编译通过 |
| 6 | 扩展 `SceneLightData`（新增每光源阴影参数，保留旧字段） | 编译通过，现有功能不受影响 |
| 7 | 扩展 `RenderContext`（新增 `ShaderShadowData`，保留旧字段） | 编译通过，现有功能不受影响 |
| 8 | 运行测试：确认现有 CSM 阴影功能正常 | 视觉验证 |

---

## 十一、设计决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| Atlas 布局策略 | 固定分区（方案 A） | 实现简单，当前光源数量有限，空间足够 |
| Atlas 纹理格式 | `DEPTH_COMPONENT`（2D） | 复用现有 Framebuffer，无需扩展 |
| Atlas 大小 | 4096×4096 | 平衡质量和显存（48MB） |
| Scissor Test 封装 | RenderCommand 封装（方案 A） | 与现有 API 风格一致 |
| SceneLightData 扩展 | 每光源类型独立结构体（方案 A） | 类型安全，语义清晰 |
| 向后兼容 | 保留旧字段，渐进式迁移 | 降低风险，可逐步验证 |
