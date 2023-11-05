#include "moxaic_camera.hpp"

#include <glm/glm.hpp>

Moxaic::Camera::Camera()
{
    UpdateMatrices();
}

Moxaic::Camera::~Camera() = default;

void Moxaic::Camera::UpdateMatrices()
{
    m_Projection = glm::perspective(m_FOV, m_Aspect, m_Near, m_Far);
    m_InverseProjection = glm::inverse(m_Projection);

    m_InverseView = m_Transform.modelMatrix();
    m_View = glm::inverse(m_InverseView);
}
