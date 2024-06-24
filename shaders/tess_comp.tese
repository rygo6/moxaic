#version 450

#include "global_binding.glsl"
#include "node_binding.glsl"
#include "common_util.glsl"

layout(quads, equal_spacing, cw) in;

layout (location = 0) in vec3 inNormals[];
layout (location = 1) in vec2 inUVs[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec4 outWorldPos;

void main()
{
    const vec2 inUV = mix(
        mix(inUVs[0], inUVs[1], gl_TessCoord.x),
        mix(inUVs[3], inUVs[2], gl_TessCoord.x),
        gl_TessCoord.y);

    const vec2 scale = clamp(vec2(nodeUBO.framebufferSize) / vec2(globalUBO.screenSize), 0, 1);
    const vec2 scaledUV = inUV * scale;
    // im reversing the lr and ul from what i'd expect... why!?
    const vec2 ndcUV = mix(nodeUBO.lrUV, nodeUBO.ulUV, inUV);

    float alphaValue = texture(nodeColor, scaledUV).a;
    float depthValue = texture(nodeGBuffer, scaledUV).r;

    vec2 ndc = NDCFromUV(ndcUV);
    vec4 clipPos = ClipPosFromNDC(ndc, depthValue);
    vec3 worldPos = WorldPosFromNodeClipPos(clipPos);
    gl_Position = globalUBO.viewProj * vec4(worldPos, 1.0f);

    outUV = scaledUV;

    outNormal = mix(
        mix(inNormals[0], inNormals[1], gl_TessCoord.x),
        mix(inNormals[3], inNormals[2], gl_TessCoord.x),
        gl_TessCoord.y);
}