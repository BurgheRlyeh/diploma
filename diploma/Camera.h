#pragma once

#include "framework.h"

struct Camera {
	DirectX::SimpleMath::Vector3 poi{};
	float r{};
	float angX{};
	float angY{};

	float rotationSpeed{ DirectX::XM_2PI };

	float dForward{};
	float dRight{};

	void move(float delta);

	DirectX::SimpleMath::Vector3 getDir(float shift = 0.f);
	DirectX::SimpleMath::Vector3 getUp();
	DirectX::SimpleMath::Vector3 getForward();
	DirectX::SimpleMath::Vector3 getRight();

	void getDirections(DirectX::SimpleMath::Vector3& forward, DirectX::SimpleMath::Vector3& right);
	DirectX::SimpleMath::Vector3 getPosition();
};