#version 450

#include "global_binding.glsl"
#include "binding_node.glsl"
#include "std_vertex.glsl"

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;

void main() {
    // could remove pos and normal from pipe entirely !?
//    gl_Position = vec4(inPos.xyz, 1.0);
    outNormal = inNormal;
    outUV = inUV;
}