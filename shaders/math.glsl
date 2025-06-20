#define HALF_EPSILON 0.0009765625

float Lerp(float a, float b, float t) {
    return a + ((b - a) * t);
}

float linearStep(float edge0, float edge1, float x) {
    return clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
}

vec4 LinearizeDepth(vec4 zNear, vec4 zFar, vec4 projectionDepth) {
    return zNear * zFar / (zFar - projectionDepth * (zFar - zNear));
}
vec4 ProjectDepth(vec4 newNear, vec4 newFar, vec4 linearDepth) {
    return (newFar * (linearDepth - newNear)) / (linearDepth * (newFar - newNear));
}

float LinearizeDepth(float zNear, float zFar, float projectionDepth) {
    return zNear * zFar / (zFar - projectionDepth * (zFar - zNear));
}
float ProjectDepth(float newNear, float newFar, float linearDepth) {
    return (newFar * (linearDepth - newNear)) / (linearDepth * (newFar - newNear));
}

uint PackDepth16(float depth) {
    return uint(depth * 65535.0);
}
float UnpackDepth16(uint packedDepth) {
    return float(packedDepth) / 65535.0;
}
uint PackDepth32(float depth) {
    // I have no idea why I need to sutract 128 this should be correct !?
    return uint(depth * (4294967295.0 - 128));
}
float UnpackDepth32(uint packedDepth) {
    return float(packedDepth) / (4294967295.0 - 128);
}