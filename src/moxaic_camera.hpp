#pragma once

#include "moxaic_transform.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace Moxaic
{
    class Camera
    {
    public:
        MXC_NO_VALUE_PASS(Camera);

        Camera() = default;
        virtual ~Camera() = default;

        bool UserCommandUpdate(uint32_t deltaTime);

        void UpdateView();
        void UpdateProjection();

        auto& transform() { return m_Transform; }

        // do I like this?
        MXC_GETSET(FOV);
        MXC_GETSET(Near);
        MXC_GETSET(Far);
        MXC_GETSET(Aspect);

        const auto& view() const { return m_View; }
        const auto& projection() const { return m_Projection; }
        const auto& inverseView() const { return m_InverseView; }
        const auto& inverseProjection() const { return m_InverseProjection; }

    private:
        float m_FOV{45.0f};
        float m_Near{0.0001f};
        float m_Far{100.0f};
        float m_Aspect{800.0f / 600.0f};

        Transform m_Transform{};
        glm::mat4x4 m_View{glm::identity<glm::mat4x4>()};
        glm::mat4x4 m_Projection{glm::identity<glm::mat4x4>()};
        glm::mat4x4 m_InverseView{glm::identity<glm::mat4x4>()};
        glm::mat4x4 m_InverseProjection{glm::identity<glm::mat4x4>()};

        bool m_CameraLocked{};
    };
}
