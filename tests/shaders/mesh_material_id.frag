#version 450 core

layout(location = 0) flat in uint MaterialId;

layout(location = 0) out vec4 OutSample;

void main()
{
    OutSample = MaterialId == 1u ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
}
