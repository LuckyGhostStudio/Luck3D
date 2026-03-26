#pragma once

#include "Mesh.h"

namespace Lucky
{
    /// <summary>
    /// 网格工厂：用于创建网格
    /// </summary>
    class MeshFactory
    {
    public:
        /// <summary>
        /// 创建立方体网格
        /// </summary>
        /// <returns></returns>
        static Ref<Mesh> CreateCube();
    };
}
