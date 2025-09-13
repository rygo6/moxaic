#version 450
#extension GL_GOOGLE_include_directive : require

#include "global_binding.glsl"
#include "binding_node.glsl"
#include "std_vertex.glsl"
#include "common_util.glsl"

layout(location = 0) out vec2 outUV;

float doubleWide = 1.0f;
bool clipped = false;
bool cropped = false;

void main()
{
    mat4 nodeViewProj    = nodeState[push.nodeHandle].viewProj;
    mat4 nodeInvViewProj = nodeState[push.nodeHandle].invViewProj;
    mat4 nodeModel       = nodeState[push.nodeHandle].model;
    vec2 nodeSwapSize    = nodeState[push.nodeHandle].framebufferSize;
    vec2 nodeULUV        = nodeState[push.nodeHandle].ulUV;
    vec2 nodeLRUV        = nodeState[push.nodeHandle].lrUV;

    vec4 originClipPos = nodeViewProj * nodeModel * vec4(0,0,0,1);
    float originDepth  = originClipPos.z / originClipPos.w;

    vec2 scale    = vec2(nodeSwapSize) / vec2(globalUBO.screenSize);
    vec2 scaledUV = inUV * scale;
    vec2 nodeUv   = mix(nodeULUV, nodeLRUV, inUV);

//    vec2 finalUv = clipped ?
//        vec2(scaledUV.x / doubleWide, scaledUV.y) :
//        vec2(inUV.x / doubleWide, inUV.y);
    vec2 finalUv = nodeUv;
    outUV = finalUv;

    vec2 nodeNdc       = NDCFromUV(nodeUv);
    vec4 nodeClipPos   = ClipPosFromNDC(nodeNdc, originDepth);
    vec3 worldPos      = WorldPosFromClipPos(nodeInvViewProj, nodeClipPos);
    vec4 globalClipPos = GlobalClipPosFromWorldPos(worldPos);
    gl_Position = globalClipPos;
}