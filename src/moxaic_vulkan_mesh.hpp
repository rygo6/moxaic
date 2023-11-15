#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace Moxaic::Vulkan
{
    class Device;

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
    };

    class Mesh
    {
    public:
        /// Really just a test sphere mesh right now.
        Mesh(const Device &device);
        virtual ~Mesh();

        MXC_RESULT Init();

        void RecordRender();

    private:
        const Device &k_Device;

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
