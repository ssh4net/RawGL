#version 460 core

layout(location = 0) in vec2 UV;

layout(location = 0) out vec3 STmap;

void main()
{
    STmap = vec3(UV,0.0);
}