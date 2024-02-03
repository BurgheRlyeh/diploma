#pragma once

#include "framework.h"
#include "CSVIterator.h"

struct CSVGeometryLoader {
	static void loadFrom(
		const std::string& filepath,
		std::vector<DirectX::XMINT4>* indices,
		std::vector<DirectX::SimpleMath::Vector4>* vertices
	);
};