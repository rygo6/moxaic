#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>
#include <array>

#if WIN32
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#include <vector>
#endif

namespace Moxaic
{

    class VulkanDevice
    {
    public:
        VulkanDevice();

        virtual ~VulkanDevice();

        bool Init();

        bool AllocateMemory(const VkMemoryPropertyFlags &properties,
                            VkImage image,
                            VkDeviceMemory &outDeviceMemory) const;

        bool BeginImmediateCommandBuffer(VkCommandBuffer &outCommandBuffer) const;

        bool EndImmediateCommandBuffer(const VkCommandBuffer &commandBuffer) const;

    private:
        uint32_t m_GraphicsQueueFamilyIndex{};
        uint32_t m_ComputeQueueFamilyIndex{};

        VkPhysicalDeviceMeshShaderPropertiesEXT m_PhysicalDeviceMeshShaderProperties{};
        VkPhysicalDeviceProperties2 m_PhysicalDeviceProperties{};
        VkPhysicalDeviceMemoryProperties m_PhysicalDeviceMemoryProperties{};

        bool PickPhysicalDevice();
        bool FindQueues();
        bool CreateDevice();
        bool CreateRenderPass();
        bool CreateCommandBuffers();
        bool CreatePools();
        bool CreateSamplers();


    public:
#define VK_HANDLES \
        VK_HANDLE(,Device) \
        VK_HANDLE(,PhysicalDevice) \
        VK_HANDLE(Graphics,Queue) \
        VK_HANDLE(Compute,Queue) \
        VK_HANDLE(,RenderPass) \
        VK_HANDLE(,DescriptorPool) \
        VK_HANDLE(,QueryPool) \
        VK_HANDLE(Graphics,CommandPool) \
        VK_HANDLE(Compute,CommandPool) \
        VK_HANDLE(Graphics,CommandBuffer) \
        VK_HANDLE(Compute,CommandBuffer) \
        VK_HANDLE(Linear,Sampler) \
        VK_HANDLE(Nearest,Sampler)

#define VK_HANDLE(p, h) VK_GETTERS(p, h)
        VK_HANDLES
#undef VK_HANDLE
    private:
#define VK_HANDLE(p, h) VK_MEMBERS(p, h)
        VK_HANDLES
#undef VK_HANDLE
#undef VK_HANDLES
    };
}
