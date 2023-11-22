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

        const auto& position() const { return m_Position; }
        void setPosition(const glm::vec3& position)
        {
            m_Position = position;
        }
        const auto& orientation() const { return m_Orientation; }
        void setOrientation(const glm::quat& orientation)
        {
            m_Orientation = orientation;
        }

        auto modelMatrix() const
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
