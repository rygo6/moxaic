#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_shuffle : require

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
  0  g g g g g g g
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
#define QUAD_COUNT 2

#define SUBGROUP_SQUARE_SIZE 4
#define SUBGROUP_COUNT 16 // 4 * 4
const vec2 SUBGGROUP_SQUARE_DIMENSIONS = vec2(SUBGROUP_SQUARE_SIZE, SUBGROUP_SQUARE_SIZE);

#define WORKGROUP_SQUARE_SIZE 8
#define WORKGROUP_SUBGROUP_COUNT 64 // 8 * 8
const vec2 WORKGROUP_SQUARE_DIMENSIONS = vec2(WORKGROUP_SQUARE_SIZE, WORKGROUP_SQUARE_SIZE);

/*
    0 1
    3 2
*/
#define QUAD_SELF 3
const ivec2 QUAD_OFFSETS[4] = {
    { 0, 1 }, { 1, 1 },
    { 1, 0 }, { 0, 0 },
};

// Dimension of entire grid
ivec2 grid_Dimensions;

bool grid_GlobalFirstInvocation;

// Index of the workgroup globally
uint grid_GlobalWorkgroupID;

// Where 0,0 root grid coord of the workgroup is globally
ivec2 grid_GlobalWorkgroupCoord;

// Index of subgroup quad globally
uint grid_GlobalSubgroupQuadID;

bool grid_LocalFirstInvocation;

// Local index of subgroup invocation in workgroup
uint grid_SubgroupQuadID;

// Invocation quad morton coord in local workgroup
uvec2 grid_SubgroupQuadMortonCoord;

//// Invocation quad linear coord in local workgroup
//ivec2 grid_SubgroupQuadCoord;

// Linear coord of subgroup quad
ivec2 grid_SubgroupQuadCoord;

// ID in quad subgroup. Happens to be the same for linear and morton.
uint grid_InvocationSubgroupQuadID;

// Starting index of subgroup quad in subgroup
uint grid_InvocationSubgroupQuadBaseID;

// Morton coord of invocation in subgroup quad
uvec2 grid_InvocationSubgroupQuadMortonCoord;

// Invocation quad linear coord in subgroup
ivec2 grid_InvocationSubgroupQuadCoord;

ivec2 grid_InvocationQuadCoord;

uvec2 grid_InvocationQuadMortonCoord;

// Invocation Quad linear Coord globally
ivec2 grid_GlobalInvocationQuadCoord;

// Invocation Quad Coord globally. Within the workgroup it us morton. Outside the workgroup it is linear.
uvec2 grid_GlobalInvocationQuadMortonCoord;

// Grid UV globally
vec2 grid_GlobalInvocationUV;

// Grid UV globally
vec2 grid_GlobalInvocationMortonUV;

/*

   Morton Encoding

      0    1    2    3
   ┌─────────┬─────────┬
 0 │ 00 → 01 │ 04 → 05 │
   │    ↙    │    ↙    │
 1 │ 02 → 03 │ 06 → 07 │
   ├──────── ↙ ────────┤
 2 │ 08 → 09 │ 12 → 13 │
   │    ↙    │     ↙   │
 3 │ 10 → 11 │ 14 → 15 │
   └─────────┴─────────┘

      0    1    2    3    4    5    6    7
   ┌─────────┬─────────┬─────────┬─────────┐
 0 │ 00 → 01 │ 04 → 05 │ 16 → 17 │ 20 → 21 │
   │    ↙    │    ↙    │    ↙    │    ↙    │
 1 │ 02 → 03 │ 06 → 07 │ 18 → 19 │ 22 → 23 │
   ├──────── ↙ ────────┼──────── ↙ ────────┤
 2 │ 08 → 09 │ 12 → 13 │ 24 → 25 │ 28 → 29 │
   │    ↙    │    ↙    │    ↙    │    ↙    │
 3 │ 10 → 11 │ 14 → 15 │ 26 → 27 │ 30 → 31 │
   ├─────────┼──────── ↙ ────────┼─────────┤
 4 │ 32 → 33 │ 36 → 37 │ 48 → 49 │ 52 → 53 │
   │    ↙    │    ↙    │    ↙    │    ↙    │
 5 │ 34 → 35 │ 38 → 39 │ 50 → 51 │ 54 → 55 │
   ├──────── ↙ ────────┼──────── ↙ ────────┤
 6 │ 40 → 41 │ 44 → 45 │ 56 → 57 │ 60 → 61 │
   │    ↙    │    ↙    │    ↙    │    ↙    │
 7 │ 42 → 43 │ 46 → 47 │ 58 → 59 │ 62 → 63 │
   └─────────┴─────────┴─────────┴─────────┘

*/

// this is upside down     // LL LR UL UR // no its lerpquad2 is upside down
#define QUAD_UL 0
#define QUAD_UR 1
#define QUAD_LL 2
#define QUAD_LR 3

uint MortonQuadRoot(uint cornerMorton) {
    // zero out bottom 2 bits to get root
    return cornerMorton & ~3u;
}

uint MortonShuffleHorizontal(uint morton) {
    return morton ^ 1u;
}

uint MortonShuffleVertical(uint morton) {
    return morton ^ 2u;
}

uint MortonShuffleDiagonal(uint morton) {
    return morton ^ 3u;
}

uint MortonShuffleHorizontalAtLevel(uint morton, uint level) {
    return morton ^ (1u << (level * 2u));
}

// Vertical shuffle at specific level
uint MortonShuffleVerticalAtLevel(uint morton, uint level) {
    return morton ^ (2u << (level * 2u));
}

// Diagonal shuffle at specific level
uint MortonShuffleDiagonalAtLevel(uint morton, uint level) {
    return morton ^ (3u << (level * 2u));
}

// Get parent at specific level (clear bits below that level)
uint MortonQuadRootAtLevel(uint morton, uint level) {
    uint mask = ~((1u << (level * 2u)) - 1u);
    return morton & mask;
}

uvec4 MortonQuadAtLevel(uint rootMorton, uint level) {
    return uvec4(
        rootMorton,                                  // UL
        MortonShuffleHorizontalAtLevel(rootMorton, level),  // UR
        MortonShuffleVerticalAtLevel(rootMorton, level),    // LL
        MortonShuffleDiagonalAtLevel(rootMorton, level)     // LR
    );
}

uvec4 MortonQuad(uint rootMorton) {
    return uvec4(
        rootMorton,                           // UL
        MortonShuffleHorizontal(rootMorton),  // UR
        MortonShuffleVertical(rootMorton),    // LL
        MortonShuffleDiagonal(rootMorton)     // LR
    );
}

// 32 bit Morton
uint MortonEncode32bit(uint x, uint y) {
    // Limit to 16-bit coordinates for 32-bit result
    x &= 0x0000FFFFu;
    y &= 0x0000FFFFu;

    x = (x | (x << 8)) & 0x00FF00FFu;
    x = (x | (x << 4)) & 0x0F0F0F0Fu;
    x = (x | (x << 2)) & 0x33333333u;
    x = (x | (x << 1)) & 0x55555555u;

    y = (y | (y << 8)) & 0x00FF00FFu;
    y = (y | (y << 4)) & 0x0F0F0F0Fu;
    y = (y | (y << 2)) & 0x33333333u;
    y = (y | (y << 1)) & 0x55555555u;

    return x | (y << 1);
}

uvec2 MortonDecode32bit(uint morton) {
    uint x = morton & 0x55555555u;
    uint y = (morton >> 1) & 0x55555555u;

    x = (x | (x >> 1)) & 0x33333333u;
    x = (x | (x >> 2)) & 0x0F0F0F0Fu;
    x = (x | (x >> 4)) & 0x00FF00FFu;
    x = (x | (x >> 8)) & 0x0000FFFFu;

    y = (y | (y >> 1)) & 0x33333333u;
    y = (y | (y >> 2)) & 0x0F0F0F0Fu;
    y = (y | (y >> 4)) & 0x00FF00FFu;
    y = (y | (y >> 8)) & 0x0000FFFFu;

    return uvec2(x, y);
}

// 8 bit Morton (256x256 space, fits in 16 bits)
uint MortonEncode8bit(uint x, uint y) {
    x &= 0xFFu;
    y &= 0xFFu;

    x = (x | (x << 4)) & 0x0F0Fu;
    x = (x | (x << 2)) & 0x3333u;
    x = (x | (x << 1)) & 0x5555u;

    y = (y | (y << 4)) & 0x0F0Fu;
    y = (y | (y << 2)) & 0x3333u;
    y = (y | (y << 1)) & 0x5555u;

    return x | (y << 1);
}

uvec2 MortonDecode8bit(uint morton) {
    uint x = morton & 0x5555u;        // Extract X bits: 0101010101010101
    uint y = (morton >> 1) & 0x5555u; // Extract Y bits: 1010101010101010 shifted

    x = (x | (x >> 1)) & 0x3333u;     // 0101... → 0011001100110011
    x = (x | (x >> 2)) & 0x0F0Fu;     // 0011... → 0000111100001111
    x = (x | (x >> 4)) & 0x00FFu;     // 0000... → 0000000011111111

    y = (y | (y >> 1)) & 0x3333u;
    y = (y | (y >> 2)) & 0x0F0Fu;
    y = (y | (y >> 4)) & 0x00FFu;

    return uvec2(x, y);
}

// 4 bit Morton (16x16 space, fits in 8 bits)
uint MortonEncode4bit(uint x, uint y) {
    x &= 0xFu;
    y &= 0xFu;

    x = (x | (x << 2)) & 0x33u; // 0b00110011
    x = (x | (x << 1)) & 0x55u; // 0b01010101

    y = (y | (y << 2)) & 0x33u;
    y = (y | (y << 1)) & 0x55u;

    return x | (y << 1);
}

uvec2 MortonDecode4bit(uint morton) {
    uint x = morton & 0x55u;        // Extract X bits: 01010101 pattern
    uint y = (morton >> 1) & 0x55u; // Extract Y bits: 10101010 pattern shifted

    x = (x | (x >> 1)) & 0x33u;     // Merge pairs: 01010101 → 00110011
    x = (x | (x >> 2)) & 0x0Fu;     // Merge quads: 00110011 → 00001111

    y = (y | (y >> 1)) & 0x33u;
    y = (y | (y >> 2)) & 0x0Fu;

    return uvec2(x, y);
}

uint MortonEncode4bitAtLevel(uint x, uint y, uint level) {
    // Shift coordinates right to get appropriate resolution for this level
    x = (x >> level) & 0xFu;
    y = (y >> level) & 0xFu;

    x = (x | (x << 2)) & 0x33u; // 0b00110011
    x = (x | (x << 1)) & 0x55u; // 0b01010101

    y = (y | (y << 2)) & 0x33u;
    y = (y | (y << 1)) & 0x55u;

    return x | (y << 1);
}

// Decode at a specific level
uvec2 MortonDecode4bitAtLevel(uint morton, uint level) {
    uint x = morton & 0x55u;        // Extract X bits: 01010101 pattern
    uint y = (morton >> 1) & 0x55u; // Extract Y bits: 10101010 pattern shifted

    x = (x | (x >> 1)) & 0x33u;     // Merge pairs: 01010101 → 00110011
    x = (x | (x >> 2)) & 0x0Fu;     // Merge quads: 00110011 → 00001111

    y = (y | (y >> 1)) & 0x33u;
    y = (y | (y >> 2)) & 0x0Fu;

    // Shift back up to original coordinate space
    return uvec2(x << level, y << level);
}

uint IndexFromID(ivec2 id, uint dimension) {
    return id.x + (id.y * dimension);
}

ivec2 GlobalWorkgroupCoordFromIndex(uint globalWorkgroupIndex, ivec2 size) {
    size /= ivec2(WORKGROUP_SQUARE_SIZE, WORKGROUP_SQUARE_SIZE);
    size /= ivec2(SUBGROUP_SQUARE_SIZE, SUBGROUP_SQUARE_SIZE);
    size = max(size, 1);
    return ivec2(globalWorkgroupIndex % size.x, globalWorkgroupIndex / size.x) * WORKGROUP_SQUARE_SIZE * SUBGROUP_SQUARE_SIZE;
}

ivec2 SubgroupQuadCoordFromID(uint localWorkgroupIndex) {
    return ivec2(localWorkgroupIndex % WORKGROUP_SQUARE_SIZE, localWorkgroupIndex / WORKGROUP_SQUARE_SIZE);
}

//ivec2 LocalSubgroupCoordFromIndex(uint localWorkgroupIndex) {
//    return ivec2(localWorkgroupIndex % WORKGROUP_SQUARE_SIZE, localWorkgroupIndex / WORKGROUP_SQUARE_SIZE) * SUBGROUP_SQUARE_SIZE;
//}

uint LocalSubgroupIndexFromID(ivec2 id) {
    return id.x + (id.y * WORKGROUP_SQUARE_SIZE);
}

uint LocalSubgroupIndexFromOffset(ivec2 coordDxDy) {
    return grid_SubgroupQuadID + coordDxDy.x + (coordDxDy.y * WORKGROUP_SQUARE_SIZE);
}

ivec2 SubgroupCoordFromIndex(uint subgroupIndex) {
    subgroupIndex %= SUBGROUP_COUNT;
    return ivec2(subgroupIndex % SUBGROUP_SQUARE_SIZE, subgroupIndex / SUBGROUP_SQUARE_SIZE);
}

// SubgroupIndex by specified offset
uint SubgroupIndexFromOffset(ivec2 coordDxDy) {
    return gl_SubgroupInvocationID + coordDxDy.x + (coordDxDy.y * SUBGROUP_SQUARE_SIZE);
}

// SubgroupIndex from subgroup id
uint SubgroupIndexFromCoord(ivec2 coord) {
    uint subgroupHalf = gl_SubgroupInvocationID / SUBGROUP_COUNT;
    return coord.x + (coord.y * SUBGROUP_SQUARE_SIZE) + (subgroupHalf * SUBGROUP_COUNT);
}

float AverageQuadOmitZero(vec4 quad) {
    vec4 mask = step(HALF_EPSILON, quad);
    float sum = dot(quad, mask);
    float count = dot(vec4(1.0), mask);
    return count > 0.0 ? sum / count : 0.0;
}

vec4 ReplazeZero(vec4 quad, float average) {
    vec4 mask = step(HALF_EPSILON, quad);
    return quad * mask + average * (1.0 - mask);
}

float LerpQuad(vec2 uv, vec4 quad) {
    vec2 invUV = vec2(1.0) - uv;
    return
        quad[QUAD_UL] * (invUV.x * invUV.y) +
        quad[QUAD_UR] * (uv.x * invUV.y) +
        quad[QUAD_LL] * (invUV.x * uv.y) +
        quad[QUAD_LR] * (uv.x * uv.y);
}

float MinQuadOmitZero(vec4 quad) {
    float minValue = 1.0f;
    for (int i = 0; i < 4; ++i)
    minValue = quad[i] > HALF_EPSILON ? min(minValue, quad[i]) : minValue;
    return minValue == 1.0f ? 0.0f : minValue;
}

void InitializeSubgroupGridInfo(ivec2 dimensions) {
    // Invocation = individual pixels and threads
    // SubgroupQuad = 4x4 invocations in subgroup

    grid_Dimensions = dimensions;

    // Workgroups
    grid_GlobalWorkgroupID = gl_WorkGroupID.y;
    grid_GlobalWorkgroupCoord = GlobalWorkgroupCoordFromIndex(grid_GlobalWorkgroupID, grid_Dimensions);

    // Subgroup Quads
    grid_GlobalSubgroupQuadID = gl_GlobalInvocationID.y;
    grid_SubgroupQuadID = grid_GlobalSubgroupQuadID % WORKGROUP_SUBGROUP_COUNT;
    grid_SubgroupQuadMortonCoord = MortonDecode4bit(grid_SubgroupQuadID);
    grid_SubgroupQuadCoord = SubgroupQuadCoordFromID(grid_SubgroupQuadID);

    // Invocations in Subgroup Quad
    grid_InvocationSubgroupQuadID = gl_SubgroupInvocationID % SUBGROUP_COUNT;
    grid_InvocationSubgroupQuadBaseID = (gl_SubgroupInvocationID >> 4) << 4; // ( / 16 * 16 )
    grid_InvocationSubgroupQuadMortonCoord = MortonDecode4bit(grid_InvocationSubgroupQuadID);
    grid_InvocationSubgroupQuadCoord = SubgroupCoordFromIndex(grid_InvocationSubgroupQuadID);



    grid_GlobalFirstInvocation = grid_InvocationSubgroupQuadID == 0 && grid_GlobalWorkgroupID == 0;
    grid_LocalFirstInvocation = grid_InvocationSubgroupQuadID == 0 && grid_SubgroupQuadID == 0;

    grid_InvocationQuadCoord = grid_InvocationSubgroupQuadCoord + (grid_SubgroupQuadCoord * SUBGROUP_SQUARE_SIZE);
    grid_InvocationQuadMortonCoord = grid_InvocationSubgroupQuadMortonCoord + (grid_SubgroupQuadMortonCoord * SUBGROUP_SQUARE_SIZE);
//    grid_InvocationQuadMortonCoord = MortonDecode8bit((grid_GlobalSubgroupQuadID * SUBGROUP_COUNT) + grid_InvocationSubgroundQuadID);

    grid_GlobalInvocationQuadCoord = grid_InvocationQuadCoord + grid_GlobalWorkgroupCoord;
    grid_GlobalInvocationQuadMortonCoord =  grid_InvocationQuadMortonCoord + grid_GlobalWorkgroupCoord;

    grid_GlobalInvocationUV = vec2(grid_GlobalInvocationQuadCoord) / vec2(dimensions);
    grid_GlobalInvocationMortonUV = vec2(grid_GlobalInvocationQuadMortonCoord) / vec2(dimensions);
}

//void InitializeSubgroupGridQuadInfo(ivec2 dimensions) {
//    grid_Dimensions = dimensions / 2;
//
//    grid_GlobalWorkgroupID = gl_WorkGroupID.y;
//    grid_GlobalWorkgroupCoord = GlobalWorkgroupCoordFromIndex(grid_GlobalWorkgroupID, grid_Dimensions);
//
//    grid_GlobalSubgroupQuadID = gl_GlobalInvocationID.y;
//    grid_SubgroupQuadID = gl_GlobalInvocationID.y % WORKGROUP_SUBGROUP_COUNT;
//
//    grid_SubgroupQuadCoord = LocalSubgroupCoordFromIndex(grid_SubgroupQuadID);
//    grid_SubgroupQuadCoord = SubgroupQuadCoordFromID(grid_SubgroupQuadID);
//
//    grid_InvocationSubgroundQuadID = gl_SubgroupInvocationID % SUBGROUP_COUNT;
//    grid_InvocationSubgroupQuadBaseID = (gl_SubgroupInvocationID >> 4) << 4; // / 16 * 16
//
//    grid_InvocationSubgroundQuadID = gl_SubgroupInvocationID % SUBGROUP_COUNT;
//    grid_InvocationSubgroupQuadCoord = SubgroupCoordFromIndex(grid_InvocationSubgroundQuadID);
//
//    grid_GlobalFirstInvocation = grid_InvocationSubgroundQuadID == 0 && grid_GlobalWorkgroupID == 0;
//    grid_LocalFirstInvocation = grid_InvocationSubgroundQuadID == 0 && grid_SubgroupQuadID == 0;
//
//    grid_InvocationWorkgroupQuadCoord = (grid_InvocationSubgroupQuadCoord + grid_SubgroupQuadCoord) * 2;
//    grid_GlobalCoord = (grid_InvocationSubgroupQuadCoord + grid_SubgroupQuadCoord + grid_GlobalWorkgroupCoord) * 2;
//
//    grid_GlobalUV = vec2(grid_GlobalCoord) / vec2(dimensions * 2);
//}
