#pragma once

#include <cstdint>

namespace Moxaic
{
    enum Role : char {
        Compositor,
        Node
    };

    inline constexpr uint32_t k_DefaultWidth = 1280;
    inline constexpr uint32_t k_DefaultHeight = 1024;
    inline constexpr char k_ApplicationName[] = "moxaic";
    inline constexpr uint8_t k_FramebufferCount = 2;

    inline bool g_ApplicationRunning = true;

    inline Role g_Role;

    inline bool IsCompositor() {
        return g_Role == Compositor;
    }
}