#include "BVH.h"

#include <algorithm>

void BVH::init(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix) {
	m_primsCnt = idsCnt;

	m_nodesUsed = 1;
	m_leafs = 0;
	m_depthMin = 2 * m_primsCnt - 1;
	m_depthMax = -1;

	m_prims.resize(m_primsCnt);
	m_primIds.resize(m_primsCnt);
	m_nodes.resize(2 * m_primsCnt - 1);

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

		m_primIds[i] = { i, 0, 0, 0 };
	}
}

void BVH::build() {
	BVHNode& root = m_nodes[0];
	root.leftCntPar = {
		0, m_primsCnt, -1, 0
	};
	updateNodeBounds(0);
	//mortonSort();
	buildStochastic();
	subdivide(0);
}

void BVH::initStochastic(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix) {
	m_primsCnt = idsCnt;

	m_nodesUsed = 1;
	m_leafs = 0;
	m_depthMin = 2 * m_primsCnt - 1;
	m_depthMax = -1;

	m_prims.resize(m_primsCnt);
	m_primIds.resize(m_primsCnt);
	m_nodes.resize(2 * m_primsCnt - 1);

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

		m_primIds[i] = { i, 0, 0, 0 };
	}
}

void BVH::buildStochastic() {
	mortonSort();

	// Primitive weights & CDF
	std::vector<float> cdf(m_primsCnt);
	float sum{};
	for (int i{}; i < m_primsCnt; ++i) {
		sum += (cdf[i] = m_prims[i].bb.area());
	}

	// algorithm 1
	{
		const float BIN_BASE{ std::sqrtf(2.f) };
		const int BIN_OFFSET{ 32 };
		const int BIN_COUNT{ 64 };

		int bin_counts[BIN_COUNT]{};

		for (int i{}; i < m_primsCnt; ++i) {
			float w = cdf[i];
			if (w == 0) continue;
			size_t bin;
			if (w == std::numeric_limits<float>::infinity())
				bin = BIN_COUNT - 1;
			else {
				bin = std::min<float>(BIN_COUNT - 1.f, std::max<float>(0.f,
					BIN_OFFSET + std::floor(std::log(w) / std::log(BIN_BASE))
				));
			}
			++bin_counts[bin];
		}

		float s{ 1.f / (m_primsCnt * m_carcassPart) };
		
		// compensate uniformity
		s = (s - m_carcassUniform / m_primsCnt) / (1 - m_carcassUniform);

		float uSum{};
		float cSum{ static_cast<float>(m_primsCnt) };
		float clamp;

		for (int i{}; i < BIN_COUNT; ++i) {
			if (i == BIN_COUNT - 1) {
				clamp = std::numeric_limits<float>::infinity();
				break;
			}
			clamp = pow(BIN_BASE, i - BIN_OFFSET + 1);
			if (clamp / (uSum + clamp * cSum) >= s) break;
			uSum += clamp * bin_counts[i];
			cSum -= bin_counts[i];
		}

		sum = 0.f;
		for (int i{}; i < m_primsCnt; ++i) {
			sum += cdf[i] = std::min(cdf[i], clamp);
		}
	}

	float pfix{};
	for (int i{}; i < m_primsCnt; ++i) {
		// uniformity
		//pfix += cdf[i] * (1.f - m_carcassUniform) + sum * m_carcassUniform / m_primsCnt;
		pfix += cdf[i] - m_carcassUniform * (cdf[i] + sum / m_primsCnt);
		cdf[i] = pfix;
	}
	sum = pfix;

	// subset
	int subsetSize{ static_cast<int>(std::round(m_primsCnt * m_carcassPart)) };
	std::vector<bool> subsetSelect(m_primsCnt);
	std::fill(subsetSelect.begin(), subsetSelect.end(), false);

	m_carcass.resize(m_primsCnt);
	std::fill(m_carcass.begin(), m_carcass.end(), XMINT4{ 0, 0, 0, 0 });

	for (int i{ 1 }; i < subsetSize; ++i) {
		float rnd{ static_cast<float>(i) / subsetSize };

		auto it = std::upper_bound(cdf.begin(), cdf.end() - 1, rnd * sum);
		auto j = it - cdf.begin();
		subsetSelect[j] = true; 
		
		m_carcass[j].x = 1;
		m_primIds[j].y = 1;
	}

	m_carcass[0] = m_carcass[0];
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
		Primitive& leafTri = m_prims[m_primIds[node.leftCntPar.x + i].x];

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
		Primitive& t = m_prims[m_primIds[node.leftCntPar.x + i].x];

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
			Primitive& t = m_prims[m_primIds[node.leftCntPar.x + i].x];
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
			Primitive& t = m_prims[m_primIds[node.leftCntPar.x + i].x];
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
	// terminate recursion
	BVHNode& node{ m_nodes[nodeId] };

	// determine split axis and position
	int axis{};
	float splitPos{};
	float cost{};

	switch (m_alg) {
	case 0:
		if (node.leftCntPar.y <= m_primsPerLeaf) {
			++m_leafs;
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
		++m_leafs;
		updateDepths(nodeId);
		return;
	}

	// in-place partition
	INT i{ node.leftCntPar.x };
	INT j{ i + node.leftCntPar.y - 1 };
	while (i <= j) {
		if (splitPos <= comp(m_prims[m_primIds[i++].x].ctr, axis))
			std::swap(m_primIds[--i].x, m_primIds[j--].x);
	}

	// abort split if one of the sides is empty
	INT leftCnt{ i - node.leftCntPar.x };
	if (leftCnt == 0 || leftCnt == node.leftCntPar.y) {
		++m_leafs;
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
