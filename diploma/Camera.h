#pragma once

#include "framework.h"

class Camera {
	const float ROTATE_COEF{ DirectX::XM_2PI / 1e3f };
	const float MOVE_COEF{ 2.0f };
	const float ZOOM_COEF{ 1e-2f };

	DirectX::SimpleMath::Vector3 m_poi{};

	float m_r{ 5.f };

	float m_angX{ -3.5f * DirectX::XM_PI / 4 };
	float m_angY{ DirectX::XM_PI / 6 };

	float m_dForward{};
	float m_dRight{};

public:
	void rotate(float dx, float dy);
	void zoom(float delta);
	void moveForward(float dir = 1.f);
	void moveRight(float dir = 1.f);
	void updatePosition(float delta);

	DirectX::SimpleMath::Vector3 getPoi();
	DirectX::SimpleMath::Vector3 getDir(float shift = 0.f);
	DirectX::SimpleMath::Vector3 getUp();
	DirectX::SimpleMath::Vector3 getPosition();

private:
	void getDirections(DirectX::SimpleMath::Vector3& forward, DirectX::SimpleMath::Vector3& right);
};