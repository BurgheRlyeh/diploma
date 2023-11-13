#include "Camera.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void Camera::move(float delta) {
    Vector3 forward, right;
    getDirections(forward, right);
    poi += delta * (forward * dForward + right * dRight);
}

Vector3 Camera::getDir(float shift) {
    return {
        cosf(angY + shift) * cosf(angX),
        sinf(angY + shift),
        cosf(angY + shift) * sinf(angX)
    };
}

Vector3 Camera::getUp() {
    return getDir(XM_PIDIV2);
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

Vector3 Camera::getForward() {
    auto dir{ getDir() };

    auto forward{ XMMax(fabs(dir.x), fabs(dir.z)) <= 1e-5f ? getUp() : dir };
    forward.y = 0;
    forward.Normalize();

    return forward;
}

Vector3 Camera::getRight() {
    return -getUp().Cross(getDir());
}

Vector3 Camera::getPosition() {
    return poi + r * getDir();
}
