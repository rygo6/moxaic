//// Subgroup Grid
#define SUBGROUP_SQUARE_SIZE 4
#define SUBGROUP_CAPACITY 16 // 4 * 4

#define WORKGROUP_SQUARE_SIZE 8
#define WORKGROUP_SUBGROUP_COUNT 64 // 8 * 8

#define WORKGROUP_SAMPLE_COUNT (WORKGROUP_SUBGROUP_COUNT / 2)

const ivec2 quadGatherOffsets[4] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 }, };

ivec2 GlobalWorkgroupID(uint globalWorkgroupIndex, ivec2 size) {
    size /= ivec2(WORKGROUP_SQUARE_SIZE, WORKGROUP_SQUARE_SIZE);
    size /= ivec2(SUBGROUP_SQUARE_SIZE, SUBGROUP_SQUARE_SIZE);
    return ivec2(globalWorkgroupIndex % size.x, globalWorkgroupIndex / size.x) * WORKGROUP_SQUARE_SIZE * SUBGROUP_SQUARE_SIZE;
}

ivec2 LocalWorkgroupID(uint localSubgroupIndex) {
    return ivec2(localSubgroupIndex % WORKGROUP_SQUARE_SIZE, localSubgroupIndex / WORKGROUP_SQUARE_SIZE) * SUBGROUP_SQUARE_SIZE;
}

ivec2 SubgroupID(uint subgroupInvocIndex) {
    subgroupInvocIndex %= SUBGROUP_CAPACITY;
    return ivec2(subgroupInvocIndex % SUBGROUP_SQUARE_SIZE, subgroupInvocIndex / SUBGROUP_SQUARE_SIZE);
}

uint SubgroupOffsetIndex(ivec2 dxdy) {
    return (gl_SubgroupInvocationID + dxdy.x + (dxdy.y * SUBGROUP_SQUARE_SIZE));
}

uint SubgroupIndex(ivec2 dxdy) {
    uint subgroupIndex = gl_SubgroupInvocationID / SUBGROUP_CAPACITY;
    return dxdy.x + (dxdy.y * SUBGROUP_SQUARE_SIZE) + (subgroupIndex * SUBGROUP_CAPACITY);
}
