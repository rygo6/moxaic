#include "moxaic_vulkan_mesh.hpp"
#include "moxaic_vulkan_device.hpp"

#include <glm/glm.hpp>

using namespace Moxaic;
using namespace Moxaic::Vulkan;

#define PI 3.14159265358979323846f

static int GenerateSphereVertexCount(const int nslices, const int nstacks)
{
    return (nslices + 1) * (nstacks + 1);
}

static int GenerateSphereIndexCount(const int nslices, const int nstacks)
{
    return nslices * nstacks * 2 * 3;
}

static void GenerateSphere(const int nslices, const int nstacks, const float radius, Vertex* pVertex)
{
    const float fnslices = (float) nslices;
    const float fnstacks = (float) nstacks;

    const float dtheta = 2.0f * PI / fnslices;
    const float dphi = PI / fnstacks;

    int idx = 0;
    for (int i = 0; +i <= nstacks; i++) {
        const float fi = (float) i;
        const float phi = fi * dphi;
        for (int j = 0; j <= nslices; j++) {
            const float ji = (float) j;
            const float theta = ji * dtheta;

            const float x = radius * sinf(phi) * cosf(theta);
            const float y = radius * sinf(phi) * sinf(theta);
            const float z = radius * cosf(phi);

            const glm::vec3 pos = {x, y, z};
            const glm::vec3 normal = {x, y, z};
            const glm::vec2 uv = {ji / fnslices, fi / fnstacks};

            Vertex vertexData = {};
            vertexData.pos = pos;
            vertexData.normal = normal;
            vertexData.uv = uv;

            pVertex[idx++] = vertexData;
        }
    }
}

static void GenerateSphereIndices(const int nslices, const int nstacks, uint16_t* pIndices)
{
    int idx = 0;
    for (int i = 0; i < nstacks; i++) {
        for (int j = 0; j < nslices; j++) {
            const uint16_t v1 = i * (nslices + 1) + j;
            const uint16_t v2 = i * (nslices + 1) + j + 1;
            const uint16_t v3 = (i + 1) * (nslices + 1) + j;
            const uint16_t v4 = (i + 1) * (nslices + 1) + j + 1;

            pIndices[idx++] = v1;
            pIndices[idx++] = v2;
            pIndices[idx++] = v3;

            pIndices[idx++] = v2;
            pIndices[idx++] = v4;
            pIndices[idx++] = v3;
        }
    }
}

Mesh::Mesh(const Device& device)
    : k_Device(device) {}

Mesh::~Mesh()
{
    vkDestroyBuffer(k_Device.GetVkDevice(), m_VkIndexBuffer, VK_ALLOC);
    vkFreeMemory(k_Device.GetVkDevice(), m_VkIndexBufferMemory, VK_ALLOC);
    vkDestroyBuffer(k_Device.GetVkDevice(), m_VkVertexBuffer, VK_ALLOC);
    vkFreeMemory(k_Device.GetVkDevice(), m_VkVertexBufferMemory, VK_ALLOC);
}

MXC_RESULT Mesh::InitSphere()
{
    const int nSlices = 32;
    const int nStack = 32;
    const int vertexCount = GenerateSphereVertexCount(nSlices, nStack);
    Vertex pVertices[vertexCount];
    GenerateSphere(nSlices, nStack, 0.5f, pVertices);
    const int indexCount = GenerateSphereIndexCount(nSlices, nStack);
    uint16_t pIndices[indexCount];
    GenerateSphereIndices(nSlices, nStack, pIndices);
    MXC_CHK(CreateVertexBuffer(pVertices, vertexCount));
    MXC_CHK(CreateIndexBuffer(pIndices, indexCount));
    return MXC_SUCCESS;
}

MXC_RESULT Mesh::CreateVertexBuffer(const Vertex* pVertices,
                                    const int vertexCount)
{
    m_VertexCount = vertexCount;
    const VkDeviceSize bufferSize = (sizeof(Vertex) * vertexCount);
    MXC_CHK(k_Device.CreateAllocateBindPopulateBufferViaStaging(pVertices,
                                                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                                bufferSize,
                                                                &m_VkVertexBuffer,
                                                                &m_VkVertexBufferMemory));
    return MXC_SUCCESS;
}

MXC_RESULT Mesh::CreateIndexBuffer(const uint16_t* pIndices,
                                   const int indexCount)
{
    m_IndexCount = indexCount;
    const VkDeviceSize bufferSize = (sizeof(uint16_t) * indexCount);
    MXC_CHK(k_Device.CreateAllocateBindPopulateBufferViaStaging(pIndices,
                                                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                                bufferSize,
                                                                &m_VkIndexBuffer,
                                                                &m_VkIndexBufferMemory));
    return MXC_SUCCESS;
}
void Mesh::RecordRender() const
{
    const VkBuffer vertexBuffers[] = {m_VkVertexBuffer};
    constexpr VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(k_Device.GetVkGraphicsCommandBuffer(),
                           0,
                           1,
                           vertexBuffers,
                           offsets);
    vkCmdBindIndexBuffer(k_Device.GetVkGraphicsCommandBuffer(),
                         m_VkIndexBuffer,
                         0,
                         VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(k_Device.GetVkGraphicsCommandBuffer(),
                     m_IndexCount,
                     1,
                     0,
                     0,
                     0);
}
