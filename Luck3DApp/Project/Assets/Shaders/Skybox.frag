#version 450 core

in vec2 v_ClipXY;                   // 裁剪空间 xy（NDC 范围 [-1, 1]）

uniform samplerCube u_SkyboxMap;    // Cubemap 纹理
uniform mat4 u_InverseVP;           // 逆 VP（View 已移除平移；由 SkyboxPass 自动设置）

uniform float u_Rotation;           // Y 轴旋转角度（度）
uniform float u_Exposure;           // 曝光调整（默认 1.0）
uniform vec4 u_Tint;                // 色调调整（默认白色）

layout(location = 0) out vec4 o_Color;

void main()
{
    // 通过近/远平面反投影反算像素对应的世界方向
    // 对透视投影：near/far 是同一视线上的两点，direction = far - near
    // 对正交投影：所有像素反算出的方向平行（正交观察 skybox 的正确表现）
    vec4 nearWS = u_InverseVP * vec4(v_ClipXY, -1.0, 1.0);
    vec4 farWS  = u_InverseVP * vec4(v_ClipXY,  1.0, 1.0);
    vec3 dir = normalize(farWS.xyz / farWS.w - nearWS.xyz / nearWS.w);

    // 应用 Y 轴旋转
    float rad = radians(u_Rotation);
    float cosR = cos(rad);
    float sinR = sin(rad);
    dir = vec3(
        dir.x * cosR + dir.z * sinR,
        dir.y,
        -dir.x * sinR + dir.z * cosR
    );

    vec3 color = texture(u_SkyboxMap, dir).rgb;
    color *= u_Exposure;
    color *= u_Tint.rgb;

    o_Color = vec4(color, 1.0);
}
