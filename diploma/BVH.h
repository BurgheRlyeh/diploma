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
public:
	struct BVHNode {
		AABB bb{};
		XMINT4 leftCntPar{};
	};
	std::vector<BVHNode> m_nodes{};
	std::vector<XMINT4> m_triIds{};

private:
	struct Triangle {
		Vector4 v0{}, v1{}, v2{};
		Vector4 ctr{};
	};
	std::vector<Triangle> m_tris{};

public:
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