//struct Tile
//{
//    float16_t  x;
//    float16_t  y;
//    float8_t size;
//    uint8_t id;
//};

layout (set = 1, binding = 0) uniform NodeUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 invView;
    mat4 invProj;
    mat4 invViewProj;
    int width;
    int height;
    float planeZDepth;
} nodeUBO;

layout (set = 1, binding = 1) uniform sampler2D nodeColorTexture;
layout (set = 1, binding = 2) uniform sampler2D nodeNormalTexture;
layout (set = 1, binding = 3) uniform sampler2D nodeGBufferTexture;
layout (set = 1, binding = 4) uniform sampler2D nodeDepthTexture;

layout(binding = 0, buffer) buffer AtomicBuffer {
    atomic_uint myAtomicCounter; // Define atomic_uint
};

#define LOCAL_SIZE 32

vec3 intersectRayPlane(vec3 rayOrigin, vec3 rayDir, vec3 planePoint, vec3 planeNormal) {
    const float facingRatio = dot(planeNormal, rayDir);
    const float t = dot(planePoint - rayOrigin, planeNormal) / facingRatio;
    return (facingRatio > 0) ? (rayOrigin + t * rayDir) : vec3(0, 0, 0);
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

vec3 GlobalNDCRay(vec2 ndc)
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

    //    return ivec2(roundEven(coord));

    return ivec2(coord);
}

ivec2 CoordFromUV(vec2 uv, vec2 screenSize)
{
    return iRound(uv * screenSize);
}

vec2 UVFromCoord(ivec2 coord, vec2 screenSize)
{
    return vec2(coord) / screenSize;
}