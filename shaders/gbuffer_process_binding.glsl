#define GBUFFER_PROCESS_LOCAL_SIZE 32

struct ProcessState {
    float depthNearZ;
    float depthFarZ;
    float cameraNearZ;
    float cameraFarZ;
};

layout(push_constant) uniform Push {
    ProcessState state;
} push;

const int PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT = 0;

const int SET_BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH = 0;
const int SET_BIND_INDEX_GBUFFER_PROCESS_SRC_GBUFFER = 1;
const int SET_BIND_INDEX_GBUFFER_PROCESS_DST_GBUFFER = 2;

layout (set = PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT, binding = SET_BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH) uniform sampler2D srcDepth;
layout (set = PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT, binding = SET_BIND_INDEX_GBUFFER_PROCESS_SRC_GBUFFER) uniform sampler2D srcGbuffer;
layout (set = PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT, binding = SET_BIND_INDEX_GBUFFER_PROCESS_DST_GBUFFER, rgba16f) writeonly uniform image2D dstGbuffer;

