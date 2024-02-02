#include "moxaic_camera.hpp"
#include "moxaic_window.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/reciprocal.hpp>

using namespace Moxaic;
using namespace glm;

Camera::Camera()
    : aspectRatio(float(Window::GetExtents().width) / float(Window::GetExtents().height))
{}

bool Camera::UserCommandUpdate(const uint32_t deltaTime)
{
    bool updated = false;

    const auto& userCommand = Window::GetUserCommand();
    if (!cameraLocked_ && userCommand.leftMouseButtonPressed) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        cameraLocked_ = true;
    } else if (cameraLocked_ && !userCommand.leftMouseButtonPressed) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        cameraLocked_ = false;
    }

    if (cameraLocked_ && userCommand.mouseMoved) {
        const auto rotationX = radians(-userCommand.mouseDelta.x) * 0.5f;
        const auto rotationY = radians(userCommand.mouseDelta.y) * 0.5f;
        transform.Rotate(0, rotationX, 0);
        transform.LocalRotate(rotationY, 0, 0);
        updated = true;
    }

    const auto& userMove = userCommand.userMove;
    if (!userMove.None()) {
        auto delta = zero<vec3>();
        if (userMove.ContainsFlag(Window::UserMove::Forward)) {
            delta.z += 1;
        }
        if (userMove.ContainsFlag(Window::UserMove::Back)) {
            delta.z -= 1;
        }
        if (userMove.ContainsFlag(Window::UserMove::Left)) {
            delta.x -= 1;
        }
        if (userMove.ContainsFlag(Window::UserMove::Right)) {
            delta.x += 1;
        }
        if (length(delta) > 0.0f) {
            constexpr auto zeroAxis = BitFlags<Transform::AxisFlags>(Transform::X);
            transform.LocalTranslate(normalize(delta) * float(deltaTime) * 0.002f, zeroAxis);
            updated = true;
        }
    }

    if (updated) {
        UpdateView();
        return true;
    }

    return false;
}

void Camera::UpdateView()
{
    inverseView = transform.ModelMatrix();
    view = inverse(inverseView);

    inverseViewProjection = inverseView * inverseProjection;
    viewProjection = projection * view;
}

void Camera::UpdateProjection()
{
    projection = ReversePerspective(fieldOfView, aspectRatio, nearZPlane, farZPlane);
    inverseProjection = inverse(projection);

    inverseViewProjection = inverseView * inverseProjection;
    viewProjection = projection * view;

}
void Camera::UpdateViewProjection()
{
    inverseView = transform.ModelMatrix();
    view = inverse(inverseView);

    projection = ReversePerspective(fieldOfView, aspectRatio, nearZPlane, farZPlane);
    inverseProjection = inverse(projection);

    inverseViewProjection = inverseView * inverseProjection;
    viewProjection = projection * view;
}

mat4 Camera::ReversePerspective(float fov, float aspectRatio, float zNear, float zFar) {
    return perspective(fov, aspectRatio, zFar, zNear);
};

// mat4 Camera::PerspectiveFovReverseZLH_ZO(float fov, float width, float height, float zNear, float zFar) {
//     return perspectiveFovLH_ZO(fov, width, height, zFar, zNear);
// };
//
// mat4 Camera::InfinitePerspectiveFovReverseZLH_ZO(float fov, float width, float height, float zNear) {
//     const float h = cot(0.5f * fov);
//     const float w = h * height / width;
//     auto result = zero<glm::mat4>();
//     result[0][0] = w;
//     result[1][1] = h;
//     result[2][2] = 0.0f;
//     result[2][3] = 1.0f;
//     result[3][2] = zNear;
//     return result;
// };