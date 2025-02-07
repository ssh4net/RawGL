#version 460 core

layout(location = 0) in vec2 UV;
layout(location = 0) uniform int img_size;
layout(location = 1) uniform int lut_size;

layout(location = 0) out vec3 EmptyLUT;

void main()
{
    vec2 pixel_size = 1.0f / vec2(img_size);
    vec2 pixel_offset = pixel_size / 2.0;

    ivec2 square = ivec2(img_size / lut_size);
    
    vec2 uv = (1 + img_size) * (UV - pixel_offset);
    
    // R and G values with pixel size compensation to have 0.0 and 1.0 values in pixel center
    EmptyLUT.rg = mod(uv, square) / (square - 1);
    vec2 g = mod(uv / square, square) / (lut_size - 1);
    EmptyLUT.b = ((lut_size - 1) * g.y + g.x) / (lut_size);
}