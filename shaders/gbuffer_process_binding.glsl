#define LOCAL_SIZE 32

struct DepthState {
    float minDepth;
    float maxDepth;
    float nearZ;
    float farZ;
};

layout (set = 0, binding = 0) uniform ProcessState {
    DepthState depth;
    float cameraNearZ;
    float cameraFarZ;
} processState;

layout (set = 0, binding = 1) uniform sampler2D srcMip;
layout (set = 0, binding = 2) uniform sampler2D srcGbuffer;
layout (set = 0, binding = 3, rgba16f) writeonly uniform image2D dstGbuffer;
