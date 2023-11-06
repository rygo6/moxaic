#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace Moxaic
{
    class VulkanDevice;

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
    };

    class VulkanMesh
    {
    public:
        /// Really just a test sphere mesh right now.
        VulkanMesh(const VulkanDevice &device);
        virtual ~VulkanMesh();

        MXC_RESULT Init();

        void RecordRender();

    private:
        const VulkanDevice &k_Device;

        uint32_t m_IndexCount;
        uint32_t m_VertexCount;

        VkBuffer m_VkIndexBuffer;
        VkDeviceMemory m_VkIndexBufferMemory;

        VkBuffer m_VkVertexBuffer;
        VkDeviceMemory m_VkVertexBufferMemory;

        MXC_RESULT CreateVertexBuffer(const Vertex *pVertices,
                                      const int vertexCount);

        MXC_RESULT CreateIndexBuffer(const uint16_t *pIndices,
                                     const int indexCount);
    };
}
