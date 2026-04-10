#version 450 core

layout(location = 0) out vec4 OutSample[1];

void main()
{
    OutSample[0] = vec4(1.0, 0.0, 0.0, 1.0);
}
