#version 450

#include "binding_global.glsl"
#include "binding_basic_material.glsl"
#include "binding_object.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 outWorldPos;

void main() {
    gl_Position = globalUBO.proj * globalUBO.view * objectUBO.model * vec4(inPosition, 1.0);
    outNormal = inNormal;
    outUV = inUV;
    outWorldPos = (objectUBO.model * vec4(inPosition, 1.0)).xyz;
}