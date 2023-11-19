#pragma once

#include "main.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Moxaic
{
    class Transform
    {
        MXC_NO_VALUE_PASS(Transform);
    public:
        Transform();
        virtual ~Transform();

        inline void LocalTranslate(glm::vec3 delta)
        {
            delta = glm::rotate(m_Orientation, delta);
            m_Position += delta;
        }

        inline void LocalTranslate(float x, float y, float z)
        {
            auto delta = glm::vec3(x, y, z);
            delta = glm::rotate(m_Orientation, delta);
            m_Position += delta;
        }

        inline void Rotate(float x, float y, float z)
        {
            auto rotation = glm::quat(glm::vec3(
                    glm::radians(x),
                    glm::radians(y),
                    glm::radians(z)));
            m_Orientation = rotation * m_Orientation;
        }

        inline void Rotate(glm::vec3 euler)
        {
            auto rotation = glm::quat(euler);
            m_Orientation = rotation * m_Orientation;
        }

        inline const auto &position() const { return m_Position; }
        inline void setPosition(glm::vec3 position)
        {
            m_Position = position;
        }
        inline const auto &orientation() const { return m_Orientation; }
        inline void setOrientation(glm::quat orientation)
        {
            m_Orientation = orientation;
        }

        inline const auto modelMatrix() const
        {
            glm::mat4 rot = glm::toMat4(m_Orientation);
            glm::mat4 pos = glm::translate(glm::identity<glm::mat4>(), m_Position);
            return pos * rot;
        }

    private:
        glm::vec3 m_Position{glm::zero<glm::vec3>()};
        glm::quat m_Orientation{glm::identity<glm::quat>()};
    };
}
