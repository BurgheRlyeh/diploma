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
	// old, for shader 
	std::vector<XMUINT4> m_bvhPrims{};
	// new, for calc
	std::list<XMUINT4> m_primMortonFrmLeaf{}; 
	// for O(1) frame access
	std::vector<std::list<XMUINT4>::iterator> m_frmIts{};
	// frame copy with add info
	std::vector<XMUINT4> m_frm{};
	// frame edge
	std::list<XMUINT4>::iterator m_edge{};


	int m_frmSize{};


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

	void buildStochastic();

private:
	// stochastic
	void mortonSort();
	UINT mortonShift(UINT x);
	UINT encodeMorton(const Vector4& v);

	float primInsertMetric(int primId, int nodeId);
	int findBestLeaf(int primId);

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

	void foo(int nodeId, int* offsets, int* id, int* first) {
		//if (m_nodesUsed <= id) 

		if (m_nodes[nodeId].leftCntPar.y) {
			m_nodes[nodeId].leftCntPar.x += *first;
			m_nodes[nodeId].leftCntPar.y += offsets[*id];
			*first += offsets[*id];
			offsets[*id] = 0;
			*id += 1;
			return;
		}

		foo(m_nodes[nodeId].leftCntPar.x, offsets, id, first);
		foo(m_nodes[nodeId].leftCntPar.x + 1, offsets, id, first);
	}

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