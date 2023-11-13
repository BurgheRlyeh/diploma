#include "Camera.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void Camera::rotate(float dx, float dy) {
    m_angX -= dx * ROTATE_COEF;
    m_angY += dy * ROTATE_COEF;
    m_angY = (std::max)(m_angY, 0.1f - XM_PIDIV2);
    m_angY = (std::min)(m_angY, XM_PIDIV2 - 0.1f);
}

void Camera::zoom(float delta) {
    m_r = (std::max)(1.f, m_r - delta * ZOOM_COEF);
}

void Camera::moveForward(float dir) {
    m_dForward -= dir * MOVE_COEF;
}

void Camera::moveRight(float dir) {
    m_dRight += dir * MOVE_COEF;
}

void Camera::updatePosition(float delta) {
    Vector3 forward, right;
    getDirections(forward, right);
    m_poi += delta * (forward * m_dForward + right * m_dRight);
}

Vector3 Camera::getPoi() {
    return m_poi;
}

Vector3 Camera::getDir(float shift) {
    return {
        cosf(m_angY + shift) * cosf(m_angX),
        sinf(m_angY + shift),
        cosf(m_angY + shift) * sinf(m_angX)
    };
}

Vector3 Camera::getUp() {
    return getDir(XM_PIDIV2);
}

Vector3 Camera::getPosition() {
    return m_poi + m_r * getDir();
}

void Camera::getDirections(Vector3& forward, Vector3& right) {
    auto dir{ getDir() };
    auto up{ getUp() };

    forward = XMMax(fabs(dir.x), fabs(dir.y)) <= 1e-5f ? up : dir;
    forward.y = 0.f;
    forward.Normalize();

    right = up.Cross(dir);
    right.y = 0.f;
    right.Normalize();
}
