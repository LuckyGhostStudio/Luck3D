#pragma once

#include "Lucky/Renderer/RenderPass.h"
#include "Lucky/Renderer/VertexArray.h"
#include "Lucky/Renderer/Buffer.h"

namespace Lucky
{
    /// <summary>
    /// 天空盒渲染 Pass（Material 驱动）
    /// 在 OpaquePass 之后渲染，利用 Early-Z 跳过被遮挡的天空像素
    /// 渲染状态：深度测试 LessEqual + 深度写入 OFF + 面剔除 Front
    /// 属于 "Main" 分组
    /// 
    /// 通过 Material 驱动渲染：SkyboxPass 不直接持有纹理，
    /// 而是从 RenderContext 获取 SkyboxMaterial，通过 Material 绑定 Shader 和纹理。
    /// 这样未来支持程序化天空只需写新 Shader，无需修改 SkyboxPass。
    /// </summary>
    class SkyboxPass : public RenderPass
    {
    public:
        void Init() override;
        void Execute(const RenderContext& context) override;
        
        const std::string& GetName() const override
        {
            static std::string name = "SkyboxPass";
            return name;
        }
        
        const std::string& GetGroup() const override
        {
            static std::string group = "Main";
            return group;
        }
        
    private:
        Ref<VertexArray> m_CubeVAO;     // 天空盒 Cube VAO（仅位置属性）
        Ref<VertexBuffer> m_CubeVBO;    // 天空盒 Cube VBO
    };
}
