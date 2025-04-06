#define HALF_EPSILON 0.0009765625

float Lerp(float a, float b, float t) {
    return a + ((b - a) * t);
}

vec4 LinearizeDepth(vec4 zNear, vec4 zFar, vec4 projectionDepth) {
    return zNear * zFar / (zFar - projectionDepth * (zFar - zNear));
}

vec4 ProjectDepth(vec4 newNear, vec4 newFar, vec4 linearDepth) {
    return (newFar * (linearDepth - newNear)) / (linearDepth * (newFar - newNear));
}