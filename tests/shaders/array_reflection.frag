#version 450 core

uniform float weights[4];
layout(location = 0) out vec4 OutArray[2];

void main()
{
    const float sum = weights[0] + weights[1] + weights[2] + weights[3];
    OutArray[0] = vec4(sum, weights[0], weights[1], 1.0);
    OutArray[1] = vec4(weights[2], weights[3], sum, 1.0);
}
