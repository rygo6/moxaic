#version 450

#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080

#include "binding_global.glsl"
#include "binding_basic_material.glsl"
#include "binding_object.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outGBuffer;

void main()
{
    float depth = gl_FragCoord.z;
    vec2 normalizedFragCoord = vec2(gl_FragCoord.x / globalUBO.width, gl_FragCoord.y / globalUBO.height);
    vec4 clipPos = vec4(normalizedFragCoord * 2.0 - 1.0, depth, 1.0);
    vec4 eyePos = globalUBO.invProj * clipPos;
    vec3 ndcPos = eyePos.xyz / eyePos.w;
    vec4 worldPos = globalUBO.invView * vec4(ndcPos, 1.0);

     outColor = texture(texSampler, inUV);
    // outColor = worldPos;
//    outColor = vec4(0, 1, 0, 1);

    // Is this right?! https://github.com/SaschaWillems/Vulkan/blob/master/data/shaders/glsl/subpasses/gbuffer.frag
    vec3 N = normalize(inNormal);
    N.y = -N.y;
    outNormal = vec4(N, 1.0);

    outGBuffer = worldPos;
}
