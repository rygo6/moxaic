#pragma once

#include "moxaic_vulkan_uniform.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <windows.h>

namespace Moxaic
{
    class VulkanDevice;

    struct CameraBuffer
    {
        glm::mat4x4 view;
        glm::mat4x4 proj;
        glm::mat4x4 invView;
        glm::mat4x4 invProj;
        uint32_t width;
        uint32_t height;
    };

    class VulkanCamera
    {
    public:
        explicit VulkanCamera(const VulkanDevice &device);
        virtual ~VulkanCamera();

        bool Init(const BufferLocality &external);

    private:
        CameraBuffer m_Buffer;
        VulkanUniform<CameraBuffer> m_Uniform;
    };
}
