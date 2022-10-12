#version 460 core

layout(binding = 0) uniform sampler2D InSample2;

layout(location = 0) in vec2 UV;
layout(location = 0) out vec3 OutSample2;

vec2 ImageSize = textureSize(InSample2,0);

void main()
{
    OutSample2 = textureLod(InSample2,UV,0).rgb;
    OutSample2.y = 0.0;
}