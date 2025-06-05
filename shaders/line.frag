#version 450

#include "binding_line_material.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;

void main() {
    outColor = lineMaterial.primaryColor;
}