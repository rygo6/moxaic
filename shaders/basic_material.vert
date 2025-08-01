#version 450

#include "global_binding.glsl"
#include "object_binding.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;

mat4 getIdentityMatrix() {
    return mat4(1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0);
}

void main() {
    gl_Position = globalUBO.proj * globalUBO.view * objectUBO.model * vec4(inPosition, 1.0);
    outNormal = inNormal;
    outUV = inUV;
}