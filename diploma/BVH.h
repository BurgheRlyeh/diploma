#pragma once

#include "framework.h"

#include "AABB.h"
#include <list>

struct AABB;

using namespace DirectX;
using namespace DirectX::SimpleMath;

#define LIMIT_V 1013
#define LIMIT_I 1107

#undef min
#undef max

#define MaxSteps 32

class BVH {
	// ---------------
	//	GRAPHICS PART
	// ---------------
	struct ModelBuffer {
		AABB bb{};
		DirectX::SimpleMath::Color cl{};
	};

	ID3D11Device* m_pDevice{};
	ID3D11DeviceContext* m_pDeviceContext{};

	ID3D11Buffer* m_pVertexBuffer{};
	ID3D11Buffer* m_pIndexBuffer{};

	std::vector<ModelBuffer> m_modelBuffers{};
	ID3D11Buffer* m_pModelBuffer{};
	ID3D11ShaderResourceView* m_pModelBufferSRV{};

	ID3D11Buffer* m_pBVHBuffer{};
	ID3D11ShaderResourceView* m_pBVHBufferSRV{};

	ID3D11Buffer* m_pPrimIdsBuffer{};
	ID3D11ShaderResourceView* m_pPrimIdsBufferSRV{};

	ID3D11VertexShader* m_pVertexShader{};
	ID3D11PixelShader* m_pPixelShader{};
	ID3D11InputLayout* m_pInputLayout{};

	bool m_aabbHighlightAll{};
	int m_aabbHighlightNode{};
	bool m_aabbHighlightSubtree{};
	int m_aabbHighlightSubtreeDepth{};
	bool m_aabbHighlightOne{};
	bool m_aabbHighlightParent{};
	bool m_aabbHighlightSibling{};
	bool m_aabbHighlightChildren{};

	bool m_aabbHighlightPrims{};
	bool m_aabbHighlightAllPrims{};

	bool m_highlightFramePrims{};
	int m_highlightPrim{};

public:
	BVH() = delete;
	BVH(ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext, unsigned int primsCnt);

	void term();

	ID3D11ShaderResourceView* getPrimIdsBufferSRV() {
		return m_pPrimIdsBufferSRV;
	}

	ID3D11ShaderResourceView* getBVHBufferSRV() {
		return m_pBVHBufferSRV;
	}

	void updateRenderBVH();
	void updateBuffers();

	void renderBVHImGui();
	void renderAABBsImGui();

private:
	// ------------
	//	LOGIC PART
	// ------------
	struct Primitive {
		Vector4 v0{}, v1{}, v2{};
		Vector4 ctr{};
		AABB bb{};

		void updCtrAndBB() {
			ctr = (v0 + v1 + v2) / 3.f;
			bb.grow(v0);
			bb.grow(v1);
			bb.grow(v2);
		}
	};
	std::vector<Primitive> m_prims{};

	struct BVHNode {
		AABB bb{};
		XMINT4 leftCntPar{};
	};
	std::vector<BVHNode> m_nodes{};

	struct PrimRef {
		unsigned primId;
		union {
			unsigned mortonCode;
			unsigned next;
			unsigned primOrig;
		};
		union {
			unsigned subsetNearest;
			unsigned isAABBHighlight;
		};
		union {
			unsigned leafId;
			unsigned isSplitHighlight;
		};
	};

	std::vector<PrimRef> m_primRefs{};
	
	std::vector<PrimRef>::iterator m_edge{};

	AABB m_aabbAllCtrs{};
	AABB m_aabbAllPrims{};

	int m_primsCntOrig{};
	INT m_primsCnt{};

	INT m_nodesUsed{ 1 };
	INT m_leafsCnt{};
	INT m_depthMin{ 2 * 1107 - 1 };
	INT m_depthMax{ -1 };

	// 0 - dichotomy
	// 1 - sah
	// 2 - fixed step sah
	// 3 - binned sah
	// 4 - stochastic
	// 5 - psr
	INT m_algBuild{ 4 };
	INT m_primsPerLeaf{ 2 };
	INT m_sahSteps{ 32 };
	// 0 - bruteforce
	// 1 - morton
	// 2 - smart bvh
	int m_algInsert{ 2 };
	
	// 0 - orig
	// 1 - upd prims cnt
	// 2 - upd aabb
	// 3 - upd prims cnt & aabb
	int m_algInsertConds{ 3 };

	bool m_toQBVH{ true };

	// 0 - no prims splitting
	// 1 - subset splitting before clustering
	// 2 - prev clamp hist interval (naive)
	// 3 - clamped w/o uniform (smart)
	int m_primSplitting{};

	float m_primWeightMin{};
	float m_primWeightMax{};

	float m_clampBase{ sqrtf(2.f) };
	int m_clampOffset{ 32 };
	int m_clampBinCnt{ 64 };

	float m_clamp{};
	int m_clampedCnt{};
	int m_splitCnt{};

	float m_frmPart{ 0.2f };
	float m_uniform{ 0.1f };
	int m_insertSearchWindow{ 10 };

	int m_frmSize{};

public:
	void render(ID3D11SamplerState* pSampler, ID3D11Buffer* pSceneBuffer);

	void build(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix);

	float costSAH(int nodeId = 0);

	int depth(int id) {
		int d{};
		for (; id; id = m_nodes[id].leftCntPar.z) ++d;
		return d;
	}

private:
	void init(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix);

	void buildPsr(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix);
	void binaryBVH2QBVH();
	void buildStochastic();

	template <typename T>
	void preForEach(int nodeId, T f) {
		f(nodeId);

		if (!m_nodes[nodeId].leftCntPar.y) {
			preForEach(m_nodes[nodeId].leftCntPar.x, f);
			preForEach(m_nodes[nodeId].leftCntPar.x + 1, f);
		}
	}

	template <typename T>
	void preForEachQuad(int nodeId, T f) {
		f(nodeId);

		if (!m_nodes[nodeId].leftCntPar.y) {
			for (int i{}; i < m_nodes[nodeId].leftCntPar.w; ++i) {
				preForEachQuad(m_nodes[nodeId].leftCntPar.x + i, f);
			}
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

	// stochastic
	void mortonSort();
	UINT mortonShift(UINT x);
	UINT encodeMorton(const Vector4& v);

	float primInsertMetric(int primId, int nodeId);

	int findBestLeafBruteforce(int primId);
	int findBestLeafMorton(int primId, int frmNearest);
	int findBestLeafSmartBVH(int primId, int frmNearest);

	// TODO with backtrack memory
	int leftLeaf(int leaf) {
		if (!m_nodes[leaf].leftCntPar.y)
			return -1;

		int node{ m_nodes[leaf].leftCntPar.z }, prev{ leaf };

		// up
		while (node && prev == m_nodes[node].leftCntPar.x) {
			prev = node;
			node = m_nodes[node].leftCntPar.z;
		}

		// most left check
		if (!node && prev == m_nodes[node].leftCntPar.x)
			return -1;

		// to left subtree
		node = m_nodes[node].leftCntPar.x;

		// down
		while (!m_nodes[node].leftCntPar.y) {
			node = m_nodes[node].leftCntPar.x + 1;
		}

		return node;
	}
	int rightLeaf(int leaf) {
		// check leaf
		if (!m_nodes[leaf].leftCntPar.y)
			return -1;

		int node{ m_nodes[leaf].leftCntPar.z }, prev{ leaf };

		// up
		while (node && prev == m_nodes[node].leftCntPar.x + 1) {
			prev = node;
			node = m_nodes[node].leftCntPar.z;
		}

		// most right check
		if (!node && prev == m_nodes[node].leftCntPar.x + 1)
			return -1;

		// to right subtree
		node = m_nodes[node].leftCntPar.x + 1;

		// down
		while (!m_nodes[node].leftCntPar.y) {
			node = m_nodes[node].leftCntPar.x;
		}

		return node;
	}

	void subdivideStohQueue(INT rootId, bool swapPrimIdOnly);
	void subdivideStohIntelQueue(INT rootId);
	void updateNodeBoundsStoh(INT nodeIdx);
	float splitBinnedSAHStoh(BVHNode& node, int& axis, float& splitPos, int& leftCnt, int& rightCnt);
	float splitSBVH(BVHNode& node, int& axis, float& splitPos, int& leftCnt, int& rightCnt);

	// sah, binned & other
	void updateDepths(INT id);

	float& comp(Vector4& v, INT idx);

	void updateNodeBounds(INT nodeIdx);

	void splitDichotomy(BVHNode& node, int& axis, float& splitPos);

	float evaluateSAH(BVHNode& node, int axis, float pos);

	float splitSAH(BVHNode& node, int& axis, float& splitPos);

	float splitFixedStepSAH(BVHNode& node, int& axis, float& pos);

	float splitBinnedSAH(BVHNode& node, int& axis, float& splitPos);
	
	void subdivide(INT nodeId);
};