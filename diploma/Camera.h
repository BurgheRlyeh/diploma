#pragma once

#include "framework.h"

class Camera {
	const float ROTATE_COEF{ 1.f / 1e3f };
	const float MOVE_COEF{ 2.0f };
	const float ZOOM_COEF{ 1.f / 100.f };

	DirectX::SimpleMath::Vector3 m_poi{};

	float m_r{ 5.f };

	float m_angZ{ - DirectX::XM_PI / 2 };// -3.5f * DirectX::XM_PI / 4 };
	float m_angY{};// DirectX::XM_PI / 6 };

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