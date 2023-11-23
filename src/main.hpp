#pragma once

#include <cstdint>

#define MXC_RESULT bool
#define MXC_SUCCESS true
#define MXC_FAIL false

#define MXC_NO_VALUE_PASS(name) \
    name(const name&) = delete; \
    name& operator=(const name&) = delete;

#define MXC_GET(field) \
    auto const& Get##field() const { return m_##field; }

#define MXC_SET(field) \
    void Set##field(auto const& field) { m_##field = field; }

#define MXC_GETSET(field) \
    MXC_GET(field)        \
    MXC_SET(field)

#define MXC_GETARR(field) \
    auto const& field(int const i) const { return m_##field[i]; }

#define MXC_ACCESS(field) \
    auto* field() { return &m_##field; }

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

    inline bool IsCompositor()
    {
        return Role == Role::Compositor;
    }

    inline char const* string_Role(const enum Role role)
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
}// namespace Moxaic
