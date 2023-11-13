#pragma once

#include <cstdint>

namespace Moxaic
{
    enum Role : char {
        Compositor,
        Node
    };

    constexpr uint32_t k_DefaultWidth = 1280;
    constexpr uint32_t k_DefaultHeight = 1024;
    constexpr char k_ApplicationName[] = "moxaic";
    constexpr uint8_t k_FramebufferCount = 2;

    extern bool g_ApplicationRunning;

    extern Role g_Role;

    inline bool IsCompositor() {
        return g_Role == Compositor;
    }
}