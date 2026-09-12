#version 450 core

in vec2 v_TexCoord;

layout(location = 0) out vec2 o_Color;  // RG16F 输出

const float PI = 3.14159265359;

// ---- Hammersley 序列（低差异序列，用于准蒙特卡洛积分） ----

// Van der Corput 序列（位反转）
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;  // 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// ---- GGX 重要性采样 ----
// 根据 GGX NDF 分布生成采样方向（切线空间）
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;

    // 球面坐标
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // 切线空间半向量
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // 切线空间 → 世界空间
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

// ---- Smith's Schlick-GGX 几何遮蔽（IBL 版本，k = α2/2） ----
float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;  // IBL 使用 k = α2/2（与直接光照的 k = (r+1)2/8 不同）

    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith_IBL(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX_IBL(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX_IBL(NdotL, roughness);
    return ggx1 * ggx2;
}

// ---- BRDF 积分 ----
vec2 IntegrateBRDF(float NdotV, float roughness)
{
    // 假设 N = (0, 0, 1)，V 在 XZ 平面
    vec3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);  // sin(theta)
    V.y = 0.0;
    V.z = NdotV;                        // cos(theta)

    float A = 0.0;  // F0 的缩放因子
    float B = 0.0;  // F0 的偏移因子

    vec3 N = vec3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float G = GeometrySmith_IBL(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);

    return vec2(A, B);
}

void main()
{
    // v_TexCoord.x = NdotV, v_TexCoord.y = roughness
    o_Color = IntegrateBRDF(v_TexCoord.x, v_TexCoord.y);
}
