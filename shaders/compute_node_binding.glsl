layout (set = 1, binding = 0) uniform NodeUBO {
        mat4 view;
        mat4 proj;
        mat4 invView;
        mat4 invProj;
        int width;
        int height;
} nodeUBO;

layout (set = 1, binding = 1, rgba8) uniform readonly image2D inputColor;
layout (set = 1, binding = 2, rgba8) uniform readonly image2D inputNormal;
layout (set = 1, binding = 3, rgba8) uniform readonly image2D inputGBuffer;
layout (set = 1, binding = 4, rgba8) uniform readonly image2D inputDepth;

layout (set = 1, binding = 5, rgba8) uniform writeonly image2D outputColor;