#pragma once

#include "moxaic_logging.hpp"

namespace Moxaic
{
    class VulkanDevice;
    class VulkanFramebuffer;
    class Camera;

    MXC_RESULT CoreInit();
    MXC_RESULT CoreLoop();
}