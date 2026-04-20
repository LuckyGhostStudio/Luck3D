#version 450 core

// ---- 输入 ----
in vec3 v_NearPoint;    // 近平面世界空间点（从顶点着色器插值）
in vec3 v_FarPoint;     // 远平面世界空间点（从顶点着色器插值）

// ---- 输出 ----
layout(location = 0) out vec4 o_Color;

// ---- Camera UBO（与 GizmoLine.vert 共享 binding=0）----
layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    vec3 Position;
} u_Camera;

// ---- 网格参数 Uniforms ----
uniform float u_GridCellSize;       // 基础网格单元大小（默认 1.0）
uniform float u_GridMinPixels;      // 网格线最小像素宽度（默认 2.0）
uniform float u_GridFadeDistance;   // 网格淡出距离（默认 100.0）

// 轴线颜色
uniform vec4 u_AxisXColor;      // X 轴颜色（默认红色）
uniform vec4 u_AxisZColor;      // Z 轴颜色（默认蓝色）
uniform vec4 u_GridColor;       // 网格线颜色（默认灰色半透明）

/// 计算程序化网格线（Pristine Grid 算法）
/// 使用屏幕空间导数（fwidth）实现抗锯齿
/// worldPos2D: 世界空间 XZ 坐标
/// scale: 网格缩放（1.0 / cellSize）
/// 返回: 网格线强度 [0, 1]
float PristineGrid(vec2 worldPos2D, float scale)
{
    vec2 coord = worldPos2D * scale;
    
    // 屏幕空间导数：每个像素对应的世界空间变化量
    vec2 derivative = fwidth(coord);
    
    // 计算到最近网格线的距离（归一化到像素空间）
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    
    // 取 X 和 Z 方向的最小值（任一方向在线上即可）
    float line = min(grid.x, grid.y);
    
    // 将距离转换为强度
    return 1.0 - min(line / u_GridMinPixels, 1.0);
}

/// 计算单轴方向的网格线强度（仅检测一个方向）
/// coord1D: 单轴世界坐标
/// scale: 网格缩放（1.0 / cellSize）
/// 返回: 该轴方向网格线强度 [0, 1]
float PristineGridAxis(float coord1D, float scale)
{
    float c = coord1D * scale;
    float d = fwidth(c);
    float grid = abs(fract(c - 0.5) - 0.5) / d;
    return 1.0 - min(grid / u_GridMinPixels, 1.0);
}

void main()
{
    // ---- 1. 射线与 Y=0 平面求交 ----
    // 射线参数方程：P = nearPoint + t * (farPoint - nearPoint)
    // Y=0 平面：P.y = 0
    // 解：t = -nearPoint.y / (farPoint.y - nearPoint.y)
    float t = -v_NearPoint.y / (v_FarPoint.y - v_NearPoint.y);
    
    // 世界空间交点
    vec3 worldPos = v_NearPoint + t * (v_FarPoint - v_NearPoint);
    
    // ---- 2. 丢弃不在平面上的片段 ----
    // t < 0 表示交点在相机后方
    if (t < 0.0)
    {
        discard;
    }
    
    // ---- 3. 计算多级 LOD 网格线 ----
    float cellSize = u_GridCellSize;
    
    float grid1 = PristineGrid(worldPos.xz, 1.0 / cellSize);           // 细网格（如 1m）
    float grid2 = PristineGrid(worldPos.xz, 1.0 / (cellSize * 10.0));  // 中网格（如 10m）
    float grid3 = PristineGrid(worldPos.xz, 1.0 / (cellSize * 100.0)); // 粗网格（如 100m）
    
    // ---- 4. 根据相机高度混合 LOD ----
    float cameraHeight = abs(u_Camera.Position.y);
    
    // LOD 混合因子：相机越高，细网格越淡，粗网格越显
    float lod1Fade = 1.0 - smoothstep(cellSize * 5.0,  cellSize * 20.0,  cameraHeight);  // 细网格在 5~20 倍 cellSize 高度淡出
    float lod2Fade = 1.0 - smoothstep(cellSize * 50.0, cellSize * 200.0, cameraHeight);  // 中网格在 50~200 倍 cellSize 高度淡出
    // 粗网格始终显示
    
    // 合并网格线强度（取最大值，避免重叠区域过亮）
    float gridIntensity = max(grid1 * lod1Fade, max(grid2 * lod2Fade, grid3));
    
    // ---- 5. 距离衰减 ----
    float distToCamera = length(worldPos - u_Camera.Position);
    float fade = 1.0 - smoothstep(u_GridFadeDistance * 0.5, u_GridFadeDistance, distToCamera);
    
    gridIntensity *= fade;
    
    // ---- 6. 轴线"染色"方案 ----
    // 核心思想：不单独绘制轴线，而是检测当前片段是否在 x=0 或 z=0 的网格线上
    // 如果是，则将灰色网格线的颜色替换为轴线颜色
    // 这样轴线的"形状"完全由 PristineGrid 算法决定，不会断裂
    
    // 检测当前片段是否在 Z 方向的网格线上（即 x=0 这条竖线）
    // 使用与 PristineGrid 完全相同的算法，但只检测 x 坐标是否在 x=0 网格线上
    // 对于 x=0 这条线：coord = worldPos.x * scale，当 coord 的小数部分接近 0 或 1 时在线上
    // 但我们需要判断的是"最近的网格线是否是 x=0"
    // 方法：检查 round(worldPos.x * scale) == 0，即最近的网格线索引是否为 0
    
    // 使用最细网格的 scale 来判断轴线（轴线始终以最细网格精度显示）
    float scale = 1.0 / cellSize;
    vec2 coord = worldPos.xz * scale;
    
    // 最近的网格线索引
    float nearestGridX = round(coord.x);  // x 方向最近的网格线索引
    float nearestGridZ = round(coord.y);  // z 方向最近的网格线索引（coord.y 对应 worldPos.z）
    
    // 当前片段到最近网格线的归一化距离（与 PristineGrid 相同的计算方式）
    vec2 derivative = fwidth(coord);
    vec2 gridDist = abs(fract(coord - 0.5) - 0.5) / derivative;
    
    // 判断当前片段是否在 X 方向的网格线上（gridDist.x < u_GridMinPixels 表示在线上）
    bool onGridLineX = (gridDist.x < u_GridMinPixels);
    // 判断当前片段是否在 Z 方向的网格线上
    bool onGridLineZ = (gridDist.y < u_GridMinPixels);
    
    // 判断最近的网格线是否是轴线（索引为 0）
    bool isAxisX = onGridLineX && (abs(nearestGridX) < 0.5);  // x=0 的网格线 → Z 轴（蓝色）
    bool isAxisZ = onGridLineZ && (abs(nearestGridZ) < 0.5);  // z=0 的网格线 → X 轴（红色）
    
    // ---- 7. 合成最终颜色 ----
    vec4 color = u_GridColor;
    color.a *= gridIntensity;
    
    // 轴线染色：将灰色网格线替换为轴线颜色（保持相同的强度/alpha）
    if (isAxisZ)
    {
        // z=0 的网格线 → X 轴（红色）
        color.rgb = u_AxisXColor.rgb;
    }
    if (isAxisX)
    {
        // x=0 的网格线 → Z 轴（蓝色）
        color.rgb = u_AxisZColor.rgb;
    }
    
    // 完全透明则丢弃
    if (color.a < 0.001)
    {
        discard;
    }
    
    o_Color = color;
    
    // ---- 8. 手动写入深度 ----
    vec4 clipSpacePos = u_Camera.ViewProjectionMatrix * vec4(worldPos, 1.0);
    float depth = (clipSpacePos.z / clipSpacePos.w) * 0.5 + 0.5;   // NDC [-1,1] → [0,1]
    gl_FragDepth = depth;
}
