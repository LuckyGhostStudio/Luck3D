#version 450 core

// 输出到颜色缓冲区 1（R32I Entity ID 缓冲区）
layout(location = 1) out int o_EntityID;

uniform int u_EntityID;

void main()
{
    o_EntityID = u_EntityID;
}
