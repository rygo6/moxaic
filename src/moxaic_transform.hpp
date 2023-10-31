#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Moxaic
{
    class Transform
    {
    public:
        explicit Transform();
        virtual ~Transform();

        inline auto position() const { return m_Position; }
        inline auto rotation() const { return m_Orientation; }

        inline auto mat4() const
        {
            glm::mat4 rot = glm::toMat4(m_Orientation);
            glm::mat4 pos = glm::translate(glm::identity<glm::mat4>(), m_Position);
            return pos * rot;
        }

    private:
        glm::vec3 m_Position{glm::zero<glm::vec3>()};
        glm::quat m_Orientation{glm::identity<glm::quat>()};
//        glm::mat4 m_ModelMatrix{glm::mat4(1.0)};
    };
}
