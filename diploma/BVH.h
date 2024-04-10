#pragma once

#include "framework.h"

#include "AABB.h"
#include <algorithm>
#include <functional>
#include <map>
#include <stack>

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
	struct Prim {
		int primId;
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
	std::vector<Prim> m_prims{};

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
	std::map<unsigned, std::vector<unsigned>> m_subset2leafs{};
	
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
	// 6 - sbvh
	INT m_algBuild{ 4 };
	INT m_primsPerLeaf{ 2 };
	INT m_sahSteps{ 32 };
	// 0 - bruteforce
	// 1 - morton
	// 2 - smart bvh
	int m_algInsert{ 2 };

	// 0 - binned sah
	// 1 - sbvh
	int m_algSubsetBuild{ 1 };
	// 0 - binned sah
	// 1 - sbvh
	int m_algNotSubsetBuild{ 1 };

	// 0 - orig
	// 1 - upd prims cnt
	// 2 - upd aabb
	// 3 - upd prims cnt & aabb
	int m_algInsertConds{ 2 };

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

	template <typename T>
	void forEachLeaf(int nodeId, T f) {
		std::stack<int> nodes{};
		nodes.push(nodeId);

		while (!nodes.empty()) {
			int n{ nodes.top() };
			nodes.pop();

			if (m_nodes[n].leftCntPar.y) {
				f(n);
			}
			else {
				nodes.push(m_nodes[n].leftCntPar.x + 1);
				nodes.push(m_nodes[n].leftCntPar.x);
			}
		}
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

	void subdivideSBVHStohQueue(INT rootId, bool swapPrimIdOnly, std::function<void(int)> leafProc);
	void subdivideStohQueue(INT rootId, bool swapPrimIdOnly);
	void subdivideStohIntelQueue(INT rootId);
	void updateNodeBoundsStoh(INT nodeIdx);
	void updateNodeBoundsSBVH(INT nodeIdx);
	float splitBinnedSAHStoh4SBVH(BVHNode& node, int& axis, float& splitPos, AABB& leftBb, int& leftCnt, AABB& rightBb, int& rightCnt);
	float splitBinnedSAHStoh(BVHNode& node, int& axis, float& splitPos, AABB& leftBb, int& leftCnt, AABB& rightBb, int& rightCnt);
	float splitSBVH(BVHNode& node, int& axis, float& splitPos, AABB& leftBb, int& leftCnt, AABB& rightBb, int& rightCnt);

	std::vector<Vector4>& primPlaneIntersections(std::vector<Vector4>& vts, int dim, float plane) {
		std::vector<Vector4> intersections{};

		for (int i{}; i < 3; ++i) {
			Vector4 v0{ vts[i] };
			Vector4 v1{ vts[(i + 1) % 3] };

			float v0dim(comp(vts[i], dim));
			float v1dim(comp(vts[(i + 1) % 3], dim));

			if (plane < v0dim || v1dim < plane)
				continue;

			// both on plane
			if (v1dim - v0dim < std::numeric_limits<float>::epsilon()) {
				intersections.push_back(v0);
				intersections.push_back(v1);
			}
			// find intersection
			else {
				intersections.push_back(Vector4::Lerp(v0, v1, (plane - v0dim) / (v1dim - v0dim)));
			}
		}

		return intersections;
	}

	std::pair<AABB, AABB> splitPrimSuperNaive(const Prim& prim, AABB space, int dim, float plane) {
		AABB left{ prim.bb }, right{ prim.bb };
		comp(left.bmax, dim) = comp(right.bmin, dim) = plane;
		return { AABB::bbIntersection(space, left), AABB::bbIntersection(space, right) };
	}

	std::pair<AABB, AABB> splitPrimNaive(const Prim& prim, AABB space, int dim, float plane) {
		AABB left{}, right{};
		
		Vector4 vts[3]{ prim.v0, prim.v1, prim.v2 };

		for (int i{}; i < 3; ++i) {
			Vector4 v0{ vts[i] };
			Vector4 v1{ vts[(i + 1) % 3] };

			float v0p{ comp(v0, dim) };
			float v1p{ comp(v1, dim) };

			if (v0p <= plane)
				left.grow(v0);
			if (plane <= v0p)
				right.grow(v0);

			if ((v0p < plane && plane < v1p) || (v1p < plane && plane < v0p)) {
				Vector4 intersection{
					Vector4::Lerp(v0, v1, std::max<float>(0.f, std::min<float>((plane - v0p) / (v1p - v0p), 1.f)))
				};
				left.grow(intersection);
				right.grow(intersection);
			}
		}

		comp(left.bmax, dim) = plane;
		comp(right.bmin, dim) = plane;
		return { AABB::bbIntersection(space, left), AABB::bbIntersection(space, right) };
	}

	std::pair<AABB, AABB> splitPrimSmart(const Prim& prim, AABB space, int dim, float plane) {
		AABB left{}, right{};
		
		Vector4 vts[3]{ prim.v0, prim.v1, prim.v2 };

		for (int i{}; i < 3; ++i) {
			Vector4 vtx0{ vts[i] };
			Vector4 vtx1{ vts[(i + 1) % 3] };

			Vector3 v0{ vtx0.x, vtx0.y, vtx0.z };
			Vector3 v1{ vtx1.x, vtx1.y, vtx1.z };

			Ray edge{ v0, v1 - v0 };

			std::vector<Vector4> intsecs{};

			if (space.contains(vtx0))
				intsecs.push_back(vtx0);
			if (space.contains(vtx1))
				intsecs.push_back(vtx1);

			for (int a{}; a < 3 && intsecs.size() != 2; ++a) {
				Vector3 norm{};
				comp(norm, a) = 1.f;

				Plane pmin{ { space.bmin.x, space.bmin.y, space.bmin.z }, norm };
				Plane pmax{ { space.bmax.x, space.bmax.y, space.bmax.z }, norm };

				float t{};
				if (edge.Intersects(pmin, t) && t <= 1) {
					Vector4 intsec{ Vector4::Lerp(vtx0, vtx1, t) };
					intsecs.push_back(intsec);
				}
				if (edge.Intersects(pmax, t) && t <= 1) {
					Vector4 intsec{ Vector4::Lerp(vtx0, vtx1, t) };
					intsecs.push_back(intsec);
				}
			}

			if (intsecs.empty()) {
				continue;
			}

			assert(intsecs.size() == 2);

			vtx0 = intsecs[0];
			vtx1 = intsecs[1];

			if (comp(vtx0, dim) > comp(vtx1, dim))
				std::swap(vtx0, vtx1);

			comp(vtx0, dim) < plane ? left.grow(vtx0) : right.grow(vtx0);
			comp(vtx1, dim) < plane ? left.grow(vtx1) : right.grow(vtx1);
			if (comp(vtx0, dim) < plane && plane < comp(vtx1, dim)) {
				Vector4 intersec{
					Vector4::Lerp(
						vtx0,
						vtx1,
						std::max<float>(
							0.f,
							std::min<float>(
								(plane - comp(vtx0, dim)) / (comp(vtx1, dim) - comp(vtx0, dim)),
								1.f
							)
						)
					)
				};
				left.grow(intersec);
				right.grow(intersec);
			}
		}
		
		return { left, right };
		//return {
		//	AABB::bbIntersection(prim.bb, left),
		//	AABB::bbIntersection(prim.bb, right)
		//};
	}

	// sah, binned & other
	void updateDepths(INT id);

	float& comp(Vector3& v, INT idx);
	float& comp(Vector4& v, INT idx);

	void updateNodeBounds(INT nodeIdx);

	void splitDichotomy(BVHNode& node, int& axis, float& splitPos);

	float evaluateSAH(BVHNode& node, int axis, float pos);

	float splitSAH(BVHNode& node, int& axis, float& splitPos);

	float splitFixedStepSAH(BVHNode& node, int& axis, float& pos);

	float splitBinnedSAH(BVHNode& node, int& axis, float& splitPos);
	
	void subdivide(INT nodeId);
};