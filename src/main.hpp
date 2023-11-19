#pragma once

#include <cstdint>

#define MXC_RESULT bool
#define MXC_SUCCESS true
#define MXC_FAIL false

// delete copy constructors to disallow pass by value
#define MXC_NO_VALUE_PASS(name) \
name(const name&) = delete; \
name& operator=(const name&) = delete;

namespace Moxaic
{
    inline constexpr uint32_t DefaultWidth = 1280;
    inline constexpr uint32_t DefaultHeight = 1024;
    inline constexpr char ApplicationName[] = "moxaic";
    inline constexpr uint8_t FramebufferCount = 2;

    inline MXC_RESULT Running = MXC_SUCCESS;

    enum class Role {
        Compositor,
        Node
    };

    inline Role Role = Role::Compositor;

    inline bool IsCompositor() {
        return Role == Role::Compositor;
    }

    inline const char *string_Role(enum Role role)
    {
        switch (role) {
            case Role::Compositor:
                return "Comp";
            case Role::Node:
                return "Node";
            default:
                return "Unknown Role";
        }
    }
}