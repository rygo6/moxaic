#version 450

#include "global_binding.glsl"
#include "node_binding.glsl"
#include "std_vertex.glsl"
#include "common_util.glsl"

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;

void main() {
    const vec4 originClipPos = nodeUBO.viewProj * nodeUBO.model * vec4(0,0,0,1);

    const vec2 scale = vec2(nodeUBO.framebufferSize) / vec2(globalUBO.screenSize);
    const vec2 scaledUV = inUV * scale;
    // im reversing the lr and ul from what i'd expect... why!?
    const vec2 ndcUV = mix(nodeUBO.lrUV, nodeUBO.ulUV, inUV);

    vec2 nodeNDC = NDCFromUV(ndcUV);
    vec4 nodeClipPos = ClipPosFromNDC(nodeNDC, originClipPos.z / originClipPos.w);
    vec3 worldPos = WorldPosFromNodeClipPos(nodeClipPos);
    gl_Position = globalUBO.viewProj * vec4(worldPos, 1.0f);

    outNormal = inNormal;

    outUV = scaledUV;
}