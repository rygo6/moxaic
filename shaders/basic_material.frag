#version 450

#include "global_binding.glsl"
#include "binding_material.glsl"
#include "object_binding.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;

void main()
{
    outColor = texture(materialTexture, inUV);
    const vec4 viewNormal = globalUBO.view * vec4(inNormal, 1);
    outNormal = vec4(normalize(viewNormal.xyz), 1.0);
}
