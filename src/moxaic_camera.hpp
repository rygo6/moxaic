#pragma once

#include "moxaic_transform.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#define MXC_CAMERA_MIN_Z 0.1f

using namespace glm;

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
        void UpdateViewProjection();

        static mat4 ReversePerspective(float fov, float aspectRatio, float zNear, float zFar);

        Moxaic::Transform transform{};

        float fieldOfView{45.0f};
        float nearZPlane{MXC_CAMERA_MIN_Z};
        float farZPlane{100.0f};
        float aspectRatio{800.0/600.0};

        const auto& GetView() const { return view; }
        const auto& GetProjection() const { return projection; }
        const auto& GetViewProjection() const { return viewProjection; }
        const auto& GetInverseView() const { return inverseView; }
        const auto& GetInverseProjection() const { return inverseProjection; }
        const auto& GetInverseViewProjection() const { return inverseViewProjection; }

    private:

        mat4x4 view{identity<mat4x4>()};
        mat4x4 projection{identity<mat4x4>()};
        mat4x4 viewProjection{identity<mat4x4>()};
        mat4x4 inverseView{identity<mat4x4>()};
        mat4x4 inverseProjection{identity<mat4x4>()};
        mat4x4 inverseViewProjection{identity<mat4x4>()};

        bool cameraLocked_{false};
    };
}// namespace Moxaic
