#version 450 core

layout(location = 0) in vec3 pos;
layout(location = 4) in uint material_id;

layout(location = 0) flat out uint MaterialId;

void main()
{
    MaterialId = material_id;
    gl_Position = vec4(pos, 1.0);
}
