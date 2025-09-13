#version 450

#include "global_binding.glsl"
#include "binding_node.glsl"

layout (vertices = 4) out;

layout (location = 0) in vec3 inNormals[];
layout (location = 1) in vec2 inUVs[];

layout (location = 0) out vec3 outNormals[4];
layout (location = 1) out vec2 outUVs[4];

void main()
{
    vec2 nodeULUV = nodeState[push.nodeHandle].ulUV;
    vec2 nodeLRUV = nodeState[push.nodeHandle].lrUV;

    if (gl_InvocationID == 0)
    {
        vec2 uvDiff =  abs(nodeLRUV - nodeULUV);

        vec2 tessellationFactor = uvDiff * 64;
        gl_TessLevelOuter[0] = tessellationFactor.y;
        gl_TessLevelOuter[1] = tessellationFactor.x;
        gl_TessLevelOuter[2] = tessellationFactor.y;
        gl_TessLevelOuter[3] = tessellationFactor.x;

        gl_TessLevelInner[0] = tessellationFactor.x;
        gl_TessLevelInner[1] = tessellationFactor.y;
    }

    gl_out[gl_InvocationID].gl_Position =  gl_in[gl_InvocationID].gl_Position;
    outNormals[gl_InvocationID] = inNormals[gl_InvocationID];
    outUVs[gl_InvocationID] = inUVs[gl_InvocationID];
}