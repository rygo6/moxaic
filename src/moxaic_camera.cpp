#include "moxaic_camera.hpp"

#include <glm/glm.hpp>

Moxaic::Camera::Camera()
{
    UpdateView();
    UpdateProjection();
}

Moxaic::Camera::~Camera() = default;

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
