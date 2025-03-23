layout (set = 1, binding = 0) uniform NodeUBO {
    mat4 model;

    // Laid out to be alignment with GlobalSet
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 invView;
    mat4 invProj;
    mat4 invViewProj;
    ivec2 framebufferSize;

    vec2 ulUV;
    vec2 lrUV;
} nodeUBO;

layout (set = 1, binding = 1) uniform sampler2D nodeColor;
layout (set = 1, binding = 2) uniform sampler2D nodeGBuffer;