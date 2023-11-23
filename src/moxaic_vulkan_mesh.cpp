#include "moxaic_vulkan_mesh.hpp"
#include "moxaic_vulkan_device.hpp"

#include <glm/glm.hpp>

using namespace Moxaic;
using namespace Moxaic::Vulkan;

#define PI 3.14159265358979323846f

static int GenerateSphereVertexCount(int const nslices, int const nstacks)
{
    return (nslices + 1) * (nstacks + 1);
}

static int GenerateSphereIndexCount(int const nslices, int const nstacks)
{
    return nslices * nstacks * 2 * 3;
}

static void GenerateSphere(int const nslices, int const nstacks, float const radius, Vertex* pVertex)
{
    float const fnslices = (float) nslices;
    float const fnstacks = (float) nstacks;

    float const dtheta = 2.0f * PI / fnslices;
    float const dphi = PI / fnstacks;

    int idx = 0;
    for (int i = 0; +i <= nstacks; i++) {
        float const fi = (float) i;
        float const phi = fi * dphi;
        for (int j = 0; j <= nslices; j++) {
            float const ji = (float) j;
            float const theta = ji * dtheta;

            float const x = radius * sinf(phi) * cosf(theta);
            float const y = radius * sinf(phi) * sinf(theta);
            float const z = radius * cosf(phi);

            glm::vec3 const pos = {x, y, z};
            glm::vec3 const normal = {x, y, z};
            glm::vec2 const uv = {ji / fnslices, fi / fnstacks};

            Vertex vertexData = {};
            vertexData.pos = pos;
            vertexData.normal = normal;
            vertexData.uv = uv;

            pVertex[idx++] = vertexData;
        }
    }
}

static void GenerateSphereIndices(int const nslices, int const nstacks, uint16_t* pIndices)
{
    int idx = 0;
    for (int i = 0; i < nstacks; i++) {
        for (int j = 0; j < nslices; j++) {
            uint16_t const v1 = i * (nslices + 1) + j;
            uint16_t const v2 = i * (nslices + 1) + j + 1;
            uint16_t const v3 = (i + 1) * (nslices + 1) + j;
            uint16_t const v4 = (i + 1) * (nslices + 1) + j + 1;

            pIndices[idx++] = v1;
            pIndices[idx++] = v2;
            pIndices[idx++] = v3;

            pIndices[idx++] = v2;
            pIndices[idx++] = v4;
            pIndices[idx++] = v3;
        }
    }
}

Mesh::Mesh(Device const& device)
    : k_pDevice(&device) {}

Mesh::~Mesh()
{
    vkDestroyBuffer(k_pDevice->GetVkDevice(), m_VkIndexBuffer, VK_ALLOC);
    vkFreeMemory(k_pDevice->GetVkDevice(), m_VkIndexBufferMemory, VK_ALLOC);
    vkDestroyBuffer(k_pDevice->GetVkDevice(), m_VkVertexBuffer, VK_ALLOC);
    vkFreeMemory(k_pDevice->GetVkDevice(), m_VkVertexBufferMemory, VK_ALLOC);
}

MXC_RESULT Mesh::InitSphere()
{
    int const nSlices = 32;
    int const nStack = 32;
    int const vertexCount = GenerateSphereVertexCount(nSlices, nStack);
    Vertex pVertices[vertexCount];
    GenerateSphere(nSlices, nStack, 0.5f, pVertices);
    int const indexCount = GenerateSphereIndexCount(nSlices, nStack);
    uint16_t pIndices[indexCount];
    GenerateSphereIndices(nSlices, nStack, pIndices);
    MXC_CHK(CreateVertexBuffer(pVertices, vertexCount));
    MXC_CHK(CreateIndexBuffer(pIndices, indexCount));
    return MXC_SUCCESS;
}

MXC_RESULT Mesh::CreateVertexBuffer(Vertex const* pVertices,
                                    int const vertexCount)
{
    m_VertexCount = vertexCount;
    VkDeviceSize const bufferSize = (sizeof(Vertex) * vertexCount);
    MXC_CHK(k_pDevice->CreateAllocateBindPopulateBufferViaStaging(pVertices,
                                                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                                bufferSize,
                                                                &m_VkVertexBuffer,
                                                                &m_VkVertexBufferMemory));
    return MXC_SUCCESS;
}

MXC_RESULT Mesh::CreateIndexBuffer(uint16_t const* pIndices,
                                   int const indexCount)
{
    m_IndexCount = indexCount;
    VkDeviceSize const bufferSize = (sizeof(uint16_t) * indexCount);
    MXC_CHK(k_pDevice->CreateAllocateBindPopulateBufferViaStaging(pIndices,
                                                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                                bufferSize,
                                                                &m_VkIndexBuffer,
                                                                &m_VkIndexBufferMemory));
    return MXC_SUCCESS;
}
void Mesh::RecordRender() const
{
    VkBuffer const vertexBuffers[] = {m_VkVertexBuffer};
    constexpr VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(k_pDevice->GetVkGraphicsCommandBuffer(),
                           0,
                           1,
                           vertexBuffers,
                           offsets);
    vkCmdBindIndexBuffer(k_pDevice->GetVkGraphicsCommandBuffer(),
                         m_VkIndexBuffer,
                         0,
                         VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(k_pDevice->GetVkGraphicsCommandBuffer(),
                     m_IndexCount,
                     1,
                     0,
                     0,
                     0);
}
