#version 450

#include "global_binding.glsl"
#include "node_binding.glsl"

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
    vec2 scale = nodeUBO.framebufferSize / 1024.0;

    vec3 right = vec3(globalUBO.view[0][0], globalUBO.view[1][0], globalUBO.view[2][0]);
    vec3 up = vec3(globalUBO.view[0][1], globalUBO.view[1][1], globalUBO.view[2][1]);
    vec3 pos = right * inPosition.x * scale.x + up * inPosition.y * scale.y;

    gl_Position = globalUBO.proj * globalUBO.view * nodeUBO.model * vec4(pos, 1);
    outNormal = inNormal;
    outUV = inUV * scale;
}