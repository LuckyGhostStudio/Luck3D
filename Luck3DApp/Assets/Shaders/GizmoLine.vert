#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    vec3 Position;
} u_Camera;

out vec4 v_Color;

void main()
{
    v_Color = a_Color;
    gl_Position = u_Camera.ViewProjectionMatrix * vec4(a_Position, 1.0);
}