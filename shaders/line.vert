#version 450

#include "global_binding.glsl"
#include "binding_line_material.glsl"

layout(location = 0) in vec3 inPosition;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = globalUBO.proj * globalUBO.view * vec4(inPosition, 1.0);
}