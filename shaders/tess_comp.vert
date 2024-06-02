#version 450

#include "global_binding.glsl"
#include "node_binding.glsl"
#include "std_vertex.glsl"

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;

void main() {
//    gl_Position = vec4(inPos.xyz, 1.0);
    outNormal = inNormal;
    outUV = inUV;
}