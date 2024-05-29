#version 450

#include "global_binding.glsl"
#include "node_binding.glsl"
#include "common_util.glsl"

layout (set = 3, binding = 2) uniform sampler2D color;
layout (set = 3, binding = 3) uniform sampler2D normal;
layout (set = 3, binding = 4) uniform sampler2D depth;

layout(quads, equal_spacing, cw) in;

layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec2 inUV[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec4 outWorldPos;

void main()
{
    outUV = mix(
        mix(inUV[0], inUV[1], gl_TessCoord.x),
        mix(inUV[3], inUV[2], gl_TessCoord.x),
        gl_TessCoord.y);

    outNormal = mix(
        mix(inNormal[0], inNormal[1], gl_TessCoord.x),
        mix(inNormal[3], inNormal[2], gl_TessCoord.x),
        gl_TessCoord.y);

    vec4 pos = mix(
        mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x),
        mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x),
        gl_TessCoord.y);

    float alphaValue = texture(nodeColor, outUV).a;
    float depthValue = texture(nodeGBuffer, outUV).r;

    vec2 ndc = NDCFromUV(outUV);
    vec4 clipPos = ClipPosFromNDC(ndc, depthValue);

    // Convert clip space coordinates to eye space coordinates
    vec4 eyePos = nodeUBO.invProj * clipPos;
    // Divide by w component to obtain normalized device coordinates
    vec3 ndcPos = eyePos.xyz / eyePos.w;
    // Convert NDC coordinates to world space coordinates
    vec4 worldPos = inverse(nodeUBO.view) * vec4(ndcPos, 1.0);

//    gl_Position = alphaValue > 0 ?
//        globalUBO.proj * globalUBO.view * worldPos :
//        globalUBO.proj * globalUBO.view * objectUBO.model * pos;

    gl_Position = globalUBO.proj * globalUBO.view * worldPos;

    outWorldPos = worldPos;
}