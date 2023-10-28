#pragma once

#include <cstdint>

namespace Moxaic
{
    enum Role : char {
        Compositor,
        Node
    };

    inline constinit uint32_t k_DefaultWidth = 800;
    inline constinit uint32_t k_DefaultHeight = 600;
    inline constinit char k_ApplicationName[] = "moxaic";

    extern bool g_ApplicationRunning;

    extern Role g_Role;

    inline bool IsCompositor() {
        return g_Role == Compositor;
    }
}