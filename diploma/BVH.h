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
	struct Primitive {
		Vector4 v0{}, v1{}, v2{};
		Vector4 ctr{};
		AABB bb{};
	};
	std::vector<Primitive> m_prims{};

public:
	struct BVHNode {
		AABB bb{};
		XMINT4 leftCntPar{};
	};
	std::vector<BVHNode> m_nodes{};
	//std::vector<BVHNode> m_internals{};
	//std::vector<BVHNode> m_leafs{};
	std::vector<XMINT4> m_primFrm{};
	std::vector<XMINT4> m_stoh{};

	INT m_primsCnt{};

	INT m_nodesUsed{ 1 };
	INT m_leafsCnt{};
	INT m_depthMin{ 2 * 1107 - 1 };
	INT m_depthMax{ -1 };

	INT m_alg{ 3 };
	INT m_primsPerLeaf{ 2 };
	INT m_sahSteps{ 8 };

	float m_frmPart{ 0.1f };
	float m_uniform{ 0.f };

	int m_primsInBVH{};

	void init(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix);
	void build();

	struct MortonPrim {
		int primId{};
		UINT mortonCode{};
	};
	std::vector<MortonPrim> m_mortonPrims{};

	void buildStochastic();

private:
	// stochastic
	void mortonSort();
	UINT mortonShift(UINT x);
	UINT encodeMorton(const Vector4& v);

	float primInsertMetric(int primId, int nodeId);
	int findBestLeaf(int primId);

	// sah, binned & other
	void updateDepths(INT id);

	float comp(Vector4 v, INT idx);

	void updateNodeBounds(INT nodeIdx);

	void splitDichotomy(BVHNode& node, int& axis, float& splitPos);

	float evaluateSAH(BVHNode& node, int axis, float pos);

	float splitSAH(BVHNode& node, int& axis, float& splitPos);

	float splitFixedStepSAH(BVHNode& node, int& axis, float& pos);

	float splitBinnedSAH(BVHNode& node, int& axis, float& splitPos);

	void subdivide(INT nodeId);

	float splitBinnedSAHStoh(BVHNode& node, int& axis, float& splitPos);
	void subdivieStochastic(int nodeId);
};