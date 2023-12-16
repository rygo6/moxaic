#include "moxaic_camera.hpp"
#include "moxaic_window.hpp"

#include <glm/glm.hpp>

using namespace Moxaic;
using namespace glm;

Camera::Camera()
    : aspectRatio_(float(Window::GetExtents().width) / float(Window::GetExtents().height))
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
            constexpr auto zeroAxis = BitFlags<Transform::Axis>(Transform::X);
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
    inverseView_ = transform.ModelMatrix();
    view_ = inverse(inverseView_);

    inverseViewProjection_ = inverseView_ * inverseProjection_;
    viewProjection_ = projection_ * view_;
}

void Camera::UpdateProjection()
{
    projection_ = perspective(fieldOfView_, aspectRatio_, nearZPlane_, farZPlane_);
    inverseProjection_ = inverse(projection_);

    inverseViewProjection_ = inverseView_ * inverseProjection_;
    viewProjection_ = projection_ * view_;
}
