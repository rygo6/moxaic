#include "moxaic_camera.hpp"
#include "moxaic_window.hpp"

#include <glm/glm.hpp>

Moxaic::Camera::Camera()
{
    UpdateView();
    UpdateProjection();
}

Moxaic::Camera::~Camera()
{

}

bool Moxaic::Camera::Update(uint32_t deltaTime)
{
    if (m_ActiveMovement != 0) {
        auto delta = glm::zero<glm::vec3>();
        if ((m_ActiveMovement & CameraMove::Forward) == CameraMove::Forward) {
            delta.z -= 1;
        }
        if ((m_ActiveMovement & CameraMove::Back) == CameraMove::Back) {
            delta.z += 1;
        }
        if ((m_ActiveMovement & CameraMove::Left) == CameraMove::Left) {
            delta.x -= 1;
        }
        if ((m_ActiveMovement & CameraMove::Right) == CameraMove::Right) {
            delta.x += 1;
        }
        m_Transform.LocalTranslate(delta * (float) deltaTime * 0.01f);

        if ((m_ActiveMovement & CameraMove::Rotation) == CameraMove::Rotation) {
            m_ActiveMovement = (CameraMove) (m_ActiveMovement & ~CameraMove::Rotation);
            m_Transform.Rotate(m_CameraRotationDelta);
        }

        UpdateView();
        return true;
    }

    return false;
}

void Moxaic::Camera::UpdateView()
{
    m_InverseView = m_Transform.modelMatrix();
    m_View = glm::inverse(m_InverseView);
}

void Moxaic::Camera::UpdateProjection()
{
    m_Projection = glm::perspective(m_FOV, m_Aspect, m_Near, m_Far);
    m_InverseProjection = glm::inverse(m_Projection);
}

void Moxaic::Camera::OnMouseMove(const MouseMotionEvent &event)
{
    if (m_CameraLocked) {
        m_ActiveMovement = (CameraMove) (m_ActiveMovement | CameraMove::Rotation);
        m_CameraRotationDelta.y = glm::radians(-event.delta.x);
    }
}

void Moxaic::Camera::OnMouse(const MouseEvent &event)
{
    if (event.button == Button::Left && event.phase == Phase::Pressed) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        m_CameraLocked = true;
    } else if (event.button == Button::Left && event.phase == Phase::Released) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        m_CameraLocked = false;
    }
}

void Moxaic::Camera::OnKey(const KeyEvent &event)
{
    switch (event.key) {
        case SDLK_w:
            m_ActiveMovement = (CameraMove) (event.phase == Phase::Pressed ?
                                             m_ActiveMovement | CameraMove::Forward :
                                             m_ActiveMovement & ~CameraMove::Forward);
            break;
        case SDLK_s:
            m_ActiveMovement = (CameraMove) (event.phase == Phase::Pressed ?
                                             m_ActiveMovement | CameraMove::Back :
                                             m_ActiveMovement & ~CameraMove::Back);
            break;
        case SDLK_a:
            m_ActiveMovement = (CameraMove) (event.phase == Phase::Pressed ?
                                             m_ActiveMovement | CameraMove::Left :
                                             m_ActiveMovement & ~CameraMove::Left);
            break;
        case SDLK_d:
            m_ActiveMovement = (CameraMove) (event.phase == Phase::Pressed ?
                                             m_ActiveMovement | CameraMove::Right :
                                             m_ActiveMovement & ~CameraMove::Right);
            break;
    }
}