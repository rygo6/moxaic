#define DEPTH_FAR 0.0f
#define DEPTH_NEAR 1.0f

const uint clearCurrentDepthFlagMask = 0x7FFFFFFFu;

uint PackDepth16(float depth) {
    return uint(depth * 65535.0);
}
float UnpackDepth16(uint packedDepth) {
    return float(packedDepth) / 65535.0;
}

uint PackDepth(bool current, float normalizedDepth) {
    normalizedDepth = clamp(normalizedDepth, 0.0, 1.0);

    // Scale to use 31 bits (leaving the MSB for our flag)
    // Maximum value will be 2^31 - 1 = 2147483647
    uint packedValue = uint(normalizedDepth * 2147483647.0);

    // Set the MSB based on the flag (bit 31)
    packedValue = current ? packedValue | 0x80000000u : packedValue; // Set MSB to 1

    return packedValue;
}
void UnpackDepth(uint packedDepth, out float normalizedDepth, out bool current) {
    current = (packedDepth & 0x80000000u) != 0u;

    // Extract the value bits (clearing the flag bit)
    uint valueBits = packedDepth & 0x7FFFFFFFu;

    // Convert back to normalized float
    normalizedDepth = float(valueBits) / 2147483647.0;
}
bool UnpackDepthCurrentFlag(uint packedDepth) {
    return (packedDepth & 0x80000000u) != 0u;
}
uint SetDepthFlag(uint value) {
    return value | 0x80000000u;
}
uint ClearDepthFlag(uint value) {
    return value & 0x7FFFFFFFu;
}

bool IntersectPlane(vec3 rayOrigin, vec3 rayDir, vec3 planePoint, vec3 planeNormal, out vec3 intersectWorldPos) {
    float facingRatio = dot(planeNormal, rayDir);
    float t = dot(planePoint - rayOrigin, planeNormal) / facingRatio;
    intersectWorldPos = rayOrigin + t * rayDir;
    return facingRatio < 0;
}

//bool IntersectNodeNDC(vec3 worldPos, vec3 worldRay, vec2 globalUV, out vec3 intersectNodeNDC)
//{
//    vec3 intersectWorldPos;
//    if (IntersectPlane(worldPos, worldRay, nodeOriginWorldPos, nodeOriginWorldDirection, intersectWorldPos))
//        return false;
//
//    vec4 intersectGlobalClipPos = GlobalClipPosFromWorldPos(intersectWorldPos);
//    vec3 intersectGlobalNDC = NDCFromClipPos(intersectGlobalClipPos);
//    vec4 intersectNodeClipPos = NodeClipPosFromWorldPos(intersectWorldPos);
//    intersectNodeNDC = NDCFromClipPos(intersectNodeClipPos);
//
//    return true;
//}
