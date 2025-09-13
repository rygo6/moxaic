#version 450

#include "global_binding.glsl"
#include "binding_node.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 color = texture(nodeColor[push.nodeHandle], inUV);
    if (color.a == 0)
        discard;

    outColor = color;
}
