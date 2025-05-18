#pragma once

#include "mid_math.h"
#include "mid_vulkan.h"

void vkCreateSphereMesh(float radius, int slicesCount, int stackCount, VkMesh* pMesh);
void vkCreateQuadMesh(float size, VkMesh* pMesh);
void vkCreateQuadPatchMeshSharedMemory(VkSharedMesh* pMesh);
void vkBindUpdateQuadPatchMesh(float size, VkSharedMesh* pMesh);
void vkCreateQuadPatchMesh(float size, VkMesh* pMesh);

#if defined(MID_SHAPE_IMPLEMENTATION) || defined(MID_IDE_ANALYSIS)

void vkCreateSphereMesh(float radius, int slicesCount, int stackCount, VkMesh* pMesh) {
	VkMeshCreateInfo info = {
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
  float dtheta = 2.0f * PI / slices;
  float dphi = PI / stacks;
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
          .position = VEC3(x, y, z),
          .normal = VEC3(x, y, z),
          .uv = VEC2(ji / slices, fi / stacks),
      };
    }
    info.pIndices = indices;
    info.pVertices = vertices;
  }
  vkCreateMesh(&info, pMesh);
}

void vkCreateQuadMesh(float size, VkMesh* pMesh) {
  uint16_t  indices[] = {0, 1, 2, 1, 3, 2};
  MidVertex vertices[] = {
      {.position = VEC3(-size, -size, 0), .uv = VEC2(0, 0)},
      {.position = VEC3(size, -size, 0),  .uv = VEC2(1, 0)},
      {.position = VEC3(-size, size, 0),  .uv = VEC2(0, 1)},
      {.position = VEC3(size, size, 0),   .uv = VEC2(1, 1)},
  };
  VkMeshCreateInfo info = {
      .indexCount = 6,
      .vertexCount = 4,
      .pIndices = indices,
      .pVertices = vertices,
  };
  vkCreateMesh(&info, pMesh);
}

#define QUAD_PATCH_MESH_INFO  \
	VkMeshCreateInfo info = { \
		.indexCount = 6,      \
		.vertexCount = 4,     \
	};
#define QUAD_PATCH_MESH_VERTICES_INDICES                       \
	info.pIndices = (uint16_t[]){0, 1, 3, 2};                  \
	info.pVertices = (MidVertex[]){                            \
		{.position = VEC3(-size, -size, 0), .uv = VEC2(0, 0)}, \
		{.position = VEC3(size, -size, 0),  .uv = VEC2(1, 0)},  \
		{.position = VEC3(-size, size, 0),  .uv = VEC2(0, 1)},  \
		{.position = VEC3(size, size, 0),   .uv = VEC2(1, 1)},   \
	};

void vkCreateQuadPatchMeshSharedMemory(VkSharedMesh* pMesh) {
  QUAD_PATCH_MESH_INFO
  vkCreateSharedMesh(&info, pMesh);
}

void vkBindUpdateQuadPatchMesh(float size, VkSharedMesh* pMesh) {
  QUAD_PATCH_MESH_INFO
  QUAD_PATCH_MESH_VERTICES_INDICES
  vkBindUpdateSharedMesh(&info, pMesh);
}

void vkCreateQuadPatchMesh(float size, VkMesh* pMesh) {
  QUAD_PATCH_MESH_INFO
  QUAD_PATCH_MESH_VERTICES_INDICES
  vkCreateMesh(&info, pMesh);
}

#endif