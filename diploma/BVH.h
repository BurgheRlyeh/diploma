#pragma once

#include "framework.h"

#include "AABB.h"

struct AABB;

using namespace DirectX;
using namespace DirectX::SimpleMath;

#undef min
#undef max

#define MaxSteps 32

class BVH {
	struct Triangle {
		Vector4 v0{}, v1{}, v2{};
		Vector4 ctr{};
	};
	std::vector<Triangle> m_tris{};

	void mortonSort();
	UINT mortonShift(UINT x) {
		x = (x | (x << 16)) & 0b00000011000000000000000011111111;
		x = (x | (x << 8)) & 0b00000011000000001111000000001111;
		x = (x | (x << 4)) & 0b00000011000011000011000011000011;
		x = (x | (x << 2)) & 0b00001001001001001001001001001001;
		return x;
	}
	UINT encodeMorton(const Vector4& v) {
		return (mortonShift(v.z) << 2) | (mortonShift(v.y) << 1) | mortonShift(v.x);
	}

public:
	struct BVHNode {
		AABB bb{};
		XMINT4 leftCntPar{};
	};
	std::vector<BVHNode> m_nodes{};
	std::vector<XMINT4> m_triIds{};

	struct MortonPrim {
		int primId{};
		UINT mortonCode{};
	};
	std::vector<MortonPrim> m_mortonPrims{};

	INT m_triCnt{};

	INT m_nodesUsed{ 1 };
	INT m_leafs{};
	INT m_depthMin{ 2 * 1107 - 1 };
	INT m_depthMax{ -1 };

	INT m_alg{ 3 };
	INT m_primsPerLeaf{ 2 };
	INT m_sahSteps{ 8 };

	void init(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix);

	void build();
	void buildStochastic() {
	}

private:
	void updateDepths(INT id);

	float comp(Vector4 v, INT idx);

	void updateNodeBounds(INT nodeIdx);

	void splitDichotomy(BVHNode& node, int& axis, float& splitPos);

	float evaluateSAH(BVHNode& node, int axis, float pos);

	float splitSAH(BVHNode& node, int& axis, float& splitPos);

	float splitFixedStepSAH(BVHNode& node, int& axis, float& pos);

	float splitBinnedSAH(BVHNode& node, int& axis, float& splitPos);

	void subdivide(INT nodeId);
};