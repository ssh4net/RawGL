#version 460 core

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv_co;

layout(location = 0) out vec2 UV;

void main()
{
  UV = uv_co;
  gl_Position = vec4(pos.x, pos.y, 0.0f, 1.0f);
}