#version 450

#include "global_binding.glsl"
#include "node_binding.glsl"
#include "std_vertex.glsl"

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;

void main() {
    const vec3 right = normalize(vec3(globalUBO.view[0][0], globalUBO.view[1][0], globalUBO.view[2][0]));
    const vec3 up = normalize(vec3(globalUBO.view[0][1], globalUBO.view[1][1], globalUBO.view[2][1]));
    const vec3 pos = right * inPos.x + up * inPos.y;
    gl_Position = globalUBO.viewProj * nodeUBO.model * vec4(pos, 1);

    outNormal = inNormal;

    const vec2 scale = clamp(vec2(nodeUBO.framebufferSize) / vec2(globalUBO.screenSize), 0, 1);
    const vec2 scaledUV = inUV * scale;
    // im reversing the lr and ul from what i'd expect... why!?
    const vec2 ndcUV = mix(nodeUBO.lrUV, nodeUBO.ulUV, inUV);

    outUV = scaledUV;
}