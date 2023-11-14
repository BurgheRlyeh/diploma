#pragma once

#include "framework.h"
#include "CSVIterator.h"

struct CSVGeometryLoader {
	std::vector<DirectX::XMINT4> indices{};
	std::vector<DirectX::SimpleMath::Vector4> vertices{};

	static CSVGeometryLoader loadFrom(const std::string& filepath);
};