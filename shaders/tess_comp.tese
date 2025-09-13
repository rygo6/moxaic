#version 450

#include "global_binding.glsl"
#include "binding_node.glsl"
#include "common_util.glsl"

layout(quads, equal_spacing, cw) in;

layout (location = 0) in vec3 inNormals[];
layout (location = 1) in vec2 inUVs[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec4 outWorldPos;

float doubleWide = 1.0f;
bool clipped = false;

void main()
{
    mat4 nodeViewProj    = nodeState[push.nodeHandle].viewProj;
    mat4 nodeInvView     = nodeState[push.nodeHandle].invView;
    mat4 nodeInvProj     = nodeState[push.nodeHandle].invProj;
    mat4 nodeInvViewProj = nodeState[push.nodeHandle].invViewProj;
    mat4 nodeModel       = nodeState[push.nodeHandle].model;
    vec2 nodeSwapSize    = nodeState[push.nodeHandle].framebufferSize;
    vec2 nodeULUV        = nodeState[push.nodeHandle].ulUV;
    vec2 nodeLRUV        = nodeState[push.nodeHandle].lrUV;

    vec2 inUV = mix(
        mix(inUVs[0], inUVs[1], gl_TessCoord.x),
        mix(inUVs[3], inUVs[2], gl_TessCoord.x),
        gl_TessCoord.y);
    vec2 scale = clamp(vec2(nodeSwapSize) / vec2(globalUBO.screenSize), 0, 1);
    vec2 scaledUV = inUV * scale;
    vec2 nodeUv = mix(nodeULUV, nodeLRUV, inUV);

//    vec2 finalUv = clipped ?
//        vec2(scaledUV.x / doubleWide, scaledUV.y) :
//        vec2(inUV.x / doubleWide, inUV.y);
    vec2 finalUv = nodeUv;
    outUV = finalUv;

    float alphaValue = texture(nodeColor[push.nodeHandle], finalUv).a;
    float depthValue = texture(nodeGBuffer[push.nodeHandle], finalUv).r;

    vec2 nodeNdc = NDCFromUV(nodeUv);
    vec4 nodeClipPos = ClipPosFromNDC(nodeNdc, depthValue);
    vec3 worldPos = WorldPosFromClipPos(nodeInvViewProj, nodeClipPos);
    vec4 globalClipPos = GlobalClipPosFromWorldPos(worldPos);
    gl_Position = globalClipPos;

    outNormal = mix(
        mix(inNormals[0], inNormals[1], gl_TessCoord.x),
        mix(inNormals[3], inNormals[2], gl_TessCoord.x),
        gl_TessCoord.y);
}