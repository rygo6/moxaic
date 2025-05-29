//// Subgroup Grid

/*

  // Single Subgroup. 4x4 samples. 8x8 quad samples
    0     1     2     3
  0 00    00    00    00
    00    00    00    00

  1 00    00    00    00
    00    00    00    00


  2 00    00    00    00
    00    00    00    00

  3 00    00    00    00
    00    00    00    00


    01    22
  0 11    11
  1 11    11

  2 11    11
  2 11    11


    02
  0 22
  2 22


    0
  0 3

  // Workgroup. 8x8. 32x32 samples. 64x64 quad samples
    0 1 2 3 4 5 6 7
  0 g g g g g g g g
  1 g g g g g g g g
  3 g g g g g g g g
  4 g g g g g g g g
  5 g g g g g g g g
  6 g g g g g g g g
  7 g g g g g g g g

// this should use morton encoding
    0  1  2  3  4  5  6  7
  0 00 01 02 03 04 05 06 07
  1 08 09 10 11 12 13 14 15
  2 16 17 18 19 20 21 22 23
  3 24 25 26 27 28 29 30 31
  4 32 33 34 35 36 37 38 39
  5 40 41 42 43 44 45 46 47
  6 48 49 50 51 52 53 54 55
  7 56 57 58 59 60 61 62 63


*/

#define QUAD_SQUARE_SIZE 2

#define SUBGROUP_SQUARE_SIZE 4
#define SUBGROUP_CAPACITY 16 // 4 * 4

#define WORKGROUP_SQUARE_SIZE 8
#define WORKGROUP_SUBGROUP_COUNT 64 // 8 * 8

const int quadSelf = 3;
const ivec2 quadGatherOffsets[4] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 }, };

// Dimension of entire grid
ivec2 grid_Dimensions;

// Index of the workgroup globally
uint grid_GlobalWorkgroupIndex;

// Where 0,0 root grid coord of the workgroup is globally
ivec2 grid_GlobalWorkgroupCoord;

// Index of subgroup globally
uint grid_GlobalSubgroupIndex;

// Index of subgroup in local workgroup
uint grid_LocalSubgroupIndex;

// Where 0,0 root grid coord of the subgroup is in local workgroup
ivec2 grid_LocalSubgroupCoord;

// X/Y ID of subgroup in local workgroup
ivec2 grid_LocalSubgroupID;

// Index in subgroup
uint grid_SubgroupIndex;

// Grid Coord in subgroup
ivec2 grid_SubgroupCoord;

// Grid Coord in local workgroup
ivec2 grid_LocalCoord;

// Grid Coordinate globally
ivec2 grid_GlobalCoord;

uint IndexFromID(ivec2 id, uint dimension) {
    return id.x + (id.y * dimension);
}

ivec2 GlobalWorkgroupCoordFromIndex(uint globalWorkgroupIndex, ivec2 size) {
    size /= ivec2(WORKGROUP_SQUARE_SIZE, WORKGROUP_SQUARE_SIZE);
    size /= ivec2(SUBGROUP_SQUARE_SIZE, SUBGROUP_SQUARE_SIZE);
    return ivec2(globalWorkgroupIndex % size.x, globalWorkgroupIndex / size.x) * WORKGROUP_SQUARE_SIZE * SUBGROUP_SQUARE_SIZE;
}

ivec2 LocalSubgroupIDFromIndex(uint localWorkgroupIndex) {
    return ivec2(localWorkgroupIndex % WORKGROUP_SQUARE_SIZE, localWorkgroupIndex / WORKGROUP_SQUARE_SIZE);
}

ivec2 LocalSubgroupCoordFromIndex(uint localWorkgroupIndex) {
    return ivec2(localWorkgroupIndex % WORKGROUP_SQUARE_SIZE, localWorkgroupIndex / WORKGROUP_SQUARE_SIZE) * SUBGROUP_SQUARE_SIZE;
}

uint LocalSubgroupIndexFromID(ivec2 id) {
    return id.x + (id.y * WORKGROUP_SQUARE_SIZE);
}

uint LocalSubgroupIndexFromOffset(ivec2 coordDxDy) {
    return grid_LocalSubgroupIndex + coordDxDy.x + (coordDxDy.y * WORKGROUP_SQUARE_SIZE);
}

ivec2 SubgroupCoordFromIndex(uint subgroupIndex) {
    subgroupIndex %= SUBGROUP_CAPACITY;
    return ivec2(subgroupIndex % SUBGROUP_SQUARE_SIZE, subgroupIndex / SUBGROUP_SQUARE_SIZE);
}

// SubgroupIndex by specified offset
uint SubgroupIndexFromOffset(ivec2 coordDxDy) {
    return gl_SubgroupInvocationID + coordDxDy.x + (coordDxDy.y * SUBGROUP_SQUARE_SIZE);
}

// SubgroupIndex from subgroup id
uint SubgroupIndexFromCoord(ivec2 coord) {
    uint subgroupHalf = gl_SubgroupInvocationID / SUBGROUP_CAPACITY;
    return coord.x + (coord.y * SUBGROUP_SQUARE_SIZE) + (subgroupHalf * SUBGROUP_CAPACITY);
}

void InitializeSubgroupGridInfo(ivec2 dimensions) {
    grid_Dimensions = dimensions;

    grid_GlobalWorkgroupIndex = gl_WorkGroupID.y;
    grid_GlobalWorkgroupCoord = GlobalWorkgroupCoordFromIndex(grid_GlobalWorkgroupIndex, grid_Dimensions);

    grid_GlobalSubgroupIndex = gl_GlobalInvocationID.y;
    grid_LocalSubgroupIndex = gl_GlobalInvocationID.y % WORKGROUP_SUBGROUP_COUNT;

    grid_LocalSubgroupCoord = LocalSubgroupCoordFromIndex(grid_LocalSubgroupIndex);
    grid_LocalSubgroupID = LocalSubgroupIDFromIndex(grid_LocalSubgroupIndex);

    grid_SubgroupIndex = gl_SubgroupInvocationID % SUBGROUP_CAPACITY;
    grid_SubgroupCoord = SubgroupCoordFromIndex(grid_SubgroupIndex);

    grid_LocalCoord = (grid_SubgroupCoord + grid_LocalSubgroupCoord);
    grid_GlobalCoord = (grid_SubgroupCoord + grid_LocalSubgroupCoord + grid_GlobalWorkgroupCoord);
}

void InitializeSubgroupGridQuadInfo(ivec2 dimensions) {
    grid_Dimensions = dimensions / 2;

    grid_GlobalWorkgroupIndex = gl_WorkGroupID.y;
    grid_GlobalWorkgroupCoord = GlobalWorkgroupCoordFromIndex(grid_GlobalWorkgroupIndex, grid_Dimensions);

    grid_GlobalSubgroupIndex = gl_GlobalInvocationID.y;
    grid_LocalSubgroupIndex = gl_GlobalInvocationID.y % WORKGROUP_SUBGROUP_COUNT;

    grid_LocalSubgroupCoord = LocalSubgroupCoordFromIndex(grid_LocalSubgroupIndex);
    grid_LocalSubgroupID = LocalSubgroupIDFromIndex(grid_LocalSubgroupIndex);

    grid_SubgroupIndex = gl_SubgroupInvocationID % SUBGROUP_CAPACITY;
    grid_SubgroupCoord = SubgroupCoordFromIndex(grid_SubgroupIndex);

    grid_LocalCoord = (grid_SubgroupCoord + grid_LocalSubgroupCoord) * 2;
    grid_GlobalCoord = (grid_SubgroupCoord + grid_LocalSubgroupCoord + grid_GlobalWorkgroupCoord) * 2;
}
