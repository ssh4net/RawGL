#version 460 core

layout(binding = 0) uniform sampler2D InSample;

layout(location = 0) in vec2 UV;
layout(location = 0) out vec3 OutSample;

vec2 ImageSize = textureSize(InSample,0);

void main()
{
    OutSample = textureLod(InSample,UV,0).rgb;
    OutSample.x = 1.0;
}