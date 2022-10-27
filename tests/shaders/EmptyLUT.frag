#version 460 core

layout(location = 0) in vec2 UV;
layout(location = 0) uniform int img_size;
layout(location = 1) uniform int lut_size;

layout(location = 0) out vec3 EmptyLUT;

void main()
{
    float pixel_size = 1.0f / img_size;
    float pixel_offset = pixel_size / 2.0f;
    
    // R and G values with pixel size compensation to have 0.0 and 1.0 values in pixel center
    EmptyLUT.r = mod((UV.x - pixel_offset) * lut_size, 1) * (img_size + lut_size) / img_size;
    EmptyLUT.g = mod((UV.y - 1.0f - pixel_offset) * lut_size, 1) * (img_size + lut_size) / img_size;
    
    float gv = floor(lut_size + lut_size * (UV.y - 1.0f)) / (lut_size - 1);
    float gh = floor(lut_size + lut_size * (UV.x - 1.0f)) / (lut_size - 1);
    EmptyLUT.b = (lut_size * gv + gh) / (lut_size + 1);
}