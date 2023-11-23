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

        explicit Mesh(Device const& device);
        virtual ~Mesh();

        /// Really just a test sphere mesh right now.
        MXC_RESULT InitSphere();

        void RecordRender() const;

    private:
        Device const& k_Device;

        uint32_t m_IndexCount{};
        uint32_t m_VertexCount{};

        VkBuffer m_VkIndexBuffer{};
        VkDeviceMemory m_VkIndexBufferMemory{};

        VkBuffer m_VkVertexBuffer;
        VkDeviceMemory m_VkVertexBufferMemory{};

        MXC_RESULT CreateVertexBuffer(Vertex const* pVertices,
                                      int vertexCount);

        MXC_RESULT CreateIndexBuffer(uint16_t const* pIndices,
                                     int indexCount);
    };
}// namespace Moxaic::Vulkan
