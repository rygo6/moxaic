#pragma once

#include "moxaic_logging.hpp"

#include <cstdint>

namespace Moxaic
{
    enum class Role {
        Compositor,
        Node
    };

    inline constexpr uint32_t DefaultWidth = 1280;
    inline constexpr uint32_t DefaultHeight = 1024;
    inline constexpr char ApplicationName[] = "moxaic";
    inline constexpr uint8_t FramebufferCount = 2;

    inline MXC_RESULT Running = MXC_SUCCESS;

    inline Role Role = Role::Compositor;

    inline bool IsCompositor() {
        return Role == Role::Compositor;
    }
}