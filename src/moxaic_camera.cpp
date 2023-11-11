#include "moxaic_camera.hpp"
#include "moxaic_window.hpp"

#include <glm/glm.hpp>
#include <functional>

bool g_CameraLocked = false;

enum CameraMove
{
    Forward = 1 << 0,
    Back = 1 << 1,
    Left = 1 << 2,
    Right = 1 << 3,
    Rotation = 1 << 4,
};

CameraMove g_ActiveMovement;
glm::vec3 g_CameraPositionDelta;
glm::vec3 g_CameraRotationDelta;

using funcType = void (Moxaic::Camera::*)(const Moxaic::MouseMotionEvent &);;

Moxaic::Camera::Camera()
{
    UpdateView();
    UpdateProjection();

    funcType func = &Camera::OnMouseMove;
    auto binding = std::bind(func, this, std::placeholders::_1);
    g_MouseMotionSubscribers.push_back(binding);
    g_MouseSubscribers.push_back(std::bind(&Camera::OnMouse, this, std::placeholders::_1));
    g_KeySubscribers.push_back(std::bind(&Camera::OnKey, this, std::placeholders::_1));
}

Moxaic::Camera::~Camera() = default;

bool Moxaic::Camera::Update(uint32_t deltaTime)
{
    if (g_ActiveMovement != 0) {
        auto delta = glm::zero<glm::vec3>();
        if ((g_ActiveMovement & CameraMove::Forward) == CameraMove::Forward) {
            delta.z -= 1;
        }
        if ((g_ActiveMovement & CameraMove::Back) == CameraMove::Back) {
            delta.z += 1;
        }
        if ((g_ActiveMovement & CameraMove::Left) == CameraMove::Left) {
            delta.x -= 1;
        }
        if ((g_ActiveMovement & CameraMove::Right) == CameraMove::Right) {
            delta.x += 1;
        }
        m_Transform.LocalTranslate(delta * (float) deltaTime * 0.01f);

        if ((g_ActiveMovement & CameraMove::Rotation) == CameraMove::Rotation) {
            g_ActiveMovement = (CameraMove) (g_ActiveMovement & ~CameraMove::Rotation);
            m_Transform.Rotate(g_CameraRotationDelta);
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
    if (g_CameraLocked) {
        g_ActiveMovement = (CameraMove) (g_ActiveMovement | CameraMove::Rotation);
        g_CameraRotationDelta.y = glm::radians(-event.delta.x);
    }
}

void Moxaic::Camera::OnMouse(const MouseEvent &event)
{
    if (event.button == Button::Left && event.phase == Phase::Pressed) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        g_CameraLocked = true;
    } else if (event.button == Button::Left && event.phase == Phase::Released) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        g_CameraLocked = false;
    }
}

void Moxaic::Camera::OnKey(const KeyEvent &event)
{
    switch (event.key) {
        case SDLK_w:
            g_ActiveMovement = (CameraMove) (event.phase == Phase::Pressed ?
                                             g_ActiveMovement | CameraMove::Forward :
                                             g_ActiveMovement & ~CameraMove::Forward);
            break;
        case SDLK_s:
            g_ActiveMovement = (CameraMove) (event.phase == Phase::Pressed ?
                                             g_ActiveMovement | CameraMove::Back :
                                             g_ActiveMovement & ~CameraMove::Back);
            break;
        case SDLK_a:
            g_ActiveMovement = (CameraMove) (event.phase == Phase::Pressed ?
                                             g_ActiveMovement | CameraMove::Left :
                                             g_ActiveMovement & ~CameraMove::Left);
            break;
        case SDLK_d:
            g_ActiveMovement = (CameraMove) (event.phase == Phase::Pressed ?
                                             g_ActiveMovement | CameraMove::Right :
                                             g_ActiveMovement & ~CameraMove::Right);
            break;
    }
}