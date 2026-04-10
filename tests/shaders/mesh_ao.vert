#version 450 core

layout(location = 0) in vec3 pos;
layout(location = 3) in uvec4 color_rgba;

layout(location = 0) out float AO;

void main()
{
    AO = float(color_rgba.r) / 255.0;
    gl_Position = vec4(pos, 1.0);
}
