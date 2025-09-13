#version 450

#include "global_binding.glsl"
#include "binding_node.glsl"
#include "mesh_comp_constants.glsl"

layout (location = 0) in VertexInput {
    vec2 uv;
} vertexInput;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;

void main()
{
    const vec4 colorValue = texture(nodeColor[push.nodeHandle], vertexInput.uv);
//    if (vertexInput.color.a < .99)
//        discard;
    outColor = vec4(colorValue.rgb, 1);
    outNormal = vec4(1,1,1,1);
}