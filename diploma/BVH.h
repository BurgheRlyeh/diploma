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
	std::vector<BVHNode> nodes{};
	std::vector<XMINT4> triIds{};

private:
	struct Triangle {
		Vector4 v0{}, v1{}, v2{};
		Vector4 ctr{};
	};
	std::vector<Triangle> tris{};
	INT triCnt{};

public:
	INT nodesUsed{ 1 };

	// sah settings
	bool isStepSAH{ true };
	bool isBinsSAH{ true };	// true / false
	INT sahStep{ 8 };


	void init(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt) {
		nodesUsed = 1;
		triCnt = idsCnt;
		tris.resize(triCnt);
		triIds.resize(triCnt);
		nodes.resize(2 * triCnt - 1);

		for (INT i{}; i < triCnt; ++i) {
			tris[i] = {
				vts[ids[i].x],
				vts[ids[i].y],
				vts[ids[i].z]
			};
			tris[i].ctr = (tris[i].v0 + tris[i].v1 + tris[i].v2) / 3.f;

			triIds[i] = { i, 0, 0, 0 };
		}
	}

	void build() {
		BVHNode& root = nodes[0];
		root.leftCntPar = {
			0, triCnt, -1, 0
		};
		updateNodeBounds(0);
		subdivide(0);
	}

private:
	void updateNodeBounds(INT nodeIdx) {
		BVHNode& node = nodes[nodeIdx];
		node.bb = {};
		for (INT i{}; i < node.leftCntPar.y; ++i) {
			Triangle& leafTri = tris[triIds[node.leftCntPar.x + i].x];

			node.bb.grow(leafTri.v0);
			node.bb.grow(leafTri.v1);
			node.bb.grow(leafTri.v2);
		}
	}

	float comp(Vector4 v, INT idx) {
		switch (idx) {
		case 0: return v.x;
		case 1: return v.y;
		case 2: return v.z;
		case 3: return v.w;
		default: throw;
		}
	}

	void splitDichotomy(BVHNode& node, int& axis, float& splitPos) {
		Vector4 e{ node.bb.extent() };
		axis = static_cast<int>(e.x < e.y);
		axis += static_cast<int>(comp(e, axis) < e.z);
		splitPos = comp(node.bb.bmin + e / 2.f, axis);
	}

	float evaluateSAH(BVHNode& node, int axis, float pos) {
		AABB leftBox{}, rightBox{};
		int leftCnt{}, rightCnt{};
		for (int i{}; i < node.leftCntPar.y; ++i) {
			Triangle& t = tris[triIds[node.leftCntPar.x + i].x];

			if (comp(t.ctr, axis) < pos) {
				++leftCnt;
				leftBox.grow(t.v0);
				leftBox.grow(t.v1);
				leftBox.grow(t.v2);
			}
			else {
				++rightCnt;
				rightBox.grow(t.v0);
				rightBox.grow(t.v1);
				rightBox.grow(t.v2);
			}
		}
		float cost{ leftCnt * leftBox.area() + rightCnt * rightBox.area() };
		return cost > 0 ? cost : std::numeric_limits<float>::max();
	}

	float splitSAH(BVHNode& node, int& axis, float& splitPos) {
		float bestCost{ std::numeric_limits<float>::max() };
		for (int a{}; a < 3; ++a) {
			for (int i{}; i < node.leftCntPar.y; ++i) {
				Triangle& t = tris[triIds[node.leftCntPar.x + i].x];
				Vector4 center{ t.ctr };
				float pos = comp(center, a);
				float cost = evaluateSAH(node, a, pos);
				if (cost < bestCost) {
					axis = a;
					splitPos = pos;
					bestCost = cost;
				}
			}
		}

		return bestCost;
	}

	float splitFixedStepSAH(BVHNode& node, int& axis, float& pos) {
		float bestCost{ std::numeric_limits<float>::max() };
		for (int a{}; a < 3; ++a) {
			float bmin{ comp(node.bb.bmin, a) };
			float bmax{ comp(node.bb.bmax, a) };

			if (bmin == bmax)
				continue;

			float step{ (bmax - bmin) / sahStep };
			for (int i{ 1 }; i < sahStep; ++i) {
				float candPos{ bmin + i * step };
				float cost = evaluateSAH(node, a, candPos);
				if (cost < bestCost) {
					axis = a;
					pos = candPos;
					bestCost = cost;
				}
			}
		}

		return bestCost;
	}


	float splitBinnedSAH(BVHNode& node, int& axis, float& splitPos) {
		float bestCost{ std::numeric_limits<float>::max() };
		for (int a{}; a < 3; ++a) {
			float bmin{ comp(node.bb.bmin, a) };
			float bmax{ comp(node.bb.bmax, a) };
			if (bmin == bmax)
				continue;

			AABB bounds[MaxSteps]{};
			int triCnt[MaxSteps]{};

			float step = sahStep / (bmax - bmin);
			for (int i{}; i < node.leftCntPar.y; ++i) {
				Triangle& t = tris[triIds[node.leftCntPar.x + i].x];
				int id{ std::min(
					sahStep - 1,
					static_cast<int>((comp(t.ctr, a) - bmin) * step)
				) };
				++triCnt[id];
				bounds[id].grow(t.v0);
				bounds[id].grow(t.v1);
				bounds[id].grow(t.v2);
			}

			float lArea[MaxSteps - 1]{}, rArea[MaxSteps - 1]{};
			int lCnt[MaxSteps - 1]{}, rCnt[MaxSteps - 1]{};
			AABB lBox{}, rBox{};
			int lSum{}, rSum{};

			for (int i{}; i < sahStep - 1; ++i) {
				lSum += triCnt[i];
				lCnt[i] = lSum;
				lBox.grow(bounds[i]);
				lArea[i] = lBox.area();

				rSum += triCnt[sahStep - 1 - i];
				rCnt[sahStep - 2 - i] = rSum;
				rBox.grow(bounds[sahStep - 1 - i]);
				rArea[sahStep - 2 - i] = rBox.area();
			}
			step = (bmax - bmin) / sahStep;
			for (int i{}; i < sahStep - 1; ++i) {
				float planeCost{ lCnt[i] * lArea[i] + rCnt[i] * rArea[i] };
				if (planeCost < bestCost) {
					axis = a;
					splitPos = bmin + (i + 1) * step;
					bestCost = planeCost;
				}
			}
		}
		return bestCost;
	}

	void subdivide(INT nodeId) {
		// terminate recursion
		BVHNode& node{ nodes[nodeId] };

		// determine split axis and position
		int axis{};
		float splitPos{};
		float cost{};

		if (!isStepSAH) {
			cost = splitSAH(node, axis, splitPos);
		} else if (!isBinsSAH) {
			cost = splitFixedStepSAH(node, axis, splitPos);
		} else {
			cost = splitBinnedSAH(node, axis, splitPos);
		}

		if (cost >= node.bb.area() * node.leftCntPar.y) {
			return;
		}

		// in-place partition
		INT i{ node.leftCntPar.x };
		INT j{ i + node.leftCntPar.y - 1 };
		while (i <= j) {
			if (splitPos <= comp(tris[triIds[i++].x].ctr, axis))
				std::swap(triIds[--i].x, triIds[j--].x);
		}

		// abort split if one of the sides is empty
		INT leftCnt{ i - node.leftCntPar.x };
		if (leftCnt == 0 || leftCnt == node.leftCntPar.y) {
			return;
		}

		// create child nodes
		int leftIdx{ nodesUsed++ };
		nodes[leftIdx].leftCntPar = {
			node.leftCntPar.x, leftCnt, nodeId, 0
		};

		/*nodes.push_back(BVHNode{
			.leftCntPar{ node.leftCntPar.y, leftCnt, nodeId, 0 }
		});*/

		updateNodeBounds(leftIdx);

		int rightIdx{ nodesUsed++ };
		nodes[rightIdx].leftCntPar = {
			i, node.leftCntPar.y - leftCnt, nodeId, 0
		};
		updateNodeBounds(rightIdx);

		node.leftCntPar = {
			leftIdx, 0, node.leftCntPar.z, 0
		};

		// recurse
		subdivide(leftIdx);
		subdivide(rightIdx);
	}
};