#version 450 core

layout(location = 0) in vec3 a_Position;

out vec3 v_TexCoord;    // 方向向量（用于采样 Cubemap）

uniform mat4 u_SkyboxVP;   // 天空盒 VP 矩阵（View 移除平移 x Projection，由 SkyboxPass 自动设置）

uniform float u_Rotation;  // Y 轴旋转角度（度）

void main()
{
    // 应用 Y 轴旋转
    float rad = radians(u_Rotation);
    float cosR = cos(rad);
    float sinR = sin(rad);
    vec3 rotatedPos = vec3(
        a_Position.x * cosR + a_Position.z * sinR,
        a_Position.y,
        -a_Position.x * sinR + a_Position.z * cosR
    );
    
    // 旋转后的位置作为 Cubemap 采样方向
    v_TexCoord = rotatedPos;
    
    // 计算裁剪空间位置
    vec4 pos = u_SkyboxVP * vec4(a_Position, 1.0);
    
    // 将 z 设为 w，使透视除法后深度恒为 1.0（最远处）
    gl_Position = pos.xyww;
}
