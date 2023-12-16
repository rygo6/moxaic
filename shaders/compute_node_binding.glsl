layout (set = 1, binding = 0) uniform NodeUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 invView;
    mat4 invProj;
    mat4 invViewProj;
    int width;
    int height;
} nodeUBO;

layout (set = 1, binding = 1) uniform sampler2D nodeColorTexture;
layout (set = 1, binding = 2) uniform sampler2D nodeNormalTexture;
layout (set = 1, binding = 3) uniform sampler2D nodeGBufferTexture;
layout (set = 1, binding = 4) uniform sampler2D nodeDepthTexture;

#define RESOLUTION_SCALE 1
#define LOCAL_SIZE 32 * RESOLUTION_SCALE