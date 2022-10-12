#version 460 core

layout(binding = 0) uniform sampler2D InSample3;

layout(location = 0) uniform int _LOD;

layout(location = 0) in vec2 UV;
layout(location = 0) out vec3 OutSample3;

vec2 ImageSize = textureSize(InSample3,_LOD);

void main()
{
    OutSample3 = textureLod(InSample3,UV,_LOD).rgb;
    OutSample3 *= vec3(UV.x,1.0,UV.y);
}