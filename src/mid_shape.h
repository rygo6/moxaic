#pragma once

#include "mid_math.h"
#include "mid_vulkan.h"

void RequestSphereMeshAllocation(int slicesCount, int stackCount, VkmMesh* pMesh);
void CreateSphereMesh(float radius, int slicesCount, int stackCount, VkmMesh* pMesh);
void CreateQuadMesh(float size, VkmMesh* pMesh);
void CreateQuadPatchMeshSharedMemory(VkmMesh* pMesh);
void BindUpdateQuadPatchMesh(float size, VkmMesh* pMesh);
void midCreateQuadPatchMesh(float size, VkmMesh* pMesh);

#ifdef MID_SHAPE_IMPLEMENTATION
void RequestSphereMeshAllocation(int slicesCount, int stackCount, VkmMesh* pMesh) {
  VkmMeshCreateInfo info = {
      .indexCount = slicesCount * stackCount * 2 * 3,
      .vertexCount = (slicesCount + 1) * (stackCount + 1),
  };
  midVkCreateMeshSharedMemory(&info, pMesh);
}
void CreateSphereMesh(float radius, int slicesCount, int stackCount, VkmMesh* pMesh) {
  VkmMeshCreateInfo info = {
      .indexCount = slicesCount * stackCount * 2 * 3,
      .vertexCount = (slicesCount + 1) * (stackCount + 1),
  };
  uint16_t indices[info.indexCount];
  int      index = 0;
  for (int i = 0; i < stackCount; i++) {
    for (int j = 0; j < slicesCount; j++) {
      uint16_t v1 = i * (slicesCount + 1) + j;
      uint16_t v2 = i * (slicesCount + 1) + j + 1;
      uint16_t v3 = (i + 1) * (slicesCount + 1) + j;
      uint16_t v4 = (i + 1) * (slicesCount + 1) + j + 1;
      indices[index++] = v1;
      indices[index++] = v2;
      indices[index++] = v3;
      indices[index++] = v2;
      indices[index++] = v4;
      indices[index++] = v3;
    }
  }
  MidVertex   vertices[info.vertexCount];
  float slices = (float)slicesCount;
  float stacks = (float)stackCount;
  float dtheta = 2.0f * MID_PI / slices;
  float dphi = MID_PI / stacks;
  int         vertex = 0;
  for (int i = 0; +i <= stackCount; i++) {
    float fi = (float)i;
    float phi = fi * dphi;
    for (int j = 0; j <= slicesCount; j++) {
      float ji = (float)j;
      float theta = ji * dtheta;
      float x = radius * sinf(phi) * cosf(theta);
      float y = radius * sinf(phi) * sinf(theta);
      float z = radius * cosf(phi);
      vertices[vertex++] = (MidVertex){
          .position = {x, y, z},
          .normal = {x, y, z},
          .uv = {ji / slices, fi / stacks},
      };
    }
    info.pIndices = indices;
    info.pVertices = vertices;
  }
  vkmCreateMesh(&info, pMesh);
}
void CreateQuadMesh(float size, VkmMesh* pMesh) {
  uint16_t  indices[] = {0, 1, 2, 1, 3, 2};
  MidVertex vertices[] = {
      {.position = {-size, -size, 0}, .uv = {0, 0}},
      {.position = {size, -size, 0}, .uv = {1, 0}},
      {.position = {-size, size, 0}, .uv = {0, 1}},
      {.position = {size, size, 0}, .uv = {1, 1}},
  };
  VkmMeshCreateInfo info = {
      .indexCount = 6,
      .vertexCount = 4,
      .pIndices = indices,
      .pVertices = vertices,
  };
  vkmCreateMesh(&info, pMesh);
}

  #define QUAD_PATCH_MESH_INFO \
    VkmMeshCreateInfo info = { \
        .indexCount = 6,       \
        .vertexCount = 4,      \
    };
  #define QUAD_PATCH_MESH_VERTICES_INDICES             \
    info.pIndices = (uint16_t[]){0, 1, 3, 2};    \
    info.pVertices = (MidVertex[]){              \
        {.position = {-size, -size, 0}, .uv = {0, 0}}, \
        {.position = {size, -size, 0}, .uv = {1, 0}},  \
        {.position = {-size, size, 0}, .uv = {0, 1}},  \
        {.position = {size, size, 0}, .uv = {1, 1}},   \
    };
void CreateQuadPatchMeshSharedMemory(VkmMesh* pMesh) {
  QUAD_PATCH_MESH_INFO
  midVkCreateMeshSharedMemory(&info, pMesh);
}
void BindUpdateQuadPatchMesh(float size, VkmMesh* pMesh) {
  QUAD_PATCH_MESH_INFO
  QUAD_PATCH_MESH_VERTICES_INDICES
  midVkBindUpdateMeshSharedMemory(&info, pMesh);
}
void midCreateQuadPatchMesh(float size, VkmMesh* pMesh) {
  QUAD_PATCH_MESH_INFO
  QUAD_PATCH_MESH_VERTICES_INDICES
  vkmCreateMesh(&info, pMesh);
}
#endif