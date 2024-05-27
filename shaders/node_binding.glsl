layout (set = 1, binding = 0) uniform NodeUBO {
    mat4 model;

    mat4 view;
    mat4 proj;
    mat4 viewProj;

    mat4 invView;
    mat4 invProj;
    mat4 invViewProj;

    ivec2 framebufferSize;
} nodeUBO;

layout(set = 1, binding = 1) uniform sampler2D nodeColorSampler;