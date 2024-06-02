#version 450

#include "global_binding.glsl"
#include "node_binding.glsl"
#include "common_util.glsl"

layout(quads, equal_spacing, cw) in;

layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec2 inUV[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec4 outWorldPos;

void main()
{
    const vec2 uv = mix(
        mix(inUV[0], inUV[1], gl_TessCoord.x),
        mix(inUV[3], inUV[2], gl_TessCoord.x),
        gl_TessCoord.y);

    const vec2 scale = clamp(vec2(nodeUBO.framebufferSize) / vec2(globalUBO.screenSize), vec2(0), vec2(1));
    const vec2 scaledUV = uv * scale;
    const vec2 ndcUV = mix(nodeUBO.ulUV, nodeUBO.lrUV, uv);
//    const vec2 ndcUV = mix(vec2(0,0), vec2(1,1), uv);

    float alphaValue = texture(nodeColor, scaledUV).a;
    float depthValue = texture(nodeGBuffer, scaledUV).r;

    vec2 ndc = NDCFromUV(ndcUV);
    vec4 clipPos = ClipPosFromNDC(ndc, depthValue);
    vec3 worldPos = WorldPosFromNodeClipPos(clipPos);
    gl_Position = globalUBO.viewProj * vec4(worldPos, 1.0f);
//    gl_Position = globalUBO.viewProj * pos;
    outUV = scaledUV;
    outNormal = mix(
    mix(inNormal[0], inNormal[1], gl_TessCoord.x),
    mix(inNormal[3], inNormal[2], gl_TessCoord.x),
    gl_TessCoord.y);
}