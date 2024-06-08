#pragma once

#include "mid_math.h"
#include "renderer.h"

void CreateSphereMesh(const float radius, const int slicesCount, const int stackCount, VkmMesh* pMesh);
void CreateQuadMesh(const float size, VkmMesh* pMesh);
void CreateQuadPatchMesh(const float size, VkmMesh* pMesh);

#ifdef MID_SHAPE_IMPLEMENTATION
void CreateSphereMesh(const float radius, const int slicesCount, const int stackCount, VkmMesh* pMesh) {
  VkmMeshCreateInfo info = {
      .indexCount = slicesCount * stackCount * 2 * 3,
      .vertexCount = (slicesCount + 1) * (stackCount + 1),
  };
  uint16_t indices[info.indexCount];
  int      index = 0;
  for (int i = 0; i < stackCount; i++) {
    for (int j = 0; j < slicesCount; j++) {
      const uint16_t v1 = i * (slicesCount + 1) + j;
      const uint16_t v2 = i * (slicesCount + 1) + j + 1;
      const uint16_t v3 = (i + 1) * (slicesCount + 1) + j;
      const uint16_t v4 = (i + 1) * (slicesCount + 1) + j + 1;
      indices[index++] = v1;
      indices[index++] = v2;
      indices[index++] = v3;
      indices[index++] = v2;
      indices[index++] = v4;
      indices[index++] = v3;
    }
  }
  VkmVertex   vertices[info.vertexCount];
  const float slices = (float)slicesCount;
  const float stacks = (float)stackCount;
  const float dtheta = 2.0f * MID_PI / slices;
  const float dphi = MID_PI / stacks;
  int         vertex = 0;
  for (int i = 0; +i <= stackCount; i++) {
    const float fi = (float)i;
    const float phi = fi * dphi;
    for (int j = 0; j <= slicesCount; j++) {
      const float ji = (float)j;
      const float theta = ji * dtheta;
      const float x = radius * sinf(phi) * cosf(theta);
      const float y = radius * sinf(phi) * sinf(theta);
      const float z = radius * cosf(phi);
      vertices[vertex++] = (VkmVertex){
          .position = {x, y, z},
          .normal = {x, y, z},
          .uv = {ji / slices, fi / stacks},
      };
    }
    info.pIndices = indices;
    info.pVertices = vertices;
  }
  VkmCreateMesh(&info, pMesh);
}
void CreateQuadMesh(const float size, VkmMesh* pMesh) {
  const uint16_t  indices[] = {0, 1, 2, 1, 3, 2};
  const VkmVertex vertices[] = {
      {.position = {-size, -size, 0}, .uv = {0, 0}},
      {.position = {size, -size, 0}, .uv = {1, 0}},
      {.position = {-size, size, 0}, .uv = {0, 1}},
      {.position = {size, size, 0}, .uv = {1, 1}},
  };
  const VkmMeshCreateInfo info = {
      .indexCount = 6,
      .vertexCount = 4,
      .pIndices = indices,
      .pVertices = vertices,
  };
  VkmCreateMesh(&info, pMesh);
}
void CreateQuadPatchMesh(const float size, VkmMesh* pMesh) {
  const uint16_t  indices[] = {0, 1, 3, 2};
  const VkmVertex vertices[] = {
      {.position = {-size, -size, 0}, .uv = {0, 0}},
      {.position = {size, -size, 0}, .uv = {1, 0}},
      {.position = {-size, size, 0}, .uv = {0, 1}},
      {.position = {size, size, 0}, .uv = {1, 1}},
  };
  const VkmMeshCreateInfo info = {
      .indexCount = 6,
      .vertexCount = 4,
      .pIndices = indices,
      .pVertices = vertices,
  };
  VkmCreateMesh(&info, pMesh);
}
#endif