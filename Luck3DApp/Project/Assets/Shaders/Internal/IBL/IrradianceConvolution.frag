#version 450 core

in vec3 v_WorldPos;

layout(location = 0) out vec4 o_Color;

uniform samplerCube u_EnvironmentMap;   // 原始环境 Cubemap

const float PI = 3.14159265359;

void main()
{
    // 法线方向 = 当前片元对应的 Cubemap 方向
    vec3 N = normalize(v_WorldPos);
    
    vec3 irradiance = vec3(0.0);
    
    // 构建切线空间
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));
    
    // 半球均匀采样（球面坐标遍历）
    float sampleDelta = 0.025;  // 采样步长（越小越精确，越慢）
    float nrSamples = 0.0;
    
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // 球面坐标 → 切线空间方向
            vec3 tangentSample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta)
            );
            
            // 切线空间 → 世界空间
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            
            // 余弦加权采样（cos(theta) = tangentSample.z）
            // sin(theta) 是球面积分的雅可比行列式
            irradiance += texture(u_EnvironmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    
    // 归一化：π 来自漫反射 BRDF 的 1/π 因子
    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    
    o_Color = vec4(irradiance, 1.0);
}
