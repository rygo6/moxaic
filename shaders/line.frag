#version 450

#include "binding_line_material.glsl"

layout(location = 0) out vec4 outColor;

void main() {
    outColor = lineMaterial.primaryColor;
}