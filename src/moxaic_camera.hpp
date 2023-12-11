#pragma once

#include "moxaic_transform.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#define MXC_CAMERA_MIN_Z 0.0001f

namespace Moxaic
{
    class Camera
    {
    public:
        MXC_NO_VALUE_PASS(Camera);

        Camera();
        virtual ~Camera() = default;

        bool UserCommandUpdate(uint32_t deltaTime);

        void UpdateView();
        void UpdateProjection();

        MXC_ACCESS(Transform);

        MXC_GET(View);
        MXC_GET(Projection);
        MXC_GET(InverseView);
        MXC_GET(InverseProjection);

        MXC_GETSET(FOV);
        MXC_GETSET(Near);
        MXC_GETSET(Far);
        // MXC_GETSET(Aspect);
        MXC_GET(Aspect);

    private:
        float m_FOV{45.0f};
        float m_Near{MXC_CAMERA_MIN_Z};
        float m_Far{1000.0f};
        float m_Aspect;

        Moxaic::Transform m_Transform{};
        glm::mat4x4 m_View{glm::identity<glm::mat4x4>()};
        glm::mat4x4 m_Projection{glm::identity<glm::mat4x4>()};
        glm::mat4x4 m_InverseView{glm::identity<glm::mat4x4>()};
        glm::mat4x4 m_InverseProjection{glm::identity<glm::mat4x4>()};

        bool m_CameraLocked{false};
    };
}// namespace Moxaic
