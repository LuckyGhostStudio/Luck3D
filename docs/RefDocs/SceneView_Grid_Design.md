# Scene View 世界坐标系网格 - 详细设计文档

> **目标**: 本文档提供 Unity Editor Scene View 中世界坐标系网格（World Space Grid）的完整设计与实现方案。读者可根据本文档，在任意项目（Unity Editor 或 Runtime）中复刻出相同效果的无限网格渲染系统。

---

## 目录

1. [架构总览](#1-架构总览)
2. [C# 托管层——参数与状态管理](#2-c-托管层参数与状态管理)
3. [C++ 原生层——网格渲染核心](#3-c-原生层网格渲染核心)
4. [Shader 设计说明](#4-shader-设计说明)
5. [数据流与生命周期](#5-数据流与生命周期)
6. [纯 C# 等效实现方案](#6-纯-c-等效实现方案)
7. [关键常量和默认值速查表](#7-关键常量和默认值速查表)

---

## 1. 架构总览

```
+============================================================+
|  C# 托管层: SceneViewGrid.cs                                |
|  +--------------------------------------------------------+ |
|  | GridSettings (static)                                   | |
|  |   - 全局网格尺寸: Vector3 (X, Y, Z)                      | |
|  |   - sizeMultiplier: 2^n 倍数缩放                         | |
|  |   - 持久化: SavedFloat / SavedInt                       | |
|  +--------------------------------------------------------+ |
|  | SceneViewGrid (instance, serialized)                    | |
|  |   - 3 个轴面 Grid (xGrid / yGrid / zGrid)               | |
|  |   - 每轴面: pivot (原点), size (线间距), color, fade      | |
|  |   - showGrid / gridOpacity / gridAxis 控制开关            | |
|  |   - PrepareGridRender() -> DrawGridParameters             | |
|  +--------------------------+-----------------------------+ |
+==============================|==============================+
                               | DrawGridParameters (struct)
                               |   gridID, pivot, color, size
                               v
+============================================================+
|  C++ 原生层: DrawGrid.cpp                                   |
|  +--------------------------------------------------------+ |
|  | DrawGrid(camera, &params) -> DoDrawGrid(camera, &, axis) | |
|  |   - 4 级线间距 (1x, 10x, 100x, 1000x)                   | |
|  |   - GfxDevice::ImmediateBegin/End 即时模式               | |
|  |   - 透视校正线宽 (texCoord = cellSize)                    | |
|  |   - Z-fighting 偏移                                      | |
|  |   - far/near plane 裁剪                                  | |
|  +--------------------------+-----------------------------+ |
+==============================|==============================+
                               v
+============================================================+
|  Shader: Grid.mat / GridOrtho.mat                           |
|  - 根据 texCoord 做线宽抗锯齿                                 |
|  - Grid.mat: 透视模式 (世界空间线宽)                          |
|  - GridOrtho.mat: 正交模式 (屏幕空间线宽)                     |
+============================================================+
```

**核心设计思想**: 网格本质上是一个覆盖相机视野的**线框平面**。它不是一整张无限大的纹理，而是实时计算出相机视野内可见的一组平行线段的集合，用 GPU 即时模式（Immediate Mode）绘制。

---

## 2. C# 托管层——参数与状态管理

### 2.1 GridSettings（全局网格尺寸）

```csharp
static class GridSettings
{
    // 范围限制
    const float k_GridSizeMin = .0001f;
    const float k_GridSizeMax = 1024f;
    const float k_DefaultGridSize = 1.0f;
    const int   k_DefaultMultiplier = 0;

    // 持久化存储 (EditorPrefs)
    static SavedFloat s_GridSizeX = new SavedFloat("GridSizeX", k_DefaultGridSize);
    static SavedFloat s_GridSizeY = new SavedFloat("GridSizeY", k_DefaultGridSize);
    static SavedFloat s_GridSizeZ = new SavedFloat("GridSizeZ", k_DefaultGridSize);
    static SavedInt   s_GridMultiplier = new SavedInt("GridMultiplier", k_DefaultMultiplier);

    // 对外接口
    static Vector3 size { get; set; }        // 经过 multiplier 修正后的尺寸
    static int sizeMultiplier { get; set; }  // 2^n 倍数
    static event Action<Vector3> sizeChanged; // 尺寸变化事件
}
```

**关键逻辑**:
- `size` getter 会对 `rawSize` 应用 `sizeMultiplier`：当 multiplier > 0 时每 +1 尺寸 x2，< 0 时每 -1 尺寸 /2
- `size` setter 会首先 `ResetSizeMultiplier()`，然后 clamp 到 `[k_GridSizeMin, k_GridSizeMax]`
- 尺寸变化时触发 `sizeChanged` 事件，SceneViewGrid 监听后自动重新吸附 pivot

**sizeMultiplier 计算**:
```csharp
static Vector3 ApplyMultiplier(Vector3 value, int mul)
{
    if (mul > 0)
        for (int i = 0; i < mul; i++) value *= 2f;
    else if (mul < 0)
        for (int i = 0; i > mul; i--) value /= 2f;
    return value;
}
```

### 2.2 SceneViewGrid（单窗口网格实例）

```csharp
[System.Serializable]
class SceneViewGrid
{
    // === 三轴面 ===
    Grid xGrid; // 垂直于 X 轴的平面 (YZ 平面)
    Grid yGrid; // 垂直于 Y 轴的平面 (XZ 平面, 默认/水平面)
    Grid zGrid; // 垂直于 Z 轴的平面 (XY 平面)

    // === 开关控制 ===
    bool m_ShowGrid;              // 是否显示网格
    GridRenderAxis m_GridAxis;    // 当前激活轴: X/Y/Z/All
    float m_gridOpacity;          // 全局不透明度 [0, 1]

    // === 颜色 ===
    static PrefColor kViewGridColor = new PrefColor("Scene/Grid", .5f, .5f, .5f, .4f);
}
```

**Grid 子结构** (per-axis):

```csharp
[System.Serializable]
internal class Grid
{
    AnimBool m_Fade;    // 渐变开关（smooth transition）
    Color    m_Color;   // 网格颜色
    Vector3  m_Pivot;   // 网格原点（世界坐标）
    Vector2  m_Size;    // 线间距 (x, y) 单位

    internal DrawGridParameters PrepareGridRender(int gridID, float opacity)
    {
        DrawGridParameters p = default;
        p.gridID = gridID;    // 0=X轴面, 1=Y轴面, 2=Z轴面
        p.pivot  = m_Pivot;
        p.color  = m_Color;
        p.color.a = m_Fade.faded * opacity;  // 渐变 x 全局不透明度
        p.size   = m_Size;
        return p;
    }
}
```

### 2.3 轴面可见性策略

`UpdateGridsVisibility(rotation, orthoMode)` 的核心逻辑：

```
if (orthoMode && 相机方向严格对齐某轴):
    -> 只显示对齐的那个面（XY/XZ/YZ 精确对齐）
else:
    -> 按 gridAxis 设置显示指定的面
```

```csharp
void UpdateGridsVisibility(Quaternion rotation, bool orthoMode)
{
    bool showX = false, showY = false, showZ = false;

    if (orthoMode)
    {
        Vector3 fwd = rotation * Vector3.forward;
        // 正交模式特判：摄像机严格朝向某轴时，只画那个面
        if (fwd == Vector3.up || fwd == Vector3.down)
            showY = true;
        else if (fwd == Vector3.left || fwd == Vector3.right)
            showX = true;
        else if (fwd == Vector3.forward || fwd == Vector3.back)
            showZ = true;
    }

    // 透视模式 or 非对齐正交: 按 gridAxis 设置
    if (!showX && !showY && !showZ)
    {
        showX = (gridAxis == GridRenderAxis.X || gridAxis == GridRenderAxis.All);
        showY = (gridAxis == GridRenderAxis.Y || gridAxis == GridRenderAxis.All);
        showZ = (gridAxis == GridRenderAxis.Z || gridAxis == GridRenderAxis.All);
    }

    // 设置动画目标值（平滑过渡）
    xGrid.fade.target = showX;
    yGrid.fade.target = showY;
    zGrid.fade.target = showZ;
}
```

### 2.4 线间距与吸附

每个轴面的线间距来自 `GridSettings.size`：

| 轴面 | 绘制平面 | size.x (dim0) | size.y (dim1) |
|------|----------|---------------|---------------|
| X (gridID=0) | YZ | grid.y | grid.z |
| Y (gridID=1) | XZ | grid.z | grid.x |
| Z (gridID=2) | XY | grid.x | grid.y |

```csharp
// X轴面的线间距 = (Y方向间距, Z方向间距)
void ApplySnapContraintsOnXAxis() { xGrid.size = new Vector2(grid.y, grid.z); }
// Y轴面的线间距 = (Z方向间距, X方向间距)
void ApplySnapContraintsOnYAxis() { yGrid.size = new Vector2(grid.z, grid.x); }
// Z轴面的线间距 = (X方向间距, Y方向间距)
void ApplySnapContraintsOnZAxis() { zGrid.size = new Vector2(grid.x, grid.y); }
```

当 `GridSettings.size` 变化时，pivot 会自动吸附到新的网格尺寸上：

```csharp
void GridSizeChanged(Vector3 size)
{
    SetPivot(X, Snapping.Snap(GetPivot(X), GridSettings.size));
    SetPivot(Y, Snapping.Snap(GetPivot(Y), GridSettings.size));
    SetPivot(Z, Snapping.Snap(GetPivot(Z), GridSettings.size));
}
```

### 2.5 渐变系统

使用 `AnimBool` 实现轴面之间的平滑切换：
- `xGrid.fade.target = showX` — 设置目标值
- `fade.faded` — 当前插值结果（0->1 平滑过渡）
- `fade.valueChanged.AddListener(view.Repaint)` — 渐变过程中持续驱动重绘

### 2.6 PrepareGridRender 入口

```csharp
internal DrawGridParameters PrepareGridRender(Camera camera, Vector3 pivot,
    Quaternion rotation, float size, bool orthoMode)
{
    UpdateGridsVisibility(rotation, orthoMode);

    if (orthoMode)
    {
        ApplySnapConstraintsInOrthogonalMode();
        return PrepareGridRenderOrthogonalMode(camera, pivot, rotation, size);
    }

    ApplySnapConstraintsInPerspectiveMode();
    return PrepareGridRenderPerspectiveMode(camera, pivot, rotation, size);
}
```

正交模式下额外检查角度阈值 `k_AngleThresholdForOrthographicGrid = 0.15`，如果视角太斜则不显示该轴面以避免难看的效果。

---

## 3. C++ 原生层——网格渲染核心

### 3.1 数据结构

```cpp
// DrawGrid.h
struct DrawGridParameters
{
    int         gridID;   // 0=X面, 1=Y面, 2=Z面
    Vector3f    pivot;    // 网格原点（世界坐标）
    ColorRGBAf  color;    // 颜色（含 alpha）
    Vector2f    size;     // 基础线间距 (x, y)
};
BIND_MANAGED_TYPE_NAME(DrawGridParameters, UnityEditor_DrawGridParameters);

void DrawGrid(const Camera* camera, const DrawGridParameters* gridParam);
```

### 3.2 入口函数

```cpp
void DrawGrid(const Camera* camera, const DrawGridParameters* gridParam)
{
    Assert(camera != 0);
    if (gridParam->color.a > 0.0F)    // alpha 为 0 时跳过
        DoDrawGrid(camera, gridParam, gridParam->gridID);
}
```

### 3.3 核心渲染算法 (DoDrawGrid)

#### Step 1: 提前退出条件

```cpp
// 相机距离超过 far plane -> 不画
if (Abs(camera->GetPosition()[axis]) > camera->GetFar()) return;

// 非有限值 -> 不画
if (!IsFinite(camDistance * camDistance)) return;
```

#### Step 2: 计算网格覆盖范围

```cpp
Transform& transform = camera->GetComponent<Transform>();
Vector3f camPosition = transform.GetPosition();
float camDistance = Abs(camPosition[axis]);

// 计算相机能看到的地面半宽度（对应网格平面方向）
float verticalHalfCoverage = ortho
    ? camera->GetOrthographicSize()
    : camDistance * std::tan(Deg2Rad(camera->GetVerticalFieldOfView() * 0.5f));

// 考虑宽高比的修正（让对角线方向也有足够的网格覆盖）
float aspectRatioCanceller = std::sqrt(camera->GetAspect()) * std::sqrt(2.0f);
float groundHalfCoverage = verticalHalfCoverage * aspectRatioCanceller;
```

**推导说明**:
- 透视模式下，`groundHalfCoverage` = 相机在网格平面的高度 x tan(半FOV) x 宽高比修正
- `sqrt(aspect) * sqrt(2)` 的修正因子确保屏幕对角线方向也有足够的网格覆盖

#### Step 3: 4 级线间距

```cpp
const int kMultiplier = 10;        // 每级放大 10 倍
const int kStepSizeCount = 4;      // 共 4 级

float stepSizes[2][kStepSizeCount];

// 初始化第 0 级（基础间距）
stepSizes[0][0] = gridParam->size.x;
stepSizes[1][0] = gridParam->size.y;

// 生成 10x, 100x, 1000x 级别
for (int s = 1; s < kStepSizeCount; s++)
    stepSizes[0][s] = stepSizes[0][s-1] * kMultiplier;
for (int s = 1; s < kStepSizeCount; s++)
    stepSizes[1][s] = stepSizes[1][s-1] * kMultiplier;
```

**视觉效果表**:

| 级别 | 倍率 | 视觉作用 |
|------|------|----------|
| 0 | 1x | 细密网格线（近距离可见） |
| 1 | 10x | 中等网格线（中距离可见） |
| 2 | 100x | 粗网格线（远距离可见） |
| 3 | 1000x | 最粗网格线（极大范围） |

**去重逻辑**: 10x 整数倍的线由上级负责绘制，本级跳过：
```cpp
if ((i % kMultiplier + kMultiplier) % kMultiplier == 0)
{
    if (!biggestGrid)
        continue;               // 不是最大级别则跳过
    else
        cellSize *= kMultiplier; // 最大级别的粗线额外加粗
}
```

例如：线间距 base=1 时，位置 0, 10, 20, 30... 的线会被第 0 级跳过，由第 1 级（10x）绘制。

#### Step 4: 线宽控制（透视校正）

网格线的视觉粗细通过 `texCoord` 传递给 Shader：

```cpp
float cellSizeInit = size * 5 / groundHalfCoverage;
const float kMaxLineStrength = 5.0f;  // 最大线宽上限

// 每个顶点的 texCoord.x = cellSize（即此线的视觉粗细参数）
if (!ortho)
    cellSize *= camDistance;    // 透视: 越远越细

cellSize = std::min(cellSize, kMaxLineStrength);  // 限制最大值

GetGfxDevice().ImmediateTexCoordAll(cellSize, 0, 0);
```

**设计意图**:
- 正交模式：`cellSize = size * 5 / groundHalfCoverage`，线宽是**屏幕空间**常量
- 透视模式：额外乘以 `camDistance`，使线在**世界空间**保持恒定粗细（近大远小的视觉，但网格线的绝对像素宽度不变）
- `kMaxLineStrength = 5.0f` 限制最大值，防止过粗的线过于刺眼

#### Step 5: 线段数量计算

```cpp
float gridSizeFloat = (groundHalfCoverage / size) * 500;
gridSize = clamp(gridSize, 2, 100);
```

**关键**: 每条轴方向最多画 100 条线段（`gridSize` 上限），最坏情况下总线段数为：
- 1 个轴面 x 2 个方向 x 4 个级别 x 100 条 = **800 条线段/帧**

这确保了即使在极端情况下（如 size=0.0001），也不会绘制过多线段。

#### Step 6: 生成线段（伪代码结构）

```cpp
int axis2 = (axis + 1) % 3;  // 网格面的 dim0（跨越方向）
int axis3 = (axis + 2) % 3;  // 网格面的 dim1

for (int dim = 0; dim < 2; dim++)           // 两个方向维度
    for (int s = 0; s < 4; s++)             // 四个级别
        for (int i = center[dim]-gridSize;  // 遍历该方向所有线
                 i <= center[dim]+gridSize; i++)
        {
            // 1. 计算 cellSize (线宽参数)
            // 2. 去重跳过 (10x 倍数由上级画)
            // 3. 生成立即模式顶点 pair:
            //    ImmediateTexCoordAll(cellSize, 0, 0);
            //    ImmediateVertex(起点 + gridOffset);
            //    ImmediateTexCoordAll(cellSize, 0, 0);
            //    ImmediateVertex(终点 + gridOffset);
        }
```

#### Step 7: Z-Fighting 避免

```cpp
static const float kGridOffsetValue = 0.01f;
float errorDist = groundHalfCoverage / camDistance;

// 正交: 整个网格统一偏移
Vector3f gridOffset = -dir * kGridOffsetValue * camDistance * errorDist;

// 透视: 每顶点独立计算（远处偏移更大）
if (!ortho)
    gridOffset = (camPosition - v) * errorDist * kGridOffsetValue;

// 顶点 = 原始位置 + 偏移量
GetGfxDevice().ImmediateVertex(v.x + gridOffset.x, v.y + gridOffset.y, v.z + gridOffset.z);
```

**原理**: 沿相机前向（`-dir`）方向略微偏移网格顶点，使其不与恰好放在网格平面上的物体产生 Z-fighting。透视模式下偏移量与顶点到相机距离成正比。

#### Step 8: GPU 绘制

```cpp
ShaderPassContext& passContext = GetDefaultPassContext();

// 选择材质并开始立即模式
if (ortho)
    GetGfxDevice().ImmediateBegin(kPrimitiveLines,
        s_GridMaterialOrtho->SetPassSlow(0, passContext));
else
    GetGfxDevice().ImmediateBegin(kPrimitiveLines,
        s_GridMaterial->SetPassSlow(0, passContext));

// 设置颜色
ColorRGBAf color = GammaToActiveColorSpace(gridParam->color);
GetGfxDevice().ImmediateColor(color.r, color.g, color.b, color.a);

// ... 提交所有顶点 ...

GetGfxDevice().ImmediateEnd();
```

材质加载一次后缓存在 `static PPtr<Material>` 中：
```cpp
s_GridMaterial      = GetEditorResource<Material>("SceneView/Grid.mat");
s_GridMaterialOrtho = GetEditorResource<Material>("SceneView/GridOrtho.mat");
```

---

## 4. Shader 设计说明

### 4.0 核心挑战：GL.LINES 为什么不够

**背景**: Unity Editor 原生的 `Grid.mat` 使用 C++ 的 `GfxDevice::ImmediateBegin(kPrimitiveLines, ...)` 绘制线段，每个顶点附带 `texCoord.x = cellSize` 作为线宽参数。Shader 在片段着色器中根据这个参数控制线宽和抗锯齿。

**关键限制**: Unity 的 `GL.LINES` 模式绘制的是**固定 1 像素宽**的线段。标准顶点/片段着色器**无法**天然知道当前像素距离线段中心轴的偏移量——因为片段着色器只知道当前像素的插值位置，不知道它在屏幕空间中离这条线的"中心线"有多远。

**三种解决路线**:

| 方案 | 视觉品质 | 兼容性 | 复杂度 | 推荐场景 |
|------|----------|--------|--------|----------|
| A. Geometry Shader | ★★★★★ | DX11+/GL4.0+ | 中 | 编辑器工具/高端PC |
| B. CPU 生成四边形网格 | ★★★★★ | 全平台 | 高 | 跨平台/移动端 |
| C. GL.LINES + 透明度 | ★★★☆☆ | 全平台 | 低 | 快速原型 |

下面分别详细介绍前两种方案。

---

### 4.1 方案 A：Geometry Shader 管线（最接近原生效果）

**原理**: 在顶点着色器和片段着色器之间插入 Geometry Shader。Geometry Shader 接收 `GL.LINES` 的每对顶点，将它们**扩展为屏幕空间四边形**（两个三角形），然后片段着色器就能自然计算像素到线段中心的距离。

#### 4.1.1 管线结构

```
Vertex Shader:     world-space 位置 → 不做变换，直接透传
     ↓
Geometry Shader:   每对顶点 → 4 个顶点（形成四边形），计算屏幕空间法线方向
     ↓
Fragment Shader:   计算像素到线段中心距离 → smoothstep 抗锯齿 → 输出
```

#### 4.1.2 完整 Shader 代码 (Custom/GridLineGeo)

```hlsl
Shader "Custom/GridLineGeo"
{
    Properties
    {
        _Color ("Color", Color) = (0.5, 0.5, 0.5, 0.4)
    }
    SubShader
    {
        Tags { "Queue"="Transparent" "RenderType"="Transparent" }
        Blend SrcAlpha OneMinusSrcAlpha
        ZWrite Off
        ZTest LEqual

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma geometry geom
            #pragma fragment frag
            #pragma target 4.0
            #include "UnityCG.cginc"

            fixed4 _Color;

            // ---- 顶点着色器输入 ----
            struct v_in
            {
                float4 vertex : POSITION;   // 世界空间位置（来自 GL.Vertex()）
                float2 uv     : TEXCOORD0;  // x = cellSize (线宽参数)
            };

            // ---- 顶点着色器 → Geometry Shader ----
            struct v2g
            {
                float4 pos      : SV_POSITION;  // 世界空间位置（未变换）
                float  lineWidth : TEXCOORD0;
            };

            // ---- Geometry Shader → Fragment Shader ----
            struct g2f
            {
                float4 pos       : SV_POSITION;  // 裁剪空间
                float  distToEdge : TEXCOORD0;   // 顶点到线段中心的归一化距离
                float  lineWidth  : TEXCOORD1;   // 线宽
            };

            // ---- 顶点着色器：透传世界空间位置 ----
            v2g vert(v_in v)
            {
                v2g o;
                o.pos = v.vertex;          // 保持世界空间
                o.lineWidth = v.uv.x;      // C# 端传入的 cellSize
                return o;
            }

            // ---- Geometry Shader：将线段扩展为四边形 ----
            [maxvertexcount(4)]
            void geom(line v2g input[2], inout TriangleStream<g2f> triStream)
            {
                // 1. 将两个端点从世界空间变换到裁剪空间
                float4 p0 = UnityWorldToClipPos(input[0].pos.xyz);
                float4 p1 = UnityWorldToClipPos(input[1].pos.xyz);

                // 2. 计算屏幕空间中的线段方向向量
                float2 dir = normalize(p1.xy / p1.w - p0.xy / p0.w);

                // 3. 计算屏幕空间中垂直于线段的方向（法线）
                float2 normal = float2(-dir.y, dir.x);

                // 4. 线宽（半宽，单位：裁剪空间）
                //    乘以 _ScreenParams 确保线宽与屏幕分辨率无关
                float halfWidth = input[0].lineWidth / _ScreenParams.y;

                // 5. 生成四边形的 4 个顶点（两个三角形）
                g2f v[4];
                float2 offsets[4] = {
                    float2(-halfWidth, -1),  // 左下
                    float2( halfWidth, -1),  // 右下
                    float2(-halfWidth,  1),  // 左上
                    float2( halfWidth,  1)   // 右上
                };
                float dists[4] = { -1, 1, -1, 1 };  // 边距标记

                for (int i = 0; i < 4; i++)
                {
                    float t = (offsets[i].y + 1) * 0.5;  // [0, 1] 沿线段插值
                    float4 clipPos = lerp(p0, p1, t);

                    v[i].pos = clipPos;
                    v[i].pos.xy += normal * offsets[i].x * clipPos.w;
                    v[i].distToEdge = dists[i];         // -1 或 +1
                    v[i].lineWidth = input[0].lineWidth;
                }

                // 三角形 1: 0-1-2
                triStream.Append(v[0]);
                triStream.Append(v[1]);
                triStream.Append(v[2]);
                triStream.RestartStrip();

                // 三角形 2: 1-3-2
                triStream.Append(v[1]);
                triStream.Append(v[3]);
                triStream.Append(v[2]);
                triStream.RestartStrip();
            }

            // ---- 片段着色器：抗锯齿 ----
            fixed4 frag(g2f i) : SV_Target
            {
                fixed4 col = _Color;

                // 计算像素到线段中心的归一化距离
                // distToEdge 在 [-1, 1] 范围内，|distToEdge| 越小越接近中心
                float dist = abs(i.distToEdge);

                // 抗锯齿：用 smoothstep 在线条边缘做柔化
                // 边缘宽度设为线宽的 15%，使得不同粗细的线有相适应的柔边
                float edgeWidth = 0.15;
                float alpha = 1.0 - smoothstep(1.0 - edgeWidth, 1.0 + edgeWidth, dist);

                col.a *= alpha;

                // 线宽很细时额外降低 alpha，避免 sub-pixel 线闪烁
                col.a *= saturate(i.lineWidth * 0.5);

                return col;
            }
            ENDCG
        }
    }
}
```

#### 4.1.3 C# 端调用（GL.LINES + Geometry Shader）

```csharp
// 使用支持 Geometry Shader 的材质
Material geoMat = new Material(Shader.Find("Custom/GridLineGeo"));

geoMat.SetPass(0);
GL.Begin(GL.LINES);
GL.Color(gridParam.color);

// 每条线：两个顶点 + texCoord 携带线宽信息
foreach (var line in lines)
{
    GL.TexCoord(new Vector3(cellSize, 0, 0));
    GL.Vertex(worldPos_start);
    GL.TexCoord(new Vector3(cellSize, 0, 0));
    GL.Vertex(worldPos_end);
}

GL.End();
```

**优点**: 与 Unity Editor 原生逻辑最接近；CPU 侧只需提交少量顶点对；Geometry Shader 自动完成四边形扩展。

**缺点**: 需要 `#pragma target 4.0`（DX11 / OpenGL 4.0+）；WebGL / OpenGL ES 2.0 / Metal 不原生支持 Geometry Shader。

---

### 4.2 方案 B：CPU 预生成四边形（推荐，全平台兼容）

**原理**: 不在 GPU 上做线段扩展，而是在 CPU 侧将每条线段转换为两个三角形（一个四边形带），然后用 `GL.QUADS` 或 `GL.TRIANGLES` 绘制。片段着色器通过 UV 坐标知道当前像素离四边形中心的距离，从而实现抗锯齿。

#### 4.2.1 Shader (Custom/GridLineQuad)

```hlsl
Shader "Custom/GridLineQuad"
{
    Properties
    {
        _Color ("Color", Color) = (0.5, 0.5, 0.5, 0.4)
    }
    SubShader
    {
        Tags { "Queue"="Transparent" "RenderType"="Transparent" }
        Blend SrcAlpha OneMinusSrcAlpha
        ZWrite Off
        ZTest LEqual

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"

            fixed4 _Color;

            struct v2f
            {
                float4 pos        : SV_POSITION;
                float  uvX        : TEXCOORD0;  // [-1, 1] 横向距离标记
                float  lineWidth  : TEXCOORD1;  // 线宽参数
            };

            v2f vert(appdata_full v)
            {
                v2f o;
                // v.vertex = 世界空间四边形顶点坐标
                o.pos = UnityWorldToClipPos(v.vertex.xyz);
                // v.texcoord1.x = 四边形横向偏移标记 [-1, 0, 0, 1] 或 [-1, 1]
                o.uvX = v.texcoord1.x;
                // v.texcoord.x = cellSize (线宽参数)
                o.lineWidth = v.texcoord.x;
                return o;
            }

            fixed4 frag(v2f i) : SV_Target
            {
                fixed4 col = _Color;

                // 计算到线段中心的归一化距离
                float dist = abs(i.uvX);

                // 核心抗锯齿：在线条边缘用 smoothstep 做柔化
                // edgeWidth 控制柔边宽度，约为四边形宽度的 15%
                float edgeWidth = 0.20;

                // dist 范围：0（中心）到 1（四边形边缘）
                // 在 [1 - edgeWidth, 1] 区间内 alpha 从 1 渐变到 0
                float alpha = 1.0 - smoothstep(1.0 - edgeWidth, 1.0, dist);

                col.a *= alpha;

                // 线宽很小时降低整体 alpha（避免单像素线过亮或锯齿）
                col.a *= saturate(i.lineWidth * 0.4);

                return col;
            }
            ENDCG
        }
    }
}
```

#### 4.2.2 四边形生成算法

```csharp
/// <summary>
/// 将一对世界空间端点 + 线宽参数 转换为屏幕空间四边形顶点
/// 用于 GL.QUADS 绘制
/// </summary>
static void EmitLineQuad(
    Camera camera,
    Vector3 worldStart, Vector3 worldEnd,
    float cellSize,          // 线宽参数（来自 C++ 的 cellSize 计算）
    Material material)
{
    // 1. 世界空间 → 视口空间
    Vector3 vp0 = camera.WorldToViewportPoint(worldStart);
    Vector3 vp1 = camera.WorldToViewportPoint(worldEnd);

    // 2. 裁剪：两端都在视口外且同侧 → 跳过
    if ((vp0.z < 0 && vp1.z < 0) || (vp0.z > 1 && vp1.z > 1))
        return;

    // 3. 屏幕空间线段方向与法线
    Vector2 screenPos0 = new Vector2(vp0.x, vp0.y);
    Vector2 screenPos1 = new Vector2(vp1.x, vp1.y);
    Vector2 lineDir = (screenPos1 - screenPos0).normalized;
    Vector2 normal = new Vector2(-lineDir.y, lineDir.x);

    // 4. 线宽（屏幕空间半宽，归一化到视口坐标系）
    //    cellSize 除以 _ScreenParams.y 和视口比例转换
    float halfWidth = cellSize / (camera.pixelHeight * 0.5f);

    // 5. 生成四边形的 4 个顶点（用 texcoord1.x 标记横向偏移）
    //    tx=-1 左边缘, tx=0 中心, tx=1 右边缘
    //    每个顶点用 WorldToViewportPoint → ViewportToWorldPoint 回推世界坐标
    Vector3[] quadVerts = new Vector3[4];
    float[] txValues = new float[4] { -1f, 1f, -1f, 1f };
    float[] edgeOffsets = new float[4] { -1f, 1f, -1f, 1f }; // -1 左, +1 右

    Vector2[] screenPoints = new Vector2[4];
    screenPoints[0] = screenPos0 + normal * (-halfWidth);
    screenPoints[1] = screenPos0 + normal * ( halfWidth);
    screenPoints[2] = screenPos1 + normal * (-halfWidth);
    screenPoints[3] = screenPos1 + normal * ( halfWidth);

    for (int i = 0; i < 4; i++)
    {
        Vector3 vp = new Vector3(screenPoints[i].x, screenPoints[i].y, vp0.z);
        quadVerts[i] = camera.ViewportToWorldPoint(vp);

        GL.TexCoord2(cellSize, edgeOffsets[i]);  // texcoord.x=线宽, texcoord.y=边距标记
        GL.Vertex(quadVerts[i]);
    }
}
```

> **注意**: 上述四边形生成算法需要在每次绘制时对每条线段做 `WorldToViewportPoint` / `ViewportToWorldPoint` 变换，这意味着在 CPU 侧有额外开销。但因为每条轴面最多 ~100 条线段，总开销仍然可忽略。

#### 4.2.3 简化版：直接使用世界空间正交偏移

如果网格平面与坐标轴对齐（这正是 Scene View 网格的情况），可以免去屏幕空间变换，直接在网格平面内做偏移：

```csharp
/// <summary>
/// 简化版：对于轴对齐的网格平面，直接在世界空间内偏移生成四边形
/// 适用于 X/Y/Z 三轴面网格
/// </summary>
static void EmitLineQuadAxisAligned(
    Vector3 worldStart,   // 线段起点（网格平面上）
    Vector3 worldEnd,     // 线段终点（网格平面上）
    int axis,             // 0=X面, 1=Y面, 2=Z面
    float cellSize,       // 线宽参数
    Camera camera)
{
    // 网格平面的法线即 axis 方向
    // 偏移方向 = 相机方向投影到网格平面后的方向
    // 简化做法：沿相机看向方向的投影偏移
    Vector3 camPos = camera.transform.position;
    Vector3 midPoint = (worldStart + worldEnd) * 0.5f;
    Vector3 toCamera = (camPos - midPoint).normalized;

    // 偏移大小：cellSize 在屏幕空间的像素，转换到世界空间约等于
    // (cellSize / pixelHeight) * distance * tan(fov/2) * 2
    float pixelToWorld = camera.orthographic
        ? camera.orthographicSize * 2f / camera.pixelHeight
        : (midPoint - camPos).magnitude * Mathf.Tan(camera.fieldOfView * 0.5f * Mathf.Deg2Rad) * 2f / camera.pixelHeight;

    float worldHalfWidth = cellSize * pixelToWorld * 0.5f;

    // 线段方向向量
    Vector3 lineDir = (worldEnd - worldStart).normalized;
    // 在网格平面内的法线方向（即线段的横向偏移方向）
    Vector3 lineNormal = Vector3.Cross(lineDir, toCamera).normalized;

    // 4 个顶点
    GL.TexCoord2(cellSize, -1f);
    GL.Vertex(worldStart - lineNormal * worldHalfWidth);
    GL.TexCoord2(cellSize,  1f);
    GL.Vertex(worldStart + lineNormal * worldHalfWidth);
    GL.TexCoord2(cellSize,  1f);
    GL.Vertex(worldEnd   + lineNormal * worldHalfWidth);
    GL.TexCoord2(cellSize, -1f);
    GL.Vertex(worldEnd   - lineNormal * worldHalfWidth);
}
```

---

### 4.3 两种 Shader 方案的对比总结

```
                    原生 Grid.mat     方案A GeoShader     方案B CPU Quads
                    ────────────      ───────────────    ────────────────
输入图元              kPrimitiveLines   GL.LINES            GL.QUADS
线段扩展              GPU (内置)        Geometry Shader      CPU 端预计算
片段着色器输入        内置 dist          自定义 distToEdge    自定义 uvX
平台要求              Editor 专属       DX11 / GL 4.0+      全平台
性能（~800 线段）     ~0.01ms          ~0.02ms             ~0.05ms
视觉品质              ★★★★★            ★★★★★               ★★★★★
实现难度              不可访问           中                   中
```

---

### 4.4 简易备选：纯 GL.LINES + 透明度（快速原型用）

如果暂时不需要完美的抗锯齿效果，可以用最简单的 Shader 配合 GL.LINES：

```hlsl
Shader "Custom/GridLineSimple"
{
    Properties { _Color ("Color", Color) = (0.5, 0.5, 0.5, 0.4) }
    SubShader
    {
        Tags { "Queue"="Transparent" "RenderType"="Transparent" }
        Blend SrcAlpha OneMinusSrcAlpha
        ZWrite Off

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"

            fixed4 _Color;

            struct v2f
            {
                float4 pos : SV_POSITION;
                float  lineWidth : TEXCOORD0;
            };

            v2f vert(appdata_full v)
            {
                v2f o;
                o.pos = UnityWorldToClipPos(v.vertex.xyz);
                o.lineWidth = v.texcoord.x;
                return o;
            }

            fixed4 frag(v2f i) : SV_Target
            {
                fixed4 col = _Color;
                // 简单地用线宽调整 alpha：线越细越透明
                col.a *= saturate(i.lineWidth * 0.3);
                return col;
            }
            ENDCG
        }
    }
}
```

**限制**: GL.LINES 固定 1 像素宽，无法真正做抗锯齿。线在缩放/旋转时可能出现闪烁（aliasing），但作为快速原型足够了。

---

## 5. 数据流与生命周期

```
SceneView.OnEnable()
  +-> m_SceneViewGrid.OnEnable(this)
        +-> GridSettings.sizeChanged += GridSizeChanged
        +-> grid.fade.valueChanged.AddListener(view.Repaint)  // 渐变驱动重绘

每帧渲染:
  SceneView.OnGUI()
    +-> m_SceneViewGrid.PrepareGridRender(camera, pivot, rotation, size, orthoMode)
          +-> UpdateGridsVisibility()  // 更新轴面显隐
          +-> ApplySnapConstraints()   // 更新线间距
          +-> return DrawGridParameters

  [传递到 C++]
    +-> DrawGrid(camera, &params)
          +-> DoDrawGrid(camera, &params, axis)  // GPU 即时模式绘制

GridSettings.size 变化:
  +-> sizeChanged(Vector3)
        +-> GridSizeChanged()
              +-> 各轴面 pivot 吸附到新网格

SceneView.OnDisable()
  +-> m_SceneViewGrid.OnDisable(this)
        +-> GridSettings.sizeChanged -= GridSizeChanged
        +-> grid.fade.valueChanged.RemoveListener(view.Repaint)
```

### C# 到 C++ 的绑定

Unity 使用 `BIND_MANAGED_TYPE_NAME` 宏将 C# 和 C++ 的 `DrawGridParameters` 结构体直接映射：

```cpp
BIND_MANAGED_TYPE_NAME(DrawGridParameters, UnityEditor_DrawGridParameters);
```

这意味着 C# 端创建 `DrawGridParameters` 后，通过 internal call 传递到 C++ 端时，内存布局完全一致，无需手动序列化。

---

## 6. 纯 C# 等效实现方案

如果你需要在非 Unity Editor 环境下（或纯 C# Unity Runtime）实现相同效果，可以用 `GL` 类替代 C++ 的 `GfxDevice::ImmediateBegin/End`。

### 6.1 完整渲染类

```csharp
using UnityEngine;

public static class WorldGridRenderer
{
    const int kMultiplier = 10;
    const int kStepSizeCount = 4;
    const float kMaxLineStrength = 5f;
    const float kGridOffsetValue = 0.01f;

    static Material s_GridMaterial;

    public struct GridParams
    {
        public int gridID;       // 0=X面(YZ), 1=Y面(XZ), 2=Z面(XY)
        public Vector3 pivot;
        public Color color;
        public Vector2 size;     // (dim0线间距, dim1线间距)
    }

    public static void Draw(Camera camera, GridParams gridParam)
    {
        if (gridParam.color.a <= 0f) return;
        DoDraw(camera, gridParam, gridParam.gridID);
    }

    static void DoDraw(Camera camera, GridParams gridParam, int axis)
    {
        if (s_GridMaterial == null)
        {
            // 使用方案 B 的四边形式 Shader（全平台兼容）
            s_GridMaterial = new Material(Shader.Find("Custom/GridLineQuad"));
            s_GridMaterial.hideFlags = HideFlags.HideAndDontSave;
        }

        Transform t = camera.transform;
        Vector3 camPos = t.position;
        float camDist = Mathf.Abs(camPos[axis]);

        // 裁剪
        if (camDist > camera.farClipPlane) return;
        if (!float.IsFinite(camDist * camDist)) return;

        bool ortho = camera.orthographic;
        Vector3 dir = t.forward;

        // 覆盖范围
        float vertHalf = ortho
            ? camera.orthographicSize
            : camDist * Mathf.Tan(camera.fieldOfView * 0.5f * Mathf.Deg2Rad);
        float aspectFix = Mathf.Sqrt(camera.aspect) * Mathf.Sqrt(2f);
        float groundHalf = vertHalf * aspectFix;

        // 4级线间距表
        float[][] steps = new float[2][];
        for (int d = 0; d < 2; d++)
        {
            steps[d] = new float[kStepSizeCount];
            steps[d][0] = (d == 0) ? gridParam.size.x : gridParam.size.y;
            for (int s = 1; s < kStepSizeCount; s++)
                steps[d][s] = steps[d][s - 1] * kMultiplier;
        }

        int axis2 = (axis + 1) % 3;
        int axis3 = (axis + 2) % 3;

        GL.PushMatrix();
        s_GridMaterial.SetPass(0);
        // 关键：使用 GL.QUADS 而非 GL.LINES，配合 Custom/GridLineQuad Shader
        // 每条线段 → 4 个顶点（一个四边形），片段着色器做抗锯齿
        GL.Begin(GL.QUADS);
        GL.Color(gridParam.color);

        for (int dim = 0; dim < 2; dim++)
        {
            for (int s = 0; s < kStepSizeCount; s++)
            {
                float sz = steps[dim][s];
                bool biggest = (s == kStepSizeCount - 1);

                if (sz < camera.nearClipPlane) continue;

                int gridSize = Mathf.FloorToInt((groundHalf / sz) * 500f);
                gridSize = Mathf.Min(gridSize,
                    (int)Mathf.Ceil(camera.farClipPlane / sz) + 1);
                gridSize = Mathf.Clamp(gridSize, 2, 100);

                int c0 = Mathf.RoundToInt(camPos[axis2] / sz);
                int c1 = Mathf.RoundToInt(camPos[axis3] / sz);

                float cellInit = sz * 5f / groundHalf;

                int axA = (dim == 0) ? axis2 : axis3;
                int axB = (dim == 0) ? axis3 : axis2;

                for (int i = c0 - gridSize; i <= c0 + gridSize; i++)
                {
                    int modI = (i % kMultiplier + kMultiplier) % kMultiplier;
                    if (modI == 0 && !biggest) continue;

                    float cellSize = cellInit;
                    if (!ortho) cellSize *= camDist;
                    cellSize = Mathf.Min(cellSize, kMaxLineStrength);

                    float err = groundHalf / camDist;

                    // 线段两端的世界坐标
                    Vector3 vStart = gridParam.pivot;
                    vStart[axA] = i * sz;
                    vStart[axB] = (c1 - gridSize) * sz;

                    Vector3 vEnd = gridParam.pivot;
                    vEnd[axA] = i * sz;
                    vEnd[axB] = (c1 + gridSize) * sz;

                    // ---- 四边形生成：将线段扩展为带线宽的四边形 ----

                    // 1. 计算世界空间中的半宽
                    float worldHalfWidth;
                    if (ortho)
                    {
                        // 正交：像素 → 世界单位换算
                        worldHalfWidth = cellSize * camera.orthographicSize * 2f / camera.pixelHeight * 0.5f;
                    }
                    else
                    {
                        // 透视：基于线段中点到相机的距离
                        Vector3 mid = (vStart + vEnd) * 0.5f;
                        float dist = (mid - camPos).magnitude;
                        worldHalfWidth = cellSize * dist
                            * Mathf.Tan(camera.fieldOfView * 0.5f * Mathf.Deg2Rad)
                            * 2f / camera.pixelHeight * 0.5f;
                    }

                    // 2. 线段方向 + 横向法线
                    Vector3 lineDir = (vEnd - vStart).normalized;
                    Vector3 toCamera = (camPos - (vStart + vEnd) * 0.5f).normalized;
                    Vector3 lineNormal = Vector3.Cross(lineDir, toCamera).normalized;
                    // 确保法线不退化（相机恰好与线段平行时）
                    if (lineNormal.sqrMagnitude < 0.001f)
                        lineNormal = Vector3.Cross(lineDir, Vector3.up).normalized;

                    // 3. Z-fighting 偏移
                    Vector3 zOffset;
                    if (ortho)
                        zOffset = -dir * kGridOffsetValue * camDist * err;
                    else
                        zOffset = Vector3.zero; // 透视模式每顶点独立计算

                    // 4. 四边形 4 个顶点（逆时针）：左下 → 右下 → 右上 → 左上
                    //    使用 texcoord1.x 标记横向偏移 [-1, +1]，供片段着色器做抗锯齿
                    Vector3 offset = lineNormal * worldHalfWidth;

                    // 左下（texcoord1.x = -1）
                    GL.TexCoord2(cellSize, -1f);
                    GL.Vertex(vStart - offset + zOffset);

                    // 右下（texcoord1.x = +1）
                    GL.TexCoord2(cellSize,  1f);
                    GL.Vertex(vStart + offset + zOffset);

                    // 右上（texcoord1.x = +1）
                    GL.TexCoord2(cellSize,  1f);
                    GL.Vertex(vEnd   + offset + zOffset);

                    // 左上（texcoord1.x = -1）
                    GL.TexCoord2(cellSize, -1f);
                    GL.Vertex(vEnd   - offset + zOffset);
                }
            }
        }

        GL.End();
        GL.PopMatrix();
    }
}
```

### 6.2 调用示例

```csharp
// 挂到 Camera 上
[RequireComponent(typeof(Camera))]
public class GridOverlay : MonoBehaviour
{
    void OnPostRender()
    {
        var grid = new WorldGridRenderer.GridParams
        {
            gridID = 1,    // Y轴面（水平网格）
            pivot  = Vector3.zero,
            color  = new Color(0.5f, 0.5f, 0.5f, 0.4f),
            size   = new Vector2(1f, 1f)  // 1m 基础间距
        };
        WorldGridRenderer.Draw(GetComponent<Camera>(), grid);
    }
}
```

### 6.3 性能分析

- **顶点数量**: 每帧最多 ~3200 个顶点（800 线段 × 4 顶点/四边形），对现代 GPU 微不足道
- **GL.QUADS 开销**: 即时模式下 ~0.1ms（编辑器场景），相比 GL.LINES 顶点数翻倍但仍极低
- **片段着色器**: 每条四边形在屏幕空间覆盖若干像素，smoothstep 抗锯齿开销极低
- **优化方向**: 若追求极致性能，可预生成 Mesh 并用 `Graphics.DrawMesh` 一帧提交，但会失去动态裁剪能力；或使用 Geometry Shader（方案 A）将 CPU 侧顶点减半

### 6.4 轴面选择简化版

不需要三轴面系统时，可用简化逻辑：

```csharp
int DetermineAxis(Camera cam)
{
    if (!cam.orthographic) return 1;  // 透视默认 Y

    Vector3 fwd = cam.transform.forward;
    if (fwd == Vector3.up   || fwd == Vector3.down)    return 1; // Y面
    if (fwd == Vector3.left || fwd == Vector3.right)    return 0; // X面
    if (fwd == Vector3.forward || fwd == Vector3.back) return 2; // Z面
    return 1;
}
```

---

## 7. 关键常量和默认值速查表

| 常量 | 值 | 说明 | 来源 |
|------|-----|------|------|
| `k_GridSizeMin` | 0.0001 | 最小网格尺寸 | SceneViewGrid.cs |
| `k_GridSizeMax` | 1024 | 最大网格尺寸 | SceneViewGrid.cs |
| `k_DefaultGridSize` | 1.0 | 默认网格尺寸 (1m) | SceneViewGrid.cs |
| `k_DefaultMultiplier` | 0 | 默认倍数 (1x) | SceneViewGrid.cs |
| `k_DefaultGridOpacity` | 0.5 | 默认不透明度 | SceneViewGrid.cs |
| `k_DefaultRenderAxis` | Y | 默认轴面 | SceneViewGrid.cs |
| `k_DefaultShowGrid` | true | 默认显示网格 | SceneViewGrid.cs |
| `kViewGridColor` | (0.5, 0.5, 0.5, 0.4) | 网格颜色(灰, 40%透明) | SceneViewGrid.cs |
| `k_AngleThresholdForOrthographicGrid` | 0.15 | 正交网格角度阈值 | SceneViewGrid.cs |
| `kMultiplier` | 10 | 每级线间距倍数 | DrawGrid.cpp |
| `kStepSizeCount` | 4 | 线间距级别数 | DrawGrid.cpp |
| `kMaxLineStrength` | 5.0 | 最大线宽 | DrawGrid.cpp |
| `kGridOffsetValue` | 0.01 | Z-fighting 偏移系数 | DrawGrid.cpp |
| gridSize clamp | [2, 100] | 单轴最大线段数 | DrawGrid.cpp |
| Grid.mat | 透视模式材质 | 内置 Editor 资源 | DrawGrid.cpp |
| GridOrtho.mat | 正交模式材质 | 内置 Editor 资源 | DrawGrid.cpp |

---

## 附录 A: 源码文件索引

| 文件 | 路径 | 行数 | 职责 |
|------|------|------|------|
| SceneViewGrid.cs | `Editor/Mono/SceneView/SceneViewGrid.cs` | 418 | C# 参数管理、轴面状态、渐变 |
| SceneView.cs | `Editor/Mono/SceneView/SceneView.cs` | 3900 | SceneView 窗口主类，持有 SceneViewGrid 实例 |
| DrawGrid.h | `Editor/Src/Utility/DrawGrid.h` | 20 | DrawGridParameters 结构体定义 |
| DrawGrid.cpp | `Editor/Src/Utility/DrawGrid.cpp` | 155 | 网格渲染核心实现 |

## 附录 B: 算法流程图

```
PrepareGridRender()
  |
  +-> UpdateGridsVisibility()
  |     |
  |     +-> orthoMode?
  |     |     Yes: 选对齐轴面
  |     |     No:  按 gridAxis 设置
  |     +-> 设置 grid.fade.target
  |
  +-> ApplySnapConstraints()
  |     +-> 从 GridSettings.size 更新各轴面的 line spacing
  |
  +-> 返回 DrawGridParameters
        |
        v
  DrawGrid(camera, &params)
        |
        +-> color.a > 0?  ---No---> return
              |
             Yes
              |
              v
  DoDrawGrid(camera, &params, axis)
        |
        +-> camPos[axis] > far? ---Yes---> return
        +-> !IsFinite?           ---Yes---> return
        |
        +-> 计算 groundHalfCoverage
        +-> 构建 4 级 stepSizes 表
        +-> ImmediateBegin(kPrimitiveLines, material)
        |
        +-> for dim in [0, 1]:       // 两个方向
        |     for s in [0..3]:       // 四个级别
        |       for i in [-N..+N]:   // 遍历线段
        |         |
        |         +-> 10x 去重检查
        |         +-> 计算 cellSize (线宽)
        |         +-> 计算 gridOffset (Z-fighting)
        |         +-> ImmediateVertex(起点)
        |         +-> ImmediateVertex(终点)
        |
        +-> ImmediateEnd()
```
