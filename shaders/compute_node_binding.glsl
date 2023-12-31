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

struct VkDispatchIndirectCommand {
    uint x;
    uint y;
    uint z;
};

struct Tile {
    uint upperLeftX16Y16;
    uint sizeX16;// last 16 bits not used
    uint depth24ID8;
};

layout(set = 1, binding = 8) buffer TileBuffer {
    VkDispatchIndirectCommand command;
    Tile tiles[];
} tileBuffer;

#define LOCAL_SIZE 32

float SampleNodeDepth(vec2 uv){
//    return 0;
    return texture(nodeDepthTexture, uv).r;
}

bool intersectRayPlane(const vec3 rayOrigin, const vec3 rayDir, const vec3 planePoint, const vec3 planeNormal, out vec3 intersectWorldPos) {
    const float facingRatio = dot(planeNormal, rayDir);
    const float t = dot(planePoint - rayOrigin, planeNormal) / facingRatio;
    intersectWorldPos = rayOrigin + t * rayDir;
    return facingRatio < 0 && t < 0;
}

vec3 WorldPosFromGlobalClipPos(vec4 clipPos)
{
    const vec4 worldPos = globalUBO.invViewProj * clipPos;
    return worldPos.xyz / worldPos.w;
}

vec3 NDCRay(vec2 ndc, mat4 invProj, mat4 invView)
{
    const vec4 clipRayDir = vec4(ndc, 0, 1);
    const vec4 viewSpace = invProj * clipRayDir;
    const vec4 viewDir = vec4(viewSpace.xy, 1, 0);
    const vec3 globalWorldRayDir = normalize((invView * viewDir).xyz);
    return globalWorldRayDir;
}

vec3 WorldRayFroNDC(vec2 ndc)
{
    return NDCRay(ndc, globalUBO.invProj, globalUBO.invView);
}

vec3 NodeNDCRay(vec2 ndc)
{
    return NDCRay(ndc, nodeUBO.invProj, nodeUBO.invView);
}

vec4 GlobalClipPosFromWorldPos(vec3 worldPos)
{
    return globalUBO.viewProj * vec4(worldPos, 1);
}

vec3 WorldPosFromNodeClipPos(vec4 clipPos)
{
    const vec4 worldPos = nodeUBO.invViewProj * clipPos;
    return worldPos.xyz / worldPos.w;
}

vec4 NodeClipPosFromWorldPos(vec3 worldPos)
{
    return nodeUBO.viewProj * vec4(worldPos, 1);
}

vec4 ClipPosFromNDC(vec2 ndc)
{
    return vec4(ndc, 0, 1);
}

vec4 ClipPosFromNDC(vec3 ndc)
{
    return vec4(ndc, 1);
}

vec4 ClipPosFromNDC(vec2 ndc, float depth)
{
    return vec4(ndc, depth, 1);
}

vec3 NDCFromClipPos(vec4 clipPos)
{
    return clipPos.xyz / clipPos.w;
}

vec2 NDCFromUV(vec2 uv)
{
    return uv * 2.0 - 1.0;
}

vec2 UVFromNDC(vec3 ndc)
{
    return (ndc.xy + 1.0) * 0.5;
}

ivec2 iRound(vec2 coord)
{
    // Round/RoundEven returns noisy results
    // but is round even needed!?
    //    ivec2 iCoord = ivec2(coord);
    //    vec2 coordDecimal = coord - vec2(coord);
    //    iCoord.x += coordDecimal.x > 0.5 ? 1 : 0;
    //    iCoord.y += coordDecimal.y > 0.5 ? 1 : 0;
    //    return iCoord;

    //        return ivec2(roundEven(coord));

    return ivec2(coord);
}

ivec2 CoordFromUV(vec2 uv, vec2 screenSize)
{
    return ivec2(uv * screenSize);
}

ivec2 CoordFromUVRound(vec2 uv, vec2 screenSize)
{
    return ivec2(round(uv * screenSize));
}

vec2 UVFromCoord(ivec2 coord, vec2 screenSize)
{
    return vec2(coord) / screenSize;
}

vec3 GlobalNDCFromNodeNDC(vec3 nodeNDC){
    const vec4 nodeClipPos = ClipPosFromNDC(nodeNDC);
    const vec3 worldPos = WorldPosFromNodeClipPos(nodeClipPos);
    const vec4 globalClipPos = GlobalClipPosFromWorldPos(worldPos);
    const vec3 globalNDC = NDCFromClipPos(globalClipPos);
    return globalNDC;
}

float SqrMag(vec2 values[2]) {
    const  vec2 diff = values[0] - values[1];
    return dot(diff, diff);
}

uint PackUint32FromFloat12Float12Float8(float f12a, float f12b, float f8c) {
    uint aInt = uint(f12a * 4095.0);
    uint bInt = uint(f12b * 4095.0);
    uint cInt = uint(f8c * 255.0);
    uint result = (aInt << 20) | (bInt << 8) | cInt;
    return result;
}

void UnpackFloat12Float12Float8FromUint32(uint packed, out float f12a, out float f12b, out float f8c) {
    uint maskA = 0xFFF00000;
    uint maskB = 0x000FFF00;
    uint maskC = 0x000000FF;
    uint aInt = (packed & maskA) >> 20;
    uint bInt = (packed & maskB) >> 8;
    uint cInt = (packed & maskC);
    f12a = float(aInt) / 4095.0;
    f12b = float(bInt) / 4095.0;
    f8c = float(cInt) / 255.0;
}

uint PackFloat32ToUint32(float f32) {
    f32 = clamp(f32, 0.0, 1.0);
    float scaled = f32 * 4294967295.0;
    uint packed = uint(scaled);
    return packed;
}

float UnpackFloat32FromUint32(uint packed) {
    float value = float(packed);
    return value / 4294967295.0;
}

uint PackUint32FromFloat16Float16(float f16a, float f16b) {
    const uint i1 = uint(f16a * 65535.0);
    const uint i2 = uint(f16b * 65535.0);
    return (i1 << 16) | i2;
}

void UnpackFloat16Float16FromUint32(uint packed, out float f16a, out float f16b) {
    const uint i1 = (packed >> 16) & 0xFFFFu;
    const uint i2 = packed & 0xFFFFu;
    f16a = float(i1) / 65535.0;
    f16b = float(i2) / 65535.0;
}

uint PackUint32FromFloat24Uint8(float f24, uint u8) {
    const uint uf = uint(f24 * 16777215.0);
    return (uf << 8) | (u8 & 0xFFu);
}

void UnpackFloat24Uint8FromUint32(uint packed, out float f24, out uint u8) {
    u8 = packed & 0xFFu;
    const uint uf = packed >> 8;
    f24 = float(uf) / 16777215.0;
}