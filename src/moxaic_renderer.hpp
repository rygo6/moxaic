#pragma once

#include "mid_vulkan.hpp"

namespace Moxaic
{
    struct Renderer
    {
        void Init();

        Mid::Vk::Instance instance;
        Mid::Vk::PhysicalDevice physicalDevice;
        Mid::Vk::Surface surface;
        Mid::Vk::LogicalDevice logicalDevice;

    };
}// namespace Moxaic
