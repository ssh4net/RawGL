#version 450 core

#ifdef RAWGL_VERTEX_SHADER
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv_co;

void main()
{
    gl_Position = vec4(pos, 0.0, 1.0);
}
#endif

#ifdef RAWGL_FRAGMENT_SHADER
layout(location = 0) out vec4 OutSample;

void main()
{
    OutSample = vec4(1.0, 0.25, 0.5, 1.0);
}
#endif
