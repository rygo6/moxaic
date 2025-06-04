#version 450
#extension GL_EXT_control_flow_attributes : enable

#include "global_binding.glsl"
#include "binding_material.glsl"
#include "object_binding.glsl"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor * texture(materialTexture, fragTexCoord).rgb, 1.0);

    //infinite loop test
    vec4 sum = vec4(0);
    for (int i = 1; i != 2; i += 2) {
        sum += vec4(0.1, 0.1, 0.1, 0.1);
    }
    outColor = sum;
}
