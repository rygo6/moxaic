#version 450

#include "global_binding.glsl"
#include "node_binding.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;

void main() {
    const vec3 right = normalize(vec3(globalUBO.view[0][0], globalUBO.view[1][0], globalUBO.view[2][0]));
    const vec3 up = normalize(vec3(globalUBO.view[0][1], globalUBO.view[1][1], globalUBO.view[2][1]));
    const vec3 pos = right * inPosition.x + up * inPosition.y;
    gl_Position = globalUBO.proj * globalUBO.view * nodeUBO.model * vec4(pos, 1);

    outNormal = inNormal;

    const vec2 scale = clamp(vec2(nodeUBO.framebufferSize) / vec2(globalUBO.screenSize), vec2(0), vec2(1));
    outUV = inUV * scale;
}