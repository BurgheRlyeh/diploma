#include "BVH.h"

#include <algorithm>

void BVH::init(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix) {
	m_primsCnt = idsCnt;

	m_nodesUsed = 1;
	m_leafsCnt = 0;
	m_depthMin = 2 * m_primsCnt - 1;
	m_depthMax = -1;

	m_prims.resize(m_primsCnt);
	m_primFrm.resize(m_primsCnt);
	m_stoh.resize(m_primsCnt);
	m_nodes.resize(2 * m_primsCnt - 1);
	//m_internals.resize(m_primsCnt - 1);
	//m_leafs.resize(m_primsCnt);

	for (INT i{}; i < m_primsCnt; ++i) {
		m_prims[i] = {
			Vector4::Transform(vts[ids[i].x], modelMatrix),
			Vector4::Transform(vts[ids[i].y], modelMatrix),
			Vector4::Transform(vts[ids[i].z], modelMatrix)
		};
		m_prims[i].ctr = (m_prims[i].v0 + m_prims[i].v1 + m_prims[i].v2) / 3.f;
		m_prims[i].bb.grow(m_prims[i].v0);
		m_prims[i].bb.grow(m_prims[i].v1);
		m_prims[i].bb.grow(m_prims[i].v2);

		m_primFrm[i] = { i, 0, 0, 0 };
		m_stoh[i] = { i, 0, 0, 0 };
	}
}

void BVH::build() {
	//BVHNode& root = m_nodes[0];
	//root.leftCntPar = {
	//	0, m_primsCnt, -1, 0
	//};
	//updateNodeBounds(0);
	//mortonSort();
	buildStochastic();
	//subdivide(0);
}

void BVH::buildStochastic() {
	mortonSort();

	// init weights
	std::vector<float> cdf(m_primsCnt);
	float sum{};
	float wmin{ std::numeric_limits<float>::max() };
	float wmax{ std::numeric_limits<float>::min() };
	for (int i{}; i < m_primsCnt; ++i) {
		sum += (cdf[i] = m_prims[m_mortonPrims[i].primId].bb.area());
		wmin = std::min<float>(wmin, cdf[i]);
		wmax = std::max<float>(wmax, cdf[i]);
	}

	// algorithm 1: histogram weight clamping
	const float BASE{ 1.1f };	// default = sqrtf(2.f)
	const int OFFSET{ 48 };		// default = 32
	const int BIN_CNT{ 64 };	// default = 64

	int bin_cnts[BIN_CNT]{};

	// histogram building
	for (int i{}; i < m_primsCnt; ++i) {
		++bin_cnts[static_cast<size_t>(std::min<float>(
			std::max<float>(
				OFFSET + std::floor(std::log(cdf[i]) / std::log(BASE)),
				0.f
			),
			BIN_CNT - 1.f
		))];
	}


	// primitive probability, compensate uniformity
	float s{ 1.f / (m_primsCnt * m_frmPart) };
	s = (s - m_uniform / m_primsCnt) / (1 - m_uniform);

	// unclamped & clamped sums, clamp
	float uSum{}, cSum{ static_cast<float>(m_primsCnt) };
	float clamp{ std::numeric_limits<float>::infinity() };

	// selection of clamp
	for (int i{}; i < BIN_CNT - 1; ++i) {
		float c{ powf(BASE, i - OFFSET + 1) };
		if (c / (uSum + clamp * cSum) >= s) {
			clamp = c;
			break;
		}
		uSum += clamp * bin_cnts[i];
		cSum -= bin_cnts[i];
	}

	// reweighting if found
	if (clamp != std::numeric_limits<float>::infinity()) {
		sum = 0.f;
		for (int i{}; i < m_primsCnt; ++i)
			sum += cdf[i] = std::min(cdf[i], clamp);
	}

	// reweighting with uniform dist
	cdf[0] = cdf[0] * (1.f - m_uniform) + sum * m_uniform / m_primsCnt;
	for (int i{ 1 }; i < m_primsCnt; ++i) {
		cdf[i] = cdf[i - 1] + cdf[i] * (1.f - m_uniform) + sum * m_uniform / m_primsCnt;
	}
	sum = cdf[m_primsCnt - 1];

	// selecting for carcass
	int frmSize{ static_cast<int>(std::round(m_primsCnt * m_frmPart)) };
	for (int i{}; i < frmSize - 1; ++i) {
		float prob{ 1.f * (1.f * i) / frmSize };
		float probCdf{ prob * sum };

		auto first = std::upper_bound(cdf.begin(), cdf.end() - 1, prob * sum);
		m_primFrm[m_mortonPrims[first - cdf.begin()].primId].y = 1;

		//std::swap(m_primFrm[i], m_primFrm[m_mortonPrims[first - cdf.begin()].primId]);
	}

	//bool -> int
	//isInFrame -> nearest frame's prim
	for (int i{}, c{}; i < m_primsCnt; ++i) {
		if (m_primFrm[i].y) {
			std::swap(m_primFrm[i], m_primFrm[c]);
			++c;
			m_primFrm[c - 1].y = c;
		}
		m_primFrm[i].y = c;
	}

	// build frame
	BVHNode& root = m_nodes[0];
	root.leftCntPar = { 0, frmSize, -1, 0 }; // frmSize / m_primsCnt
	updateNodeBounds(0);
	subdivide(0);

	m_nodes[0] = m_nodes[0];

	std::vector<int> shifts;
	shifts.resize(frmSize);

	for (int i{ frmSize }; i < m_primsCnt; ++i) {
		int leaf{ findBestLeaf(m_primFrm[i].x) };
		++shifts[leaf];
	}

	std::vector<XMINT4> primFrmNew;
	primFrmNew.resize(m_primsCnt);

	for (int i{}, shift{ shifts[0] }; i < frmSize; ++i) {
		m_nodes[m_primFrm[i].z].leftCntPar.x += shift;
		m_nodes[m_primFrm[i].z].leftCntPar.y += shifts[i];

		primFrmNew[i + shift].x = m_primFrm[i].x;
		for (int j{}; j < shifts[i]; ++j) {
			primFrmNew[i + shift + j + 1].x = m_primFrm[frmSize + j].x;
		}

		shift += shifts[i];
	}


	//for (int i{ frmSize }; i < m_primsCnt; ++i) {
	//	int leaf = findBestLeaf(m_primFrm[i].x);
	//	// swapping prims?
	//	auto temp = m_primFrm[i];
	//	int shift_begin{ m_nodes[leaf].leftCntPar.x + m_nodes[leaf].leftCntPar.y };
	//	std::shift_right(
	//		m_primFrm.begin() + shift_begin,
	//		m_primFrm.begin() + i,
	//		1
	//	);
	//	m_primFrm[shift_begin] = temp;
	//}
}

float BVH::primInsertMetric(int primId, int nodeId) {
	Primitive* prim = &m_prims[primId];
	BVHNode* node = &m_nodes[nodeId];

	AABB nodeBounds{ node->bb };
	float oldSA{ nodeBounds.area() };
	float newSA{ AABB::bbUnion(nodeBounds, prim->bb).area() };
	float cost{ newSA * (node->leftCntPar.y + 1) - oldSA * node->leftCntPar.y };

	while (node->leftCntPar.y != -1) {
		nodeBounds = node->bb;
		oldSA = nodeBounds.area();
		newSA = AABB::bbUnion(nodeBounds, prim->bb).area();
		cost += newSA - oldSA;
		node = &m_nodes[node->leftCntPar.y];
	}

	return cost;
}

int BVH::findBestLeaf(int primId) {
	float mincost = std::numeric_limits<float>::max();
	int best{ -1 };

	// brute-force
	for (int i{}; i < m_nodesUsed; ++i) {
		if (m_nodes[i].leftCntPar.y) {
			float currmin = primInsertMetric(primId, i);
			if (currmin < mincost) {
				mincost = currmin;
				best = i;
			}
		}
	}

	// morton window
	//const int MORTON_SEARCH_WINDOW{ 10 };
	//auto frameNearest = m_primFrm[primId].y;
	//auto b = std::max(frameNearest - MORTON_SEARCH_WINDOW, 0);
	//auto e = std::min(m_primsCnt, frameNearest + MORTON_SEARCH_WINDOW);
	//for (int i{ b }; i < e; ++i) {
	//	// 
	//}

	return best;
}

void BVH::mortonSort() {
	// AABB of all primitives centroids
	AABB aabb{};
	for (const Primitive& tr : m_prims) {
		aabb.grow(tr.ctr);
	}

	// compute morton indices of primitives
	m_mortonPrims.resize(m_primsCnt);
	for (int i{}; i < m_primsCnt; ++i) {
		m_mortonPrims[i].primId = i;

		int mortonScale{ 1 << 10 };
		Vector4 relateCtr{ aabb.relateVecPos(m_prims[i].ctr) };
		m_mortonPrims[i].mortonCode = encodeMorton(mortonScale * relateCtr);
	}

	// sort primitives
	std::sort(
		m_mortonPrims.begin(),
		m_mortonPrims.end(),
		[&](const MortonPrim& mp1, const MortonPrim& mp2) {
			return mp1.mortonCode < mp2.mortonCode;
		}
	);

	m_mortonPrims[0] = m_mortonPrims[0];
}

UINT BVH::mortonShift(UINT x) {
	x = (x | (x << 16)) & 0b00000011000000000000000011111111;
	x = (x | (x <<  8)) & 0b00000011000000001111000000001111;
	x = (x | (x <<  4)) & 0b00000011000011000011000011000011;
	x = (x | (x <<  2)) & 0b00001001001001001001001001001001;
	return x;
}

UINT BVH::encodeMorton(const Vector4& v) {
	return (mortonShift(v.z) << 2) | (mortonShift(v.y) << 1) | mortonShift(v.x);
}


void BVH::updateDepths(INT id) {
	int d{};
	for (; id != 0; id = m_nodes[id].leftCntPar.z) ++d;
	m_depthMin = std::min(m_depthMin, d);
	m_depthMax = std::max(m_depthMax, d);
}

float BVH::comp(Vector4 v, INT idx) {
	switch (idx) {
	case 0: return v.x;
	case 1: return v.y;
	case 2: return v.z;
	case 3: return v.w;
	default: throw;
	}
}

void BVH::updateNodeBounds(INT nodeIdx) {
	BVHNode& node = m_nodes[nodeIdx];
	node.bb = {};
	for (INT i{}; i < node.leftCntPar.y; ++i) {
		Primitive& leafTri = m_prims[m_primFrm[node.leftCntPar.x + i].x];

		node.bb.grow(leafTri.v0);
		node.bb.grow(leafTri.v1);
		node.bb.grow(leafTri.v2);
	}
}

void BVH::splitDichotomy(BVHNode& node, int& axis, float& splitPos) {
	Vector4 e{ node.bb.diagonal() };
	axis = static_cast<int>(e.x < e.y);
	axis += static_cast<int>(comp(e, axis) < e.z);
	splitPos = comp(node.bb.bmin + e / 2.f, axis);
}

float BVH::evaluateSAH(BVHNode& node, int axis, float pos) {
	AABB leftBox{}, rightBox{};
	int leftCnt{}, rightCnt{};
	for (int i{}; i < node.leftCntPar.y; ++i) {
		Primitive& t = m_prims[m_primFrm[node.leftCntPar.x + i].x];

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

float BVH::splitSAH(BVHNode& node, int& axis, float& splitPos) {
	float bestCost{ std::numeric_limits<float>::max() };
	for (int a{}; a < 3; ++a) {
		for (int i{}; i < node.leftCntPar.y; ++i) {
			Primitive& t = m_prims[m_primFrm[node.leftCntPar.x + i].x];
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

float BVH::splitFixedStepSAH(BVHNode& node, int& axis, float& pos) {
	float bestCost{ std::numeric_limits<float>::max() };
	for (int a{}; a < 3; ++a) {
		float bmin{ comp(node.bb.bmin, a) };
		float bmax{ comp(node.bb.bmax, a) };

		if (bmin == bmax)
			continue;

		float step{ (bmax - bmin) / m_sahSteps };
		for (int i{ 1 }; i < m_sahSteps; ++i) {
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

float BVH::splitBinnedSAH(BVHNode& node, int& axis, float& splitPos) {
	float bestCost{ std::numeric_limits<float>::max() };
	for (int a{}; a < 3; ++a) {
		float bmin{ comp(node.bb.bmin, a) };
		float bmax{ comp(node.bb.bmax, a) };
		if (bmin == bmax)
			continue;

		AABB bounds[MaxSteps]{};
		int m_primsCnt[MaxSteps]{};

		float step = m_sahSteps / (bmax - bmin);
		for (int i{}; i < node.leftCntPar.y; ++i) {
			Primitive& t = m_prims[m_primFrm[node.leftCntPar.x + i].x];
			int id{ std::min(
				m_sahSteps - 1,
				static_cast<int>((comp(t.ctr, a) - bmin) * step)
			) };
			++m_primsCnt[id];
			bounds[id].grow(t.v0);
			bounds[id].grow(t.v1);
			bounds[id].grow(t.v2);
		}

		float lArea[MaxSteps - 1]{}, rArea[MaxSteps - 1]{};
		int lCnt[MaxSteps - 1]{}, rCnt[MaxSteps - 1]{};
		AABB lBox{}, rBox{};
		int lSum{}, rSum{};

		for (int i{}; i < m_sahSteps - 1; ++i) {
			lSum += m_primsCnt[i];
			lCnt[i] = lSum;
			lBox.grow(bounds[i]);
			lArea[i] = lBox.area();

			rSum += m_primsCnt[m_sahSteps - 1 - i];
			rCnt[m_sahSteps - 2 - i] = rSum;
			rBox.grow(bounds[m_sahSteps - 1 - i]);
			rArea[m_sahSteps - 2 - i] = rBox.area();
		}
		step = (bmax - bmin) / m_sahSteps;
		for (int i{}; i < m_sahSteps - 1; ++i) {
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

void BVH::subdivide(INT nodeId) {
	BVHNode& node{ m_nodes[nodeId] };

	// determine split axis and position
	int axis{};
	float splitPos{};
	float cost{};

	switch (m_alg) {
	case 0:
		if (node.leftCntPar.y <= m_primsPerLeaf) {
			++m_leafsCnt;
			updateDepths(nodeId);
			return;
		}
		splitDichotomy(node, axis, splitPos);
		break;
	case 1:
		cost = splitSAH(node, axis, splitPos);
		break;
	case 2:
		cost = splitFixedStepSAH(node, axis, splitPos);
		break;
	case 3:
		cost = splitBinnedSAH(node, axis, splitPos);
		break;
	}

	if (m_alg != 0 && cost >= node.bb.area() * node.leftCntPar.y) {
		++m_leafsCnt;
		updateDepths(nodeId);
		return;
	}

	// in-place partition
	INT i{ node.leftCntPar.x };
	INT j{ i + node.leftCntPar.y - 1 };
	while (i <= j) {
		if (splitPos <= comp(m_prims[m_primFrm[i++].x].ctr, axis))
			std::swap(m_primFrm[--i].x, m_primFrm[j--].x);
	}

	// abort split if one of the sides is empty
	INT leftCnt{ i - node.leftCntPar.x };
	if (leftCnt == 0 || leftCnt == node.leftCntPar.y) {
		++m_leafsCnt;
		updateDepths(nodeId);
		return;
	}

	// create child nodes
	int leftIdx{ m_nodesUsed++ };
	m_nodes[leftIdx].leftCntPar = {
		node.leftCntPar.x, leftCnt, nodeId, 0
	};

	/*nodes.push_back(BVHNode{
	.leftCntPar{ node.leftCntPar.y, leftCnt, nodeId, 0 }
	});*/

	updateNodeBounds(leftIdx);

	int rightIdx{ m_nodesUsed++ };
	m_nodes[rightIdx].leftCntPar = {
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
