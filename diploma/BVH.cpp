#include "BVH.h"

inline void BVH::init(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix) {
	m_triCnt = idsCnt;

	m_nodesUsed = 1;
	m_leafs = 0;
	m_depthMin = 2 * m_triCnt - 1;
	m_depthMax = -1;

	m_tris.resize(m_triCnt);
	m_triIds.resize(m_triCnt);
	m_nodes.resize(2 * m_triCnt - 1);

	for (INT i{}; i < m_triCnt; ++i) {
		m_tris[i] = {
			Vector4::Transform(vts[ids[i].x], modelMatrix),
			Vector4::Transform(vts[ids[i].y], modelMatrix),
			Vector4::Transform(vts[ids[i].z], modelMatrix)
		};
		m_tris[i].ctr = (m_tris[i].v0 + m_tris[i].v1 + m_tris[i].v2) / 3.f;

		m_triIds[i] = { i, 0, 0, 0 };
	}
}
