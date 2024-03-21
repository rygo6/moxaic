#pragma once

#include "mid_vulkan.hpp"

namespace Moxaic
{
    struct Renderer
    {
        static constexpr uint32_t DefaultWidth = 1024;
        static constexpr uint32_t DefaultHeight = 1024;

        void Init();

        Mid::Vk::Instance instance;
        Mid::Vk::PhysicalDevice physicalDevice;
        Mid::Vk::Surface surface;
        Mid::Vk::LogicalDevice logicalDevice;

    };
}// namespace Moxaic
