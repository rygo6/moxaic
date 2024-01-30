#pragma once

namespace Moxaic
{
    template<typename T>
    class BitFlags
    {
    public:
        template<typename... Args>
        constexpr BitFlags(Args... flags)
        {
            (SetFlag(flags), ...);
        }

        bool None() const
        {
            return flags == 0;
        }

        bool ContainsFlag(T flag) const
        {
            return (flags & flag) == flag;
        }

        void ToggleFlag(T flag, bool state)
        {
            state ? SetFlag(flag) : ClearFlag(flag);
        }

        constexpr void SetFlag(T flag)
        {
            flags = flags | flag;
        }

        void ClearFlag(T flag)
        {
            flags = flags & ~flag;
        }

    private:
        int flags{0};
    };
}
