#pragma once

#include "framework.h"

#include "AABB.h"
#include <list>

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
	
	std::vector<XMUINT4> m_bvhPrims{};	// old, for shader 
	std::list<XMUINT4> m_primMortonFrmLeaf{};	// new, for calc
	std::vector<std::list<XMUINT4>::iterator> m_frmIts{};	// for O(1) frame access
	std::vector<XMUINT4> m_frm{};	// frame copy with add info
	std::list<XMUINT4>::iterator m_edge{};	// frame edge

	int m_frmSize{};

	INT m_primsCnt{};

	INT m_nodesUsed{ 1 };
	INT m_leafsCnt{};
	INT m_depthMin{ 2 * 1107 - 1 };
	INT m_depthMax{ -1 };

	// 0 - dichotomy
	// 1 - sah
	// 2 - fixed step sah
	// 3 - binned sah
	// 4  - stochastic
	INT m_alg{ 3 };
	INT m_primsPerLeaf{ 2 };
	INT m_sahSteps{ 8 };

	float m_frmPart{ 0.1f };
	float m_uniform{ 0.f };


	int m_primsInBVH{};

	void init(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix);
	void build();

	void buildStochastic();
	float costSAH();

	int depth(int id) {
		int d{};
		for (; id != 0; id = m_nodes[id].leftCntPar.z) ++d;
		return d;
	}

	template <typename T>
	void preForEach(int nodeId, T f) {
		f(nodeId);

		if (!m_nodes[nodeId].leftCntPar.y) {
			preForEach(m_nodes[nodeId].leftCntPar.x, f);
			preForEach(m_nodes[nodeId].leftCntPar.x + 1, f);
		}
	}

	template <typename T>
	void postForEach(int nodeId, T f) {
		if (!m_nodes[nodeId].leftCntPar.y) {
			postForEach(m_nodes[nodeId].leftCntPar.x, f);
			postForEach(m_nodes[nodeId].leftCntPar.x + 1, f);
		}

		f(nodeId);
	}

private:
	// stochastic
	void mortonSort();
	UINT mortonShift(UINT x);
	UINT encodeMorton(const Vector4& v);

	float primInsertMetric(int primId, int nodeId);
	int findBestLeaf(int primId);

	void subdivideStoh(INT nodeId);
	void subdivideStoh2(INT nodeId);
	void updateNodeBoundsStoh(INT nodeIdx);
	float splitBinnedSAHStoh(BVHNode& node, int& axis, float& splitPos, int& leftCnt, int& rightCnt);

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
};