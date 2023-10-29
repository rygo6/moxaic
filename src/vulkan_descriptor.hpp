#pragma once

#include "moxaic_vulkan.hpp"

#include "vulkan/vulkan.h"

namespace Moxaic
{


    class VulkanDescriptor
    {
    public:
        VulkanDescriptor();
        virtual ~VulkanDescriptor();

        bool Init();

    private:
        VkDescriptorSetLayout m_VkDescriptorSetLayout;
        VkDescriptorSet m_VkDescriptorSet;
    };

    class GlobalDescriptor : VulkanDescriptor
    {
    public:
//        FBR_RESULT fbrCreateSetGlobal(const FbrVulkan *pVulkan,
//                                      const FbrDescriptors *pDescriptors,
//                                      const FbrCamera *pCamera,
//                                      FbrSetPass *pSet);
    public:
    };

    class MaterialDescriptor : VulkanDescriptor
    {
    public:
//        FBR_RESULT fbrCreateSetMaterial(const FbrVulkan *pVulkan,
//                                        const FbrDescriptors *pDescriptors,
//                                        const FbrTexture *pTexture,
//                                        FbrSetMaterial *pSet);
    public:
    };

    class ObjectDescriptor : VulkanDescriptor
    {
    public:
//        FBR_RESULT fbrCreateSetObject(const FbrVulkan *pVulkan,
//                                      const FbrDescriptors *pDescriptors,
//                                      const FbrTransform *pTransform,
//                                      FbrSetObject *pSet);
    public:
    };
}
