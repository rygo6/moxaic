#version 450

#include "global_binding.glsl"
#include "basic_material_binding.glsl"
#include "object_binding.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
//layout(location = 2) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outGBuffer;

void main()
{
    outColor = texture(texSampler, inUV);

    const vec4 viewNormal = globalUBO.view * vec4(inNormal, 1);

    outNormal = vec4(normalize(viewNormal.xyz), 1.0);


    //    float depth = gl_FragCoord.z;
    //    vec2 normalizedFragCoord = gl_FragCoord.xy / globalUBO.screenSize;
    //    vec4 clipPos = vec4(normalizedFragCoord * 2.0 - 1.0, depth, 1.0);
    //    vec4 eyePos = globalUBO.invProj * clipPos;
    //    vec3 ndcPos = eyePos.xyz / eyePos.w;
    //    vec4 worldPos = globalUBO.invView * vec4(ndcPos, 1.0);
    //    outGBuffer = worldPos;
}
