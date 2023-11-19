#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_transform.hpp"
#include "moxaic_window.hpp"

#include <vulkan/vulkan.h>
#include <windows.h>

#include <glm/glm.hpp>

namespace Moxaic
{
    class Camera
    {
        MXC_NO_VALUE_PASS(Camera);
    public:
        Camera();
        virtual ~Camera();

        bool UserCommandUpdate(uint32_t deltaTime);

        void UpdateView();
        void UpdateProjection();

        inline auto &transform() { return m_Transform; }

        inline auto view() const { return m_View; }
        inline auto projection() const { return m_Projection; }
        inline auto inverseView() const { return m_InverseView; }
        inline auto inverseProjection() const { return m_InverseProjection; }

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
