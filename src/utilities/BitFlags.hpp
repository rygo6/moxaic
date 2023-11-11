#pragma once

namespace Moxaic
{
    template<typename T>
    struct BitFlags
    {
    public:
        inline bool None() const
        {
            return m_Flags == 0;
        }

        inline bool ContainsFlag(T flag) const
        {
            return (m_Flags & flag) == flag;
        }

        inline void ToggleFlag(T flag, bool state)
        {
            state ? SetFlag(flag) : ClearFlag(flag);
        }

        inline void SetFlag(T flag)
        {
            m_Flags = m_Flags | flag;
        }

        inline void ClearFlag(T flag)
        {
            m_Flags = m_Flags & ~flag;
        }

    private:
        int m_Flags;
    };
}