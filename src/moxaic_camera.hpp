#pragma once

#include "moxaic_transform.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#define MXC_CAMERA_MIN_Z 0.05f

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

        Moxaic::Transform transform{};

        MXC_GET2(view);
        MXC_GET2(projection);
        MXC_GET2(viewProjection);
        MXC_GET2(inverseView);
        MXC_GET2(inverseProjection);
        MXC_GET2(inverseViewProjection);

        MXC_GET2(fieldOfView);
        MXC_GET2(nearZPlane);
        MXC_GET2(farZPlane);
        MXC_GET2(aspectRatio);

    private:

        float fieldOfView_{45.0f};
        float nearZPlane_{MXC_CAMERA_MIN_Z};
        float farZPlane_{1000.0f};
        float aspectRatio_{800.0/600.0};

        mat4x4 view_{identity<mat4x4>()};
        mat4x4 projection_{identity<mat4x4>()};
        mat4x4 viewProjection_{identity<mat4x4>()};
        mat4x4 inverseView_{identity<mat4x4>()};
        mat4x4 inverseProjection_{identity<mat4x4>()};
        mat4x4 inverseViewProjection_{identity<mat4x4>()};

        bool cameraLocked_{false};
    };
}// namespace Moxaic
