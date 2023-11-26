#pragma once

#include "moxaic_vulkan.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

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
        MXC_NO_VALUE_PASS(Mesh);

        explicit Mesh(const Device& device);
        virtual ~Mesh();

        /// Really just a test sphere mesh right now.
        MXC_RESULT InitSphere();

        void RecordRender(const VkCommandBuffer commandBuffer) const;

    private:
        const Device* const k_pDevice;

        uint32_t m_IndexCount{};
        uint32_t m_VertexCount{};

        VkBuffer m_VkIndexBuffer{};
        VkDeviceMemory m_VkIndexBufferMemory{};

        VkBuffer m_VkVertexBuffer;
        VkDeviceMemory m_VkVertexBufferMemory{};

        MXC_RESULT CreateVertexBuffer(const Vertex* pVertices,
                                      int vertexCount);

        MXC_RESULT CreateIndexBuffer(const uint16_t* pIndices,
                                     int indexCount);
    };
}// namespace Moxaic::Vulkan
