#version 450
#extension GL_GOOGLE_include_directive : require

#include "global_binding.glsl"
#include "node_binding.glsl"
#include "std_vertex.glsl"
#include "common_util.glsl"

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;

float doubleWide = 1.0f;
bool clipped = false;
bool cropped = false;

void main() {
    vec4 originClipPos = nodeUBO.viewProj * nodeUBO.model * vec4(0,0,0,1);
    float originDepth = originClipPos.z / originClipPos.w;

    vec2 scale = vec2(nodeUBO.framebufferSize) / vec2(globalUBO.screenSize);
    vec2 scaledUV = inUV * scale;
    vec2 nodeUv = mix(nodeUBO.ulUV, nodeUBO.lrUV, inUV);

//    vec2 finalUv = clipped ?
//        vec2(scaledUV.x / doubleWide, scaledUV.y) :
//        vec2(inUV.x / doubleWide, inUV.y);
    vec2 finalUv = nodeUv;
    outUV = finalUv;

    vec2 nodeNdc = NDCFromUV(nodeUv);
    vec4 nodeClipPos = ClipPosFromNDC(nodeNdc, originDepth);
    vec3 worldPos = WorldPosFromNodeClipPos(nodeClipPos);
    vec4 globalClipPos = GlobalClipPosFromWorldPos(worldPos);
    gl_Position = globalClipPos;

    outNormal = inNormal;
}