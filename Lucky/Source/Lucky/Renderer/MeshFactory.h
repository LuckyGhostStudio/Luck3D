#pragma once

#include "Mesh.h"

namespace Lucky
{
    /// <summary>
    /// 图元类型
    /// </summary>
    enum class PrimitiveType
    {
        None = 0,
        
        Cube,       // 立方体
        Plane,      // 平面
        Sphere,     // 经纬球
        Cylinder,   // 圆柱体
        Capsule,    // 胶囊体
    };
    
    /// <summary>
    /// 网格工厂：用于创建原生图元网格
    /// </summary>
    class MeshFactory
    {
    public:
        /// <summary>
        /// 根据图元类型创建网格（统一入口）
        /// </summary>
        /// <param name="type">图元类型</param>
        /// <returns>创建的网格，类型为 None 时返回 nullptr</returns>
        static Ref<Mesh> CreatePrimitive(PrimitiveType type);
        
        /// <summary>
        /// 创建立方体网格
        /// 中心在原点，边长为 1（范围 [-0.5, 0.5]）
        /// 24 个顶点（每面 4 个独立顶点，用于独立法线）
        /// 36 个索引（12 个三角形）
        /// </summary>
        /// <returns>立方体网格</returns>
        static Ref<Mesh> CreateCube();
        
        /// <summary>
        /// 创建平面网格
        /// XZ 平面，中心在原点，大小为 1×1（范围 [-0.5, 0.5]）
        /// 法线朝上 (0, 1, 0)
        /// </summary>
        /// <param name="subdivisions">细分次数，默认为 1（即 2 个三角形）
        /// subdivisions=1 时：4 个顶点，6 个索引
        /// subdivisions=10 时：121 个顶点，600 个索引（与 Unity 默认 Plane 一致）</param>
        /// <returns>平面网格</returns>
        static Ref<Mesh> CreatePlane(uint32_t subdivisions = 1);
        
        /// <summary>
        /// 创建球体网格（UV Sphere）
        /// 中心在原点，半径为 0.5（直径为 1）
        /// 使用经纬线（UV Sphere）算法
        /// </summary>
        /// <param name="segments">经线段数（水平方向），默认 32，最小 3</param>
        /// <param name="rings">纬线段数（垂直方向），默认 16，最小 2</param>
        /// <returns>球体网格</returns>
        static Ref<Mesh> CreateSphere(uint32_t segments = 32, uint32_t rings = 16);
        
        /// <summary>
        /// 创建圆柱体网格
        /// 中心在原点，高度为 1（范围 Y: [-0.5, 0.5]），半径为 0.5
        /// 包含顶面、底面和侧面
        /// </summary>
        /// <param name="segments">圆周段数，默认 32，最小 3</param>
        /// <returns>圆柱体网格</returns>
        static Ref<Mesh> CreateCylinder(uint32_t segments = 32);
        
        /// <summary>
        /// 创建胶囊体网格
        /// 中心在原点，总高度为 2（圆柱部分高度 1 + 上下半球各半径 0.5）
        /// 半径为 0.5
        /// </summary>
        /// <param name="segments">圆周段数，默认 32，最小 3</param>
        /// <param name="rings">每个半球的纬线段数，默认 8，最小 2</param>
        /// <returns>胶囊体网格</returns>
        static Ref<Mesh> CreateCapsule(uint32_t segments = 32, uint32_t rings = 8);
        
    private:
        /// <summary>
        /// 获取图元类型对应的名称字符串
        /// </summary>
        /// <param name="type">图元类型</param>
        /// <returns>名称字符串</returns>
        static const char* GetPrimitiveTypeName(PrimitiveType type);
    };
}
