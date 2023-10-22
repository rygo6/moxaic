#pragma once

namespace Moxaic
{
    enum Role : char {
        Compositor,
        Node
    };

    constexpr char k_ApplicationName[] = "moxaic";
    extern bool g_ApplicationRunning;

    extern Role g_Role;

    inline bool IsCompositor() {
        return g_Role == Compositor;
    }
}