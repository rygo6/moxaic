#include "moxaic_vulkan_mesh.hpp"
#include "moxaic_vulkan_device.hpp"

#include <glm/glm.hpp>

#define PI 3.14159265358979323846f

static int GenerateSphereVertexCount(int nslices, int nstacks)
{
    return (nslices + 1) * (nstacks + 1);
}

static int GenerateSphereIndexCount(int nslices, int nstacks)
{
    return nslices * nstacks * 2 * 3;
}

static void GenerateSphere(int nslices, int nstacks, float radius, Moxaic::Vertex *pVertex)
{
    float fnslices = (float) nslices;
    float fnstacks = (float) nstacks;

    float dtheta = 2.0f * PI / fnslices;
    float dphi = PI / fnstacks;

    int idx = 0;
    for (int i = 0; +i <= nstacks; i++) {
        float fi = (float) i;
        float phi = fi * dphi;
        for (int j = 0; j <= nslices; j++) {
            float ji = (float) j;
            float theta = ji * dtheta;

            float x = radius * sinf(phi) * cosf(theta);
            float y = radius * sinf(phi) * sinf(theta);
            float z = radius * cosf(phi);

            glm::vec3 pos = {x, y, z};
            glm::vec3 normal = {x, y, z};
            glm::vec2 uv = {ji / fnslices, fi / fnstacks};

            Moxaic::Vertex vertexData = {};
            vertexData.pos = pos;
            vertexData.normal = normal;
            vertexData.uv = uv;

            pVertex[idx++] = vertexData;
        }
    }
}

static void GenerateSphereIndices(int nslices, int nstacks, uint16_t *pIndices)
{
    int idx = 0;
    for (int i = 0; i < nstacks; i++) {
        for (int j = 0; j < nslices; j++) {
            uint16_t v1 = i * (nslices + 1) + j;
            uint16_t v2 = i * (nslices + 1) + j + 1;
            uint16_t v3 = (i + 1) * (nslices + 1) + j;
            uint16_t v4 = (i + 1) * (nslices + 1) + j + 1;

            pIndices[idx++] = v1;
            pIndices[idx++] = v2;
            pIndices[idx++] = v3;

            pIndices[idx++] = v2;
            pIndices[idx++] = v4;
            pIndices[idx++] = v3;
        }
    }
}

Moxaic::VulkanMesh::VulkanMesh(const VulkanDevice &device)
        : k_Device(device) {}

Moxaic::VulkanMesh::~VulkanMesh()
{
    vkDestroyBuffer(k_Device.vkDevice(), m_VkIndexBuffer, VK_ALLOC);
    vkFreeMemory(k_Device.vkDevice(), m_VkIndexBufferMemory, VK_ALLOC);
    vkDestroyBuffer(k_Device.vkDevice(), m_VkVertexBuffer, VK_ALLOC);
    vkFreeMemory(k_Device.vkDevice(), m_VkVertexBufferMemory, VK_ALLOC);
}

MXC_RESULT Moxaic::VulkanMesh::Init()
{
    const int nSlices = 32;
    const int nStack = 32;
    int vertexCount = GenerateSphereVertexCount(nSlices, nStack);
    Vertex pVertices[vertexCount];
    GenerateSphere(nSlices, nStack, 0.5f, pVertices);
    int indexCount = GenerateSphereIndexCount(nSlices, nStack);
    uint16_t pIndices[indexCount];
    GenerateSphereIndices(nSlices, nStack, pIndices);
    MXC_CHK(CreateVertexBuffer(pVertices, vertexCount));
    MXC_CHK(CreateIndexBuffer(pIndices, indexCount));
    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanMesh::CreateVertexBuffer(const Vertex *pVertices,
                                                  const int vertexCount)
{
    m_VertexCount = vertexCount;
    const VkDeviceSize bufferSize = (sizeof(Vertex) * vertexCount);
    MXC_CHK(k_Device.CreateAllocateBindPopulateBufferViaStaging(pVertices,
                                                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                                bufferSize,
                                                                m_VkVertexBuffer,
                                                                m_VkVertexBufferMemory));
    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanMesh::CreateIndexBuffer(const uint16_t *pIndices,
                                                 const int indexCount)
{
    m_IndexCount = indexCount;
    const VkDeviceSize bufferSize = (sizeof(uint16_t) * indexCount);
    MXC_CHK(k_Device.CreateAllocateBindPopulateBufferViaStaging(pIndices,
                                                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                                bufferSize,
                                                                m_VkIndexBuffer,
                                                                m_VkIndexBufferMemory));
    return MXC_SUCCESS;
}
