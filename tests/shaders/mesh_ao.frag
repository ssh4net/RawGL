#version 450 core

layout(location = 0) in float AO;

layout(location = 0) out vec4 OutSample;

void main()
{
    OutSample = vec4(AO, AO, AO, 1.0);
}
