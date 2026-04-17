#version 450 core

layout(location = 0) out vec4 OutArray[2];

void main()
{
    OutArray[0] = vec4(1.0, 0.0, 0.0, 1.0);
    OutArray[1] = vec4(0.0, 1.0, 0.0, 1.0);
}
