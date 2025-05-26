//// Subgroup Grid
#define QUAD_SQUARE_SIZE 2

#define SUBGROUP_SQUARE_SIZE 4
#define SUBGROUP_CAPACITY 16 // 4 * 4

#define WORKGROUP_SQUARE_SIZE 8
#define WORKGROUP_SUBGROUP_COUNT 64 // 8 * 8

#define WORKGROUP_SAMPLE_COUNT (WORKGROUP_SUBGROUP_COUNT / 2)

const int quadSelf = 3;
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

ivec2 grid_Dimensions;
uint grid_GlobalWorkgroupIndex;
ivec2 grid_GlobalWorkgroupId;
uint grid_LocalSubgroupIndex;
ivec2 grid_LocalWorkgroupID;
uint grid_GlobalSubgroupIndex;
uint grid_SubgroupInvocationIndex;
ivec2 grid_SubgroupID;
ivec2 grid_GlobalCoord;

void InitializeSubgroupGridInfo(ivec2 dimensions) {
    grid_Dimensions = dimensions;
    grid_GlobalWorkgroupIndex = gl_WorkGroupID.y;
    grid_GlobalWorkgroupId = GlobalWorkgroupID(grid_GlobalWorkgroupIndex, grid_Dimensions);
    grid_LocalSubgroupIndex = gl_GlobalInvocationID.y % WORKGROUP_SUBGROUP_COUNT;
    grid_LocalWorkgroupID = LocalWorkgroupID(grid_LocalSubgroupIndex);
    grid_GlobalSubgroupIndex = gl_GlobalInvocationID.y;
    grid_SubgroupInvocationIndex = gl_SubgroupInvocationID;
    grid_SubgroupID = SubgroupID(grid_SubgroupInvocationIndex);
    grid_GlobalCoord = (grid_SubgroupID + grid_LocalWorkgroupID + grid_GlobalWorkgroupId);
}

void InitializeSubgroupGridQuadInfo(ivec2 dimensions) {
    grid_Dimensions = dimensions / 2;
    grid_GlobalWorkgroupIndex = gl_WorkGroupID.y;
    grid_GlobalWorkgroupId = GlobalWorkgroupID(grid_GlobalWorkgroupIndex, grid_Dimensions);
    grid_LocalSubgroupIndex = gl_GlobalInvocationID.y % WORKGROUP_SUBGROUP_COUNT;
    grid_LocalWorkgroupID = LocalWorkgroupID(grid_LocalSubgroupIndex);
    grid_GlobalSubgroupIndex = gl_GlobalInvocationID.y;
    grid_SubgroupInvocationIndex = gl_SubgroupInvocationID;
    grid_SubgroupID = SubgroupID(grid_SubgroupInvocationIndex);
    grid_GlobalCoord = (grid_SubgroupID + grid_LocalWorkgroupID + grid_GlobalWorkgroupId) * 2;
}
