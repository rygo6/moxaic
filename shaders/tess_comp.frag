#version 450

#include "global_binding.glsl"
#include "binding_node.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 color = textureLod(nodeColor, inUV, 0);
    if (color.a == 0)
        discard;

    outColor = color;
}