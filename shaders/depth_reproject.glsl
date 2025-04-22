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

vec3 IntersectNodeNDC(vec3 nodeOriginWorldPos, vec3 nodeOriginWorldDirection, vec2 globalUV)
{
    vec2 globalNDC = NDCFromUV(globalUV);
    vec4 globalClipPos = ClipPosFromNDC(globalNDC);
    vec3 worldPos = WorldPosFromGlobalClipPos(globalClipPos);
    vec3 worldRay = WorldRayDirFromGlobalNDC(globalNDC);
    vec3 intersectWorldPos;
    bool intersected = intersectRayPlane(worldPos, worldRay, nodeOriginWorldPos, nodeOriginWorldDirection, intersectWorldPos);
    vec4 intersectGlobalClipPos = GlobalClipPosFromWorldPos(intersectWorldPos);
    vec3 intersectGlobalNDC = NDCFromClipPos(intersectGlobalClipPos);
    vec4 intersectNodeClipPos = NodeClipPosFromWorldPos(intersectWorldPos);
    vec3 intersectNodeNDC = NDCFromClipPos(intersectNodeClipPos);
    return intersectNodeNDC;
}
