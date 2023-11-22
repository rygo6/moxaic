#include "moxaic_camera.hpp"
#include "moxaic_window.hpp"

#include <glm/glm.hpp>

using namespace Moxaic;

bool Camera::UserCommandUpdate(const uint32_t deltaTime)
{
    bool updated = false;

    const auto& userCommand = Window::userCommand();
    if (!m_CameraLocked && userCommand.leftMouseButtonPressed) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        m_CameraLocked = true;
    } else if (m_CameraLocked && !userCommand.leftMouseButtonPressed) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        m_CameraLocked = false;
    }

    if (m_CameraLocked && userCommand.mouseMoved) {
        const auto rotation = glm::radians(-userCommand.mouseDelta.x) * 1.0f;
        m_Transform.Rotate(0, rotation, 0);
        updated = true;
    }

    const auto& userMove = userCommand.userMove;
    if (!userMove.None()) {
        auto delta = glm::zero<glm::vec3>();
        if (userMove.ContainsFlag(Window::UserMove::Forward)) {
            delta.z -= 1;
        }
        if (userMove.ContainsFlag(Window::UserMove::Back)) {
            delta.z += 1;
        }
        if (userMove.ContainsFlag(Window::UserMove::Left)) {
            delta.x -= 1;
        }
        if (userMove.ContainsFlag(Window::UserMove::Right)) {
            delta.x += 1;
        }
        if (glm::length(delta) > 0.0f) {
            m_Transform.LocalTranslate(glm::normalize(delta) * (float) deltaTime * 0.005f);
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
    m_InverseView = m_Transform.modelMatrix();
    m_View = glm::inverse(m_InverseView);
}

void Camera::UpdateProjection()
{
    m_Projection = glm::perspective(m_FOV, m_Aspect, m_Near, m_Far);
    m_InverseProjection = glm::inverse(m_Projection);
}
