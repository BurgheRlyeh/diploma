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

	ID3D11VertexShader* m_pVertexShader{};
	ID3D11PixelShader* m_pPixelShader{};
	ID3D11InputLayout* m_pInputLayout{};

	bool isUpd{ true };

	// logical part
	struct Primitive {
		Vector4 v0{}, v1{}, v2{};
		Vector4 ctr{};
		AABB bb{};
	};
	std::vector<Primitive> m_prims{};

	struct BVHNode {
		AABB bb{};
		XMINT4 leftCntPar{};
	};
	std::vector<BVHNode> m_nodes{};

	std::vector<XMUINT4> m_primMortonFrmLeaf{};
	std::vector<XMUINT4> m_frame{};
	std::vector<XMUINT4>::iterator m_edge{};

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
	INT m_algBuild{ 4 };
	INT m_primsPerLeaf{ 2 };
	INT m_sahSteps{ 8 };
	// 0 - bruteforce
	// 1 - morton
	// 2 - bvh prims
	// 3 - bvh prims +
	// 4 - bvh tree
	// 5 - bvh nodes
	int m_algInsert{ 0 };

	float m_frmPart{ 0.1f };
	float m_uniform{ 0.f };
	int m_insertSearchWindow{ 0 };

	int m_frmSize{};

	bool m_aabbHighlightAll{};
	int m_aabbHighlightNode{};
	bool m_aabbHighlightSubtree{};
	int m_aabbHighlightSubtreeDepth{};
	bool m_aabbHighlightOne{};
	bool m_aabbHighlightParent{};
	bool m_aabbHighlightSibling{};
	bool m_aabbHighlightChildren{};

	bool m_aabbHighlightPrims{};

	bool m_highlightFramePrims{};
	int m_highlightPrim{};

public:
	BVH() = delete;
	BVH(ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext):
		m_pDevice(pDevice), m_pDeviceContext(pDeviceContext) {
		HRESULT hr{ S_OK };

		// vertex buffer
		{
			Vector3 vertices[8]{
				{ 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f },
				{ 1.f, 0.f, 0.f }, { 1.f, 1.f, 0.f },
				{ 0.f, 0.f, 1.f }, { 0.f, 1.f, 1.f },
				{ 1.f, 0.f, 1.f }, { 1.f, 1.f, 1.f }
			};

			D3D11_BUFFER_DESC desc{
				.ByteWidth{ sizeof(Vector3) * 8 },
				.Usage{ D3D11_USAGE_IMMUTABLE },
				.BindFlags{ D3D11_BIND_VERTEX_BUFFER }
			};

			D3D11_SUBRESOURCE_DATA data{
				.pSysMem{ vertices },
				.SysMemPitch{ sizeof(Vector3) * 8 }
			};

			hr = m_pDevice->CreateBuffer(&desc, &data, &m_pVertexBuffer);
			THROW_IF_FAILED(hr);

			hr = setResourceName(m_pVertexBuffer, "BVHVerterBuffer");
			THROW_IF_FAILED(hr);
		}

		// index buffer
		{
			UINT16 indices[24]{
				0, 1, 0, 2, 1, 3, 2, 3,
				0, 4, 1, 5, 2, 6, 3, 7,
				4, 5, 4, 6, 5, 7, 6, 7
			};

			D3D11_BUFFER_DESC desc{
				.ByteWidth{ sizeof(UINT16) * 24 },
				.Usage{ D3D11_USAGE_IMMUTABLE },
				.BindFlags{ D3D11_BIND_INDEX_BUFFER }
			};

			D3D11_SUBRESOURCE_DATA data{
				.pSysMem{ indices },
				.SysMemPitch{ sizeof(UINT16) * 24 }
			};

			hr = m_pDevice->CreateBuffer(&desc, &data, &m_pIndexBuffer);
			THROW_IF_FAILED(hr);

			hr = setResourceName(m_pIndexBuffer, "BVHIndexBuffer");
			THROW_IF_FAILED(hr);
		}

		// shader processing
		ID3DBlob* pBlobVS{};
		{
			std::wstring filepath{ L"BVHVS.cso" };
			hr = D3DReadFileToBlob(filepath.c_str(), &pBlobVS);
			THROW_IF_FAILED(hr);

			hr = m_pDevice->CreateVertexShader(
				pBlobVS->GetBufferPointer(),
				pBlobVS->GetBufferSize(),
				nullptr,
				&m_pVertexShader
			);
			THROW_IF_FAILED(hr);

			ID3DBlob* pBlobPS{};
			filepath = L"BVHPS.cso";
			hr = D3DReadFileToBlob(filepath.c_str(), &pBlobPS);
			THROW_IF_FAILED(hr);

			hr = m_pDevice->CreatePixelShader(
				pBlobPS->GetBufferPointer(),
				pBlobPS->GetBufferSize(),
				nullptr,
				&m_pPixelShader
			);
			THROW_IF_FAILED(hr);
		}

		// create input layout
		{
			D3D11_INPUT_ELEMENT_DESC inputDesc[]{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};

			hr = m_pDevice->CreateInputLayout(
				inputDesc,
				1,
				pBlobVS->GetBufferPointer(),
				pBlobVS->GetBufferSize(),
				&m_pInputLayout
			);
			THROW_IF_FAILED(hr);

			hr = setResourceName(m_pInputLayout, "BVHInputLayout");
			THROW_IF_FAILED(hr);
		}

		// bvh structured buffer
		{
			D3D11_BUFFER_DESC desc{
				.ByteWidth{ (2 * LIMIT_I + 1) * sizeof(BVH::BVHNode) },
				.Usage{ D3D11_USAGE_DEFAULT },
				.BindFlags{ D3D11_BIND_SHADER_RESOURCE },
				.CPUAccessFlags{ D3D11_CPU_ACCESS_WRITE },
				.MiscFlags{ D3D11_RESOURCE_MISC_BUFFER_STRUCTURED },
				.StructureByteStride{ sizeof(BVH::BVHNode) }
			};

			hr = m_pDevice->CreateBuffer(&desc, nullptr, &m_pBVHBuffer);
			THROW_IF_FAILED(hr);

			hr = setResourceName(m_pBVHBuffer, "BVHBuffer");
			THROW_IF_FAILED(hr);

			D3D11_SHADER_RESOURCE_VIEW_DESC descSRV{
				.Format{ DXGI_FORMAT_UNKNOWN },
				.ViewDimension{ D3D11_SRV_DIMENSION_BUFFER },
				.Buffer{
					.FirstElement{ 0 },
					.NumElements{ 2 * LIMIT_I + 1 }
				}
			};

			hr = m_pDevice->CreateShaderResourceView(m_pBVHBuffer, &descSRV, &m_pBVHBufferSRV);
			THROW_IF_FAILED(hr);

			hr = setResourceName(m_pBVHBufferSRV, "BVHBufferSRV");
			THROW_IF_FAILED(hr);
		}

		// create prim indices buffer
		{
			D3D11_BUFFER_DESC desc{
				.ByteWidth{ LIMIT_I * sizeof(XMINT4) },
				.Usage{ D3D11_USAGE_DEFAULT },
				.BindFlags{ D3D11_BIND_CONSTANT_BUFFER }
			};

			hr = m_pDevice->CreateBuffer(&desc, nullptr, &m_pPrimIdsBuffer);
			THROW_IF_FAILED(hr);

			hr = setResourceName(m_pPrimIdsBuffer, "TriIdsBuffer");
			THROW_IF_FAILED(hr);
		}

		// create model buffer
		{
			D3D11_BUFFER_DESC desc{
				.ByteWidth{ sizeof(ModelBuffer) * (2 * LIMIT_I - 1) },
				.Usage{ D3D11_USAGE_DEFAULT },
				.BindFlags{ D3D11_BIND_SHADER_RESOURCE },
				.CPUAccessFlags{ D3D11_CPU_ACCESS_WRITE },
				.MiscFlags{ D3D11_RESOURCE_MISC_BUFFER_STRUCTURED },
				.StructureByteStride{ sizeof(ModelBuffer) }
			};
			hr = m_pDevice->CreateBuffer(&desc, nullptr, &m_pModelBuffer);
			THROW_IF_FAILED(hr);

			hr = setResourceName(m_pModelBuffer, "BVHRendererModelBuffer");
			THROW_IF_FAILED(hr);

			D3D11_SHADER_RESOURCE_VIEW_DESC descSRV{
				.Format{ DXGI_FORMAT_UNKNOWN },
				.ViewDimension{ D3D11_SRV_DIMENSION_BUFFER },
				.Buffer{.NumElements{ 2 * LIMIT_I - 1 } }
			};
			hr = m_pDevice->CreateShaderResourceView(m_pModelBuffer, &descSRV, &m_pModelBufferSRV);
			THROW_IF_FAILED(hr);

			hr = setResourceName(m_pModelBufferSRV, "BVHRendererModelBufferSRV");
			THROW_IF_FAILED(hr);
		}
	}

	void init(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix);
	void term();

	void render(ID3D11SamplerState* pSampler, ID3D11Buffer* pSceneBuffer);

	void build();
	void updateBuffers();

	void updateRenderBVH();

	void buildStochastic();
	float costSAH();

	int depth(int id) {
		int d{};
		for (; id; id = m_nodes[id].leftCntPar.z) ++d;
		return d;
	}

	void renderBVHImGui() {
		ImGui::Begin("BVH");

		ImGui::Text("Split algorithm:");

		bool isDichotomy{ m_algBuild == 0 };
		ImGui::Checkbox("Dichotomy", &isDichotomy);
		if (isDichotomy) {
			m_algBuild = 0;
			ImGui::DragInt("Primitives per leaf", &m_primsPerLeaf, 1, 2, 32);
		}

		bool isSAH{ m_algBuild == 1 };
		ImGui::Checkbox("SAH", &isSAH);
		if (isSAH) m_algBuild = 1;

		bool isFixedStepSAH{ m_algBuild == 2 };
		ImGui::Checkbox("FixedStepSAH", &isFixedStepSAH);
		if (isFixedStepSAH) {
			m_algBuild = 2;
			ImGui::DragInt("SAH step", &m_sahSteps, 1, 2, 32);
		}

		bool isBinnedSAH{ m_algBuild == 3 };
		ImGui::Checkbox("BinnedSAH", &isBinnedSAH);
		if (isBinnedSAH) {
			m_algBuild = 3;
			ImGui::DragInt("SAH step", &m_sahSteps, 1, 2, 32);
		}

		bool isStochastic{ m_algBuild == 4 };
		ImGui::Checkbox("Stochastic", &isStochastic);
		if (isStochastic) {
			m_algBuild = 4;

			ImGui::DragInt("SAH step", &m_sahSteps, 1, 2, 32);

			float carcassPart{ 100.f * m_frmPart };
			ImGui::DragFloat("Part for carcass", &carcassPart, 1.f, 1.f, 100.f);
			m_frmPart = carcassPart / 100.f;

			float carcassUniform{ 100.f * m_uniform };
			ImGui::DragFloat("Carcass unifrom", &carcassUniform, 1.f, 0.f, 100.f);
			m_uniform = carcassUniform / 100.f;

			ImGui::Text("Insertion prims algorithm:");

			bool isInsertBruteforce{ m_algInsert == 0 };
			ImGui::Checkbox("Bruteforce", &isInsertBruteforce);
			if (isInsertBruteforce) m_algInsert = 0;

			bool isInsertMorton{ m_algInsert == 1 };
			ImGui::Checkbox("Morton", &isInsertMorton);
			if (isInsertMorton) m_algInsert = 1;

			bool isInsertBVHPrims{ m_algInsert == 2 };
			ImGui::Checkbox("BVH prims", &isInsertBVHPrims);
			if (isInsertBVHPrims) m_algInsert = 2;

			bool isInsertBVHPrimsPlus{ m_algInsert == 3 };
			ImGui::Checkbox("BVH prims+", &isInsertBVHPrimsPlus);
			if (isInsertBVHPrimsPlus) m_algInsert = 3;

			bool isInsertBVHTree{ m_algInsert == 4 };
			ImGui::Checkbox("BVH tree", &isInsertBVHTree);
			if (isInsertBVHTree) m_algInsert = 4;

			bool isInsertBVHNodes{ m_algInsert == 5 };
			ImGui::Checkbox("BVH nodes", &isInsertBVHNodes);
			if (isInsertBVHNodes) m_algInsert = 5;

			if (m_algInsert) {
				ImGui::DragInt(
					"Insert search window",
					&m_insertSearchWindow, 1, 0,
					static_cast<int>(std::round(m_primsCnt * m_frmPart))
				);
			}
		}

		ImGui::Text(" ");

		ImGui::Text("Statistics:");
		ImGui::Text(" ");
		ImGui::Text("SAH cost: %.3f", costSAH());
		//ImGui::Text(" ");
		//ImGui::Text("Last BVH construction time (ms): %.3f", m_geomCPUAvgTime);
		ImGui::Text(" ");
		ImGui::Text("Nodes: %d", m_nodesUsed);
		ImGui::Text("Leafs: %d", m_leafsCnt);
		ImGui::Text(" ");
		ImGui::Text("Primitives: %d", m_primsCnt);
		ImGui::Text("Avg primitives per leaf: %.3f", 1.f * m_primsCnt / m_leafsCnt);
		ImGui::Text(" ");
		ImGui::Text("Min depth: %d", m_depthMin);
		ImGui::Text("Max depth: %d", m_depthMax);

		ImGui::End();

		//ImGui::Begin("Highlights");

		//bool highlightFramePrims{ m_highlightFramePrims };
		//ImGui::Checkbox("Highlight Frame Prims", &highlightFramePrims);
		//if (highlightFramePrims != m_highlightFramePrims) {

		//}

		//ImGui::End();
	}

	void renderAABBsImGui() {
		ImGui::Begin("BVH's AABBs");

		ImGui::Text("Highlight:");

		ImGui::Checkbox("All", &m_aabbHighlightAll);
		if (m_aabbHighlightAll) {
			m_aabbHighlightSubtree = m_aabbHighlightOne = false;
		}

		ImGui::Checkbox("Subtree", &m_aabbHighlightSubtree);
		if (m_aabbHighlightSubtree) {
			m_aabbHighlightAll = m_aabbHighlightOne = false;

			ImGui::Text(" ");

			ImGui::Text("Node:");
			ImGui::SameLine();
			ImGui::DragInt("N", &m_aabbHighlightNode, 1, 0, m_nodesUsed);

			ImGui::Text("Depth:");
			ImGui::SameLine();
			ImGui::DragInt("D", &m_aabbHighlightSubtreeDepth, 1, 0, m_depthMax);

			ImGui::Checkbox("Primitives", &m_aabbHighlightPrims);

			ImGui::Text(" ");
		}

		ImGui::Checkbox("One", &m_aabbHighlightOne);
		if (m_aabbHighlightOne) {
			m_aabbHighlightAll = m_aabbHighlightSubtree = false;

			int nodeId{ m_aabbHighlightNode };
			BVHNode& node{ m_nodes[nodeId] };
			int parent{};
			if (nodeId) parent = node.leftCntPar.z;
			int sibling{ m_nodes[parent].leftCntPar.x };
			if (nodeId) sibling += (nodeId != m_nodes[parent].leftCntPar.x ? 0 : 1);
			int child1{ node.leftCntPar.x }, child2{ child1 + 1 };

			ImGui::Text(" ");

			ImGui::Text("Node:");
			ImGui::SameLine();
			ImGui::DragInt(" ", &m_aabbHighlightNode, 1, 0, m_nodesUsed);

			ImGui::Checkbox("Parent", &m_aabbHighlightParent);
			ImGui::SameLine();
			ImGui::Text("(%i)", parent);

			ImGui::Checkbox("Sibling", &m_aabbHighlightSibling);
			ImGui::SameLine();
			ImGui::Text("(%i)", sibling);

			ImGui::Checkbox("Children", &m_aabbHighlightChildren);
			ImGui::SameLine();
			ImGui::Text("(%i, %i)", child1, child2);
			
			ImGui::Checkbox("Primitives", &m_aabbHighlightPrims);
			if (m_nodes[m_aabbHighlightNode].leftCntPar.y) {
				ImGui::SameLine();
				ImGui::Text("(");
				int first{ m_nodes[m_aabbHighlightNode].leftCntPar.x };
				int cnt{ m_nodes[m_aabbHighlightNode].leftCntPar.y };
				for (int i{ first }; i < first + cnt; ++i) {
					ImGui::SameLine();
					ImGui::Text("%i", m_primMortonFrmLeaf[i].x);
					if (i + 1 < m_nodes[m_aabbHighlightNode].leftCntPar.y) {
						ImGui::SameLine();
						ImGui::Text(", ");
					}
				}
				ImGui::SameLine();
				ImGui::Text(")");
			}
		}

		ImGui::End();

		updateRenderBVH();
		updateBuffers();
	}

	ID3D11Buffer* getPrimIdsBuffer() {
		return m_pPrimIdsBuffer;
	}

	ID3D11ShaderResourceView* getBVHBufferSRV() {
		return m_pBVHBufferSRV;
	}

private:

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

	// stochastic
	void mortonSort();
	UINT mortonShift(UINT x);
	UINT encodeMorton(const Vector4& v);

	float primInsertMetric(int primId, int nodeId);

	int findBestLeafBruteforce(int primId);
	int findBestLeafMorton(int primId, int frmNearest);
	int findBestLeafBVHPrims(int primId, int frmNearest);
	int findBestLeafBVHPrimsPlus(int primId, int frmNearest);
	int findBestLeafBVHTree(int primId, int frmNearest);
	int findBestLeafBVHNodes(int primId, int frmNearest);

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