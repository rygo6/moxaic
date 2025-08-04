#version 450

#include "global_binding.glsl"
#include "binding_material.glsl"
#include "object_binding.glsl"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(materialTexture, inUV);
}
