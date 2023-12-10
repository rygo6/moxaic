#pragma once

#include "bit_flags.hpp"
#include "main.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Moxaic
{
    class Transform
    {
    public:
        MXC_NO_VALUE_PASS(Transform);

        enum Axis {
            X = 1 << 0,
            Y = 1 << 1,
            Z = 1 << 2,
        };

        Transform() = default;
        virtual ~Transform() = default;

        void LocalTranslate(glm::vec3 delta)
        {
            delta = glm::rotate(m_Orientation, delta);
            m_Position += delta;
        }

        void LocalTranslate(glm::vec3 delta, BitFlags<Axis> zeroOrientationAxis)
        {
            glm::vec3 eulerAngles = glm::eulerAngles(m_Orientation);
            eulerAngles.x = zeroOrientationAxis.ContainsFlag(X) ? 0 : eulerAngles.x;
            eulerAngles.y = zeroOrientationAxis.ContainsFlag(Y) ? 0 : eulerAngles.y;
            eulerAngles.z = zeroOrientationAxis.ContainsFlag(Z) ? 0 : eulerAngles.z;
            const auto lockedOrientation = glm::quat(eulerAngles);
            delta = glm::rotate(lockedOrientation, delta);
            m_Position += delta;
        }

        void LocalTranslate(const float x, const float y, const float z)
        {
            auto delta = glm::vec3(x, y, z);
            delta = glm::rotate(m_Orientation, delta);
            m_Position += delta;
        }

        void Rotate(const float x, const float y, const float z)
        {
            const auto rotation = glm::quat(glm::vec3(
              glm::radians(x),
              glm::radians(y),
              glm::radians(z)));
            m_Orientation = rotation * m_Orientation;
        }

        void Rotate(const glm::vec3 euler)
        {
            const auto rotation = glm::quat(euler);
            m_Orientation = rotation * m_Orientation;
        }

        void LocalRotate(const float x, const float y, const float z)
        {
            auto rotation = glm::quat(glm::vec3(
              glm::radians(x),
              glm::radians(y),
              glm::radians(z)));
            m_Orientation = m_Orientation * rotation;
        }

        MXC_GETSET(Position);
        MXC_GETSET(Orientation);

        auto ModelMatrix() const
        {
            const glm::mat4 rot = glm::toMat4(m_Orientation);
            const glm::mat4 pos = glm::translate(glm::identity<glm::mat4>(), m_Position);
            return pos * rot;
        }

    private:
        glm::vec3 m_Position{glm::zero<glm::vec3>()};
        glm::quat m_Orientation{glm::identity<glm::quat>()};
    };
}// namespace Moxaic
