#pragma once

#include "Mesh.h"

namespace Lucky
{
    /// <summary>
    /// 网格切线空间计算工具
    /// 为网格的每个顶点计算 Tangent 向量和 Handedness
    /// </summary>
    class MeshTangentCalculator
    {
    public:
        /// <summary>
        /// 为网格计算切线空间（Tangent + Handedness）
        /// 基于 Lengyel 的切线空间计算算法
        /// 
        /// 算法步骤：
        /// 1. 遍历每个三角形，计算面切线和面副切线
        /// 2. 将面切线/副切线累积到三角形的三个顶点（面积加权）
        /// 3. 对每个顶点进行 Gram-Schmidt 正交化
        /// 4. 计算 handedness（±1）
        /// </summary>
        /// <param name="vertices">顶点数组（会被修改，填充 Tangent 字段）</param>
        /// <param name="indices">索引数组</param>
        static void Calculate(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    };
}