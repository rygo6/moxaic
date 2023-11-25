#version 450

#include "global_binding.glsl"
#include "mesh_node_binding.glsl"
#include "mesh_node_constants.glsl"

layout (location = 0) in VertexInput {
    vec2 uv;
} vertexInput;

layout(location = 0) out vec4 outFragColor;

void main()
{
    const vec4 colorValue = texture(nodeColor, vertexInput.uv);
//    if (vertexInput.color.a < .99)
//        discard;
    outFragColor = vec4(colorValue.rgb, 1);
}