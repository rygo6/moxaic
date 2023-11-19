#pragma once

namespace Moxaic
{
    template<typename T>
    struct BitFlags
    {
    public:
        bool None() const
        {
            return m_Flags == 0;
        }

        bool ContainsFlag(T flag) const
        {
            return (m_Flags & flag) == flag;
        }

        void ToggleFlag(T flag, bool state)
        {
            state ? SetFlag(flag) : ClearFlag(flag);
        }

        void SetFlag(T flag)
        {
            m_Flags = m_Flags | flag;
        }

        void ClearFlag(T flag)
        {
            m_Flags = m_Flags & ~flag;
        }

    private:
        int m_Flags{0};
    };
}