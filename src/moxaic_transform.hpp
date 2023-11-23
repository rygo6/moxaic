#pragma once

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

        Transform() = default;
        virtual ~Transform() = default;

        void LocalTranslate(glm::vec3 delta)
        {
            delta = glm::rotate(m_Orientation, delta);
            m_Position += delta;
        }

        void LocalTranslate(float const x, float const y, float const z)
        {
            auto delta = glm::vec3(x, y, z);
            delta = glm::rotate(m_Orientation, delta);
            m_Position += delta;
        }

        void Rotate(float const x, float const y, float const z)
        {
            auto const rotation = glm::quat(glm::vec3(
              glm::radians(x),
              glm::radians(y),
              glm::radians(z)));
            m_Orientation = rotation * m_Orientation;
        }

        void Rotate(glm::vec3 const euler)
        {
            auto const rotation = glm::quat(euler);
            m_Orientation = rotation * m_Orientation;
        }

        MXC_GETSET(Position);
        MXC_GETSET(Orientation);

        auto ModelMatrix() const
        {
            glm::mat4 const rot = glm::toMat4(m_Orientation);
            glm::mat4 const pos = glm::translate(glm::identity<glm::mat4>(), m_Position);
            return pos * rot;
        }

    private:
        glm::vec3 m_Position{glm::zero<glm::vec3>()};
        glm::quat m_Orientation{glm::identity<glm::quat>()};
    };
}// namespace Moxaic
