#include "CSVGeometryLoader.h"
#include <charconv>

using namespace DirectX;
using namespace DirectX::SimpleMath;

template <typename T>
T string_view_to(std::string_view sv) {
	T num{};
	auto res = std::from_chars(sv.data(), sv.data() + sv.size(), num);
	if (res.ec == std::errc::invalid_argument)
		throw std::invalid_argument("");
	return num;
}

CSVGeometryLoader CSVGeometryLoader::loadFrom(const std::string& filepath) {
	CSVGeometryLoader gl{};
	std::ifstream file{ filepath };

	XMINT4 triangle{};
	int tv{};
	int iter{};
	for (auto& row : CSVIterator(file)) {
		if (iter > 10) {
			break;
		}
		int id{};
		Vector4 vertex{};
		try {
			id = string_view_to<int>(row[1]);
			vertex = {
				string_view_to<float>(row[2]),
				string_view_to<float>(row[3]),
				string_view_to<float>(row[4]),
				string_view_to<float>(row[5])
			};
		}
		catch (const std::invalid_argument&) {
			continue;
		}

		switch (tv) {
		case 0:
			triangle.x = id;
			++tv;
			break;
		case 1:
			triangle.y = id;
			++tv;
			break;
		case 2:
			triangle.z = id;
			triangle.w = 0;
			tv = 0;
			gl.indices.push_back(triangle);
		}

		if (gl.vertices.size() <= id)
			gl.vertices.resize(static_cast<size_t>(id + 1));
		gl.vertices[id] = vertex;
	}

	return gl;
}