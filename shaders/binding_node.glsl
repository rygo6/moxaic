layout (std140, set = 1, binding = 0) uniform NodeState {

    mat4 model;

    // Laid out flat to align with std140
    // struct GlobalSetState
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 invView;
    mat4 invProj;
    mat4 invViewProj;
    ivec2 framebufferSize;

    // Clip
    vec2 ulUV;
    vec2 lrUV;
} nodeState;

layout (set = 1, binding = 1) uniform sampler2D nodeColor;
layout (set = 1, binding = 2) uniform sampler2D nodeGBuffer;