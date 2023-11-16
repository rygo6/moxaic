#pragma once

#include "moxaic_logging.hpp"

#include <cstdint>

namespace Moxaic
{
    enum class Role {
        Compositor,
        Node
    };

    inline constexpr uint32_t k_DefaultWidth = 1280;
    inline constexpr uint32_t k_DefaultHeight = 1024;
    inline constexpr char k_ApplicationName[] = "moxaic";
    inline constexpr uint8_t k_FramebufferCount = 2;

    inline MXC_RESULT g_ApplicationRunning = MXC_SUCCESS;

    inline Role g_Role;

    inline bool IsCompositor() {
        return g_Role == Role::Compositor;
    }
}