#include "BVH.h"

#include <algorithm>
#include <queue>

// ---------------
//	GRAPHICS PART
// ---------------
BVH::BVH(ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext) :
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

void BVH::term() {
	SAFE_RELEASE(m_pPrimIdsBuffer);
	SAFE_RELEASE(m_pBVHBufferSRV);
	SAFE_RELEASE(m_pBVHBuffer);
	SAFE_RELEASE(m_pModelBuffer);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pPixelShader);
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pIndexBuffer);
	SAFE_RELEASE(m_pVertexBuffer);
}

void BVH::updateRenderBVH() {
	m_modelBuffers.clear();

	for (int i{}; i < m_primsCnt; ++i) {
		m_primMortonFrmLeaf[i].y = 0;
	}

	if (m_aabbHighlightAll) {
		preForEach(0, [&](int nodeId) {
			float d{ static_cast<float>(depth(nodeId)) };

			AABB bb{ m_nodes[nodeId].bb };
			Color cl{
				m_nodes[nodeId].leftCntPar.y ? 1.f : 0.f,
				1.f - d / m_depthMax,
				d / m_depthMax,
				0.f
			};

			m_modelBuffers.push_back({ bb, cl });
			});
	}
	else if (m_aabbHighlightSubtree) {
		int depthLim{ depth(m_aabbHighlightNode) };

		preForEach(m_aabbHighlightNode, [&](int nodeId) {
			float d{ 1.f * depth(nodeId) };
			if (d - depthLim > m_aabbHighlightSubtreeDepth)
				return;

			AABB bb{ m_nodes[nodeId].bb };
			Color cl{
				m_nodes[nodeId].leftCntPar.y ? 1.f : 0.f,
				1.f - d / depthLim,
				d / depthLim,
				0.f
			};

			m_modelBuffers.push_back({ bb, cl });

			if (m_aabbHighlightPrims && m_nodes[nodeId].leftCntPar.y) {
				for (int i{}; i < m_nodes[nodeId].leftCntPar.y; ++i) {
					UINT index{ m_primMortonFrmLeaf[m_nodes[nodeId].leftCntPar.x + i].x };
					m_primMortonFrmLeaf[index].y = 1;
				}
			}
			});
	}
	else if (m_aabbHighlightOne) {
		int nodeId{ m_aabbHighlightNode };
		BVHNode& node{ m_nodes[nodeId] };
		int parent{};
		if (nodeId) parent = node.leftCntPar.z;
		int sibling{ m_nodes[parent].leftCntPar.x };
		if (nodeId) sibling += (nodeId != m_nodes[parent].leftCntPar.x ? 0 : 1);
		int child1{ node.leftCntPar.x }, child2{ child1 + 1 };

		// selected
		float d{ static_cast<float>(depth(nodeId)) };
		AABB bb{ m_nodes[nodeId].bb };
		Color cl{
			m_nodes[nodeId].leftCntPar.y ? 1.f : 0.f,
			1.f - d / m_depthMax,
			d / m_depthMax,
			0.f
		};

		m_modelBuffers.push_back({ bb, cl });

		if (m_aabbHighlightPrims && m_nodes[nodeId].leftCntPar.y) {
			for (int i{}; i < m_nodes[nodeId].leftCntPar.y; ++i) {
				UINT index{ m_primMortonFrmLeaf[m_nodes[nodeId].leftCntPar.x + i].x };
				m_primMortonFrmLeaf[index].y = 1;
			}
		}

		if (m_aabbHighlightParent && nodeId) {
			d = depth(parent);
			bb = m_nodes[parent].bb;
			cl = {
				m_nodes[parent].leftCntPar.y ? 1.f : 0.f,
				1.f - d / m_depthMax,
				d / m_depthMax,
				0.f
			};

			m_modelBuffers.push_back({ bb, cl });
		}

		if (m_aabbHighlightSibling && nodeId) {
			d = depth(sibling);
			bb = m_nodes[sibling].bb;
			cl = {
				m_nodes[sibling].leftCntPar.y ? 1.f : 0.f,
				1.f - d / m_depthMax,
				d / m_depthMax,
				0.f
			};

			m_modelBuffers.push_back({ bb, cl });

			if (m_aabbHighlightPrims && m_nodes[sibling].leftCntPar.y) {
				for (int i{}; i < m_nodes[sibling].leftCntPar.y; ++i) {
					UINT index{ m_primMortonFrmLeaf[m_nodes[sibling].leftCntPar.x + i].x };
					m_primMortonFrmLeaf[index].y = 1;
				}
			}
		}

		if (m_aabbHighlightChildren && !node.leftCntPar.y) {
			for (int child{ child1 }; child <= child2; ++child) {
				d = depth(child);
				bb = m_nodes[child].bb;
				cl = {
					m_nodes[child].leftCntPar.y ? 1.f : 0.f,
					1.f - d / m_depthMax,
					d / m_depthMax,
					0.f
				};

				m_modelBuffers.push_back({ bb, cl });

				if (m_aabbHighlightPrims && m_nodes[child].leftCntPar.y) {
					for (int i{}; i < m_nodes[child].leftCntPar.y; ++i) {
						UINT index{ m_primMortonFrmLeaf[m_nodes[child].leftCntPar.x + i].x };
						m_primMortonFrmLeaf[index].y = 1;
					}
				}
			}
		}
	}
	else {
		m_modelBuffers.push_back({ {}, {} });
	}

	m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, m_modelBuffers.data(), 0, 0);
}

void BVH::updateBuffers() {
	m_pDeviceContext->UpdateSubresource(m_pBVHBuffer, 0, nullptr, m_nodes.data(), 0, 0);
	m_pDeviceContext->UpdateSubresource(m_pPrimIdsBuffer, 0, nullptr, m_primMortonFrmLeaf.data(), 0, 0);
}

void BVH::renderBVHImGui() {
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

void BVH::renderAABBsImGui() {
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


// ------------
//	LOGIC PART
// ------------
void BVH::init(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix) {
	m_primsCnt = idsCnt;

	m_nodesUsed = 1;
	m_leafsCnt = 0;
	m_depthMin = 2 * m_primsCnt;
	m_depthMax = -1;

	m_prims.resize(m_primsCnt);
	m_primMortonFrmLeaf.clear();
	m_primMortonFrmLeaf.resize(m_primsCnt);
	m_nodes.resize(2 * m_primsCnt - 1);

	for (UINT i{}; i < m_primsCnt; ++i) {
		m_prims[i] = {
			Vector4::Transform(vts[ids[i].x], modelMatrix),
			Vector4::Transform(vts[ids[i].y], modelMatrix),
			Vector4::Transform(vts[ids[i].z], modelMatrix)
		};
		m_prims[i].ctr = (m_prims[i].v0 + m_prims[i].v1 + m_prims[i].v2) / 3.f;
		m_prims[i].bb.grow(m_prims[i].v0);
		m_prims[i].bb.grow(m_prims[i].v1);
		m_prims[i].bb.grow(m_prims[i].v2);

		m_primMortonFrmLeaf[i] = { i, 0, 0, 0 };
	}
}

void BVH::render(ID3D11SamplerState* pSampler, ID3D11Buffer* pSceneBuffer) {
	ID3D11SamplerState* samplers[]{ pSampler };
	m_pDeviceContext->PSSetSamplers(0, 1, samplers);

	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	ID3D11Buffer* vertexBuffers[]{ m_pVertexBuffer };
	UINT strides[]{ 12 }, offsets[]{ 0 };
	m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	m_pDeviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);

	ID3D11Buffer* cbuffers[]{ pSceneBuffer };
	m_pDeviceContext->VSSetConstantBuffers(0, 1, cbuffers);

	// bind srv
	ID3D11ShaderResourceView* srvBuffers[]{ m_pModelBufferSRV };
	m_pDeviceContext->VSSetShaderResources(0, 1, srvBuffers);
	m_pDeviceContext->PSSetShaderResources(0, 1, srvBuffers);

	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
	m_pDeviceContext->DrawIndexedInstanced(24, m_modelBuffers.size(), 0, 0, 0);
}

void BVH::build(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix) {
	if (m_algBuild != 4) {
		init(vts, vtsCnt, ids, idsCnt, modelMatrix);
		m_nodes[0].leftCntPar = {
			0, m_primsCnt, -1, 0
		};
		updateNodeBounds(0);
		subdivide(0);
	}
	else {
		buildStochastic(vts, vtsCnt, ids, idsCnt, modelMatrix);
	}
}

void BVH::buildStochastic(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix) {
	init(vts, vtsCnt, ids, idsCnt, modelMatrix);
	mortonSort();

	// init weights
	std::vector<float> cdf(m_primsCnt);
	float sum{};
	float wmin{ std::numeric_limits<float>::max() };
	float wmax{ std::numeric_limits<float>::lowest() };

	auto it = m_primMortonFrmLeaf.begin();
	for (int i{}; i < m_primsCnt; ++i) {
		sum += (cdf[i] = m_prims[(*(it++)).x].bb.area());
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
		//sum += (cdf[i] = m_prims[(*(it++)).x].bb.area());
		//wmin = std::min<float>(wmin, cdf[i]);
		//wmax = std::max<float>(wmax, cdf[i]);

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

	// reweighting with uniform dist & calc cdf
	cdf[0] = cdf[0] * (1.f - m_uniform) + sum * m_uniform / m_primsCnt;
	for (int i{ 1 }; i < m_primsCnt; ++i) {
		cdf[i] = cdf[i - 1] + cdf[i] * (1.f - m_uniform) + sum * m_uniform / m_primsCnt;
	}
	sum = cdf[m_primsCnt - 1];

	// selecting for carcass
	int frmSize{}, frmExpSize{ static_cast<int>(std::round(m_primsCnt * m_frmPart)) };
	m_frame.clear();
	m_frame.resize(frmExpSize);
	float prob{ 1.f * (1.f * frmSize) / frmExpSize };
	it = m_edge = m_primMortonFrmLeaf.begin();
	for (int i{}; i < m_primsCnt; ++i) {
		if (frmSize == frmExpSize || cdf[i] <= prob * sum) {
			(*(it++)).z = frmSize - 1;
			continue;
		}

		m_frame[frmSize] = *it;

		(*it).z = frmSize++;
		prob = 1.f * frmSize / frmExpSize;

		std::swap(*it++, *m_edge++);
	}

	// build frame
	BVHNode& root = m_nodes[0];
	root.leftCntPar = { 0, frmSize, -1, 0 };
	updateNodeBoundsStoh(0);
	subdivideStohQueue(0);

	for (int i{}; i < frmSize - 1; ++i) {
		m_primMortonFrmLeaf[i].y = i + 1;
	}
	m_primMortonFrmLeaf[frmSize - 1].y = m_primsCnt;

	std::vector<int> offsets(2 * m_primsCnt - 1);
 	m_frmSize = frmSize;
	for (int i{ m_frmSize }; i < m_primsCnt; ++i) {
		int leaf{};
		if (m_algInsert == 1)
			leaf = findBestLeafMorton((*m_edge).x, (*m_edge).z);
		else if (m_algInsert == 2)
			leaf = findBestLeafBVHPrims((*m_edge).x, (*m_edge).z);
		else if (m_algInsert == 3)
			leaf = findBestLeafBVHPrimsPlus((*m_edge).x, (*m_edge).z);
		else if (m_algInsert == 4)
			leaf = findBestLeafBVHTree((*m_edge).x, (*m_edge).z);
		else if (m_algInsert == 5)
			leaf = findBestLeafBVHNodes((*m_edge).x, (*m_edge).z);
		else
			leaf = findBestLeafBruteforce((*m_edge).x);

		++offsets[leaf];

		XMUINT4& frmPrim{ m_primMortonFrmLeaf[m_nodes[leaf].leftCntPar.x] };
		m_primMortonFrmLeaf[i].y = frmPrim.y;
		frmPrim.y = i;

		++m_edge;
	}

	std::vector<XMUINT4> temp{ static_cast<size_t>(m_primsCnt) };
	for (int i{}, j{}; j < m_primsCnt; ++i, j = m_primMortonFrmLeaf[j].y) {
		temp[i] = m_primMortonFrmLeaf[j];
	}
	m_primMortonFrmLeaf = temp;

	int firstOffset{};
	postForEach(0, [&](int nodeId) {
		if (m_nodes[nodeId].leftCntPar.y) {
			m_nodes[nodeId].leftCntPar.x += firstOffset;
			m_nodes[nodeId].leftCntPar.y += offsets[nodeId];
			firstOffset += offsets[nodeId];

			updateNodeBoundsStoh(nodeId);
			subdivideStoh2(nodeId);
			return;
		}
			
		m_nodes[nodeId].bb = AABB::bbUnion(
			m_nodes[m_nodes[nodeId].leftCntPar.x].bb,
			m_nodes[m_nodes[nodeId].leftCntPar.x + 1].bb
		);
	});

	for (int i{}; i < m_primsCnt; ++i) {
		m_primMortonFrmLeaf[i] = {
			m_primMortonFrmLeaf[i].x, 0, 0, 0
		};
	}
	  
	m_nodes[0] = m_nodes[0];
}

float BVH::primInsertMetric(int primId, int nodeId) {
	Primitive prim = m_prims[primId]; 
	BVHNode node = m_nodes[nodeId];

	float oldSA{ node.bb.area() };
	float newSA{ AABB::bbUnion(node.bb, prim.bb).area() };
	float cost{ newSA * (node.leftCntPar.y + 1) - oldSA * node.leftCntPar.y };

	while (node.leftCntPar.z != -1) {
		oldSA = node.bb.area();
		newSA = AABB::bbUnion(node.bb, prim.bb).area();
		cost += newSA - oldSA;

		node = m_nodes[node.leftCntPar.z];
	}

	return cost;
}

int BVH::findBestLeafBruteforce(int primId) {
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

	return best;
}

int BVH::findBestLeafMorton(int primId, int frmNearest) {
	unsigned int best{ m_frame[frmNearest].w };
	float mincost{ primInsertMetric(primId, best) };

	// morton window
	auto b = std::max<int>(frmNearest - m_insertSearchWindow, 0);
	auto e = std::min<int>(m_frmSize, frmNearest + m_insertSearchWindow) - 1;
	for (int i{ frmNearest - 1 }, j{ frmNearest + 1 }; b <= i || j <= e; --i, ++j) {
		if (b <= i) {
			float cost = primInsertMetric(primId, m_frame[i].w);
			if (cost < mincost) {
				mincost = cost;
				best = m_frame[i].w;
			}
		}
		if (j <= e) {
			float cost = primInsertMetric(primId, m_frame[j].w);
			if (cost < mincost) {
				mincost = cost;
				best = m_frame[j].w;
			}
		}
	}

	return best;
}

int BVH::findBestLeafBVHPrims(int primId, int frmNearest) {
	float mincost = std::numeric_limits<float>::max();
	int best{ -1 };

	// morton window
	auto b = std::max<int>(m_frame[frmNearest].z - m_insertSearchWindow, 0);
	auto e = std::min<int>(m_frmSize, m_frame[frmNearest].z + m_insertSearchWindow);
	for (int i{ b }; i < e; ++i) {
		float currmin = primInsertMetric(primId, m_primMortonFrmLeaf[i].w);
		if (currmin < mincost) {
			mincost = currmin;
			best = m_primMortonFrmLeaf[i].w;
		}
	}

	return best != -1 ? best : m_frame[frmNearest].w;
}

int BVH::findBestLeafBVHPrimsPlus(int primId, int frmNearest) {
	int best{ static_cast<int>(m_frame[frmNearest].w) };
	float mincost{ primInsertMetric(primId, best) };

	int id{ static_cast<int>(m_frame[frmNearest].z - 1) };
	for (int cnt{}; cnt < m_insertSearchWindow && id != -1; ++cnt) {
		int node{ static_cast<int>(m_primMortonFrmLeaf[id--].w) };
		float cost{ primInsertMetric(primId, node) };
		if (cost < mincost) {
			mincost = cost;
			best = node;
		}

		while (id != -1 && m_primMortonFrmLeaf[id--].w == node);
	}

	id = m_frame[frmNearest].z + 1;
	for (int cnt{}; cnt < m_insertSearchWindow && id < m_frmSize; ++cnt) {
		int node{ static_cast<int>(m_primMortonFrmLeaf[id++].w) };
		float cost{ primInsertMetric(primId, node) };
		if (cost < mincost) {
			mincost = cost;
			best = node;
		}

		while (id < m_frmSize && m_primMortonFrmLeaf[id++].w == node);
	}

	return best;
}

int BVH::findBestLeafBVHTree(int primId, int frmNearest) {
	int leaf{ static_cast<int>(m_frame[frmNearest].w) }, best{ leaf };
	float mincost{ primInsertMetric(primId, leaf) };

	for (int i{}; i < m_insertSearchWindow; ++i) {
		leaf = leftLeaf(leaf);
		if (leaf == -1) break;

		float cost{ primInsertMetric(primId, leaf) };
		if (cost < mincost) {
			mincost = cost;
			best = leaf;
		}
	}

	leaf = m_frame[frmNearest].w;
	for (int i{}; i < m_insertSearchWindow; ++i) {
		leaf = rightLeaf(leaf);
		if (leaf == -1) break;

		float cost{ primInsertMetric(primId, leaf) };
		if (cost < mincost) {
			mincost = cost;
			best = leaf;
		}
	}

	return best;
}

int BVH::findBestLeafBVHNodes(int primId, int frmNearest) {
	int best{ static_cast<int>(m_frame[frmNearest].w) }, leaf{ best };
	float mincost{ primInsertMetric(primId, best) };

	for (int i{}; i < m_insertSearchWindow; ++i) {
		for (--leaf; leaf && !m_nodes[leaf].leftCntPar.y; --leaf);
		if (!leaf) break;

		float cost{ primInsertMetric(primId, leaf) };
		if (cost < mincost) {
			mincost = cost;
			best = leaf;
		}
	}

	leaf = m_frame[frmNearest].w;
	for (int i{}; i < m_insertSearchWindow; ++i) {
		for (++leaf; leaf < m_nodesUsed && !m_nodes[leaf].leftCntPar.y; ++leaf);
		if (leaf == m_nodesUsed) break;

		float cost{ primInsertMetric(primId, leaf) };
		if (cost < mincost) {
			mincost = cost;
			best = leaf;
		}
	}

	return best;
}

void BVH::subdivideStoh(INT nodeId) {
	BVHNode& node{ m_nodes[nodeId] };
	if (node.leftCntPar.y < 2) {
		auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
		for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
			(*it).w = nodeId;
			m_frame[(*it).z].z = i;
			m_frame[(*it).z].w = nodeId;
		}

		++m_leafsCnt;
		updateDepths(nodeId);
		return;
	}

	// determine split axis and position
	int axis{}, lCnt{}, rCnt{};
	float splitPos{};
	float cost{};
	cost = splitBinnedSAHStoh(node, axis, splitPos, lCnt, rCnt);

	if (m_algBuild != 0 && cost >= node.bb.area() * node.leftCntPar.y) {
		auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
		for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
			(*it).w = nodeId;
			m_frame[(*it).z].z = i;
			m_frame[(*it).z].w = nodeId;
		}

		++m_leafsCnt;
		updateDepths(nodeId);
		return;
	}

	// in-place partition
	for (auto l = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x),
		r = std::next(l, node.leftCntPar.y - 1); l != r;)
	{
		if (splitPos <= comp(m_prims[(*l).x].ctr, axis)) {
			std::swap((*l), (*r--));
		}
		else l++;
	}

	// create child nodes
	int leftIdx{ m_nodesUsed++ };
	m_nodes[leftIdx].leftCntPar = {
		node.leftCntPar.x, lCnt, nodeId, 0
	};
	updateNodeBoundsStoh(leftIdx);

	int rightIdx{ m_nodesUsed++ };
	m_nodes[rightIdx].leftCntPar = {
		node.leftCntPar.x + lCnt, rCnt, nodeId, 0
	};
	updateNodeBoundsStoh(rightIdx);

	node.leftCntPar = {
		leftIdx, 0, node.leftCntPar.z, 0
	};

	// recurse
	subdivideStoh(leftIdx);
	subdivideStoh(rightIdx);
}

void BVH::subdivideStohQueue(INT rootId) {
	std::queue<int> nodes{};
	nodes.push(rootId);

	while (!nodes.empty()) {
		int nodeId{ nodes.front()};
		nodes.pop();

		BVHNode& node{ m_nodes[nodeId] };

		if (node.leftCntPar.y == 1) {
			auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
			for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
				(*it).w = nodeId;
				m_frame[(*it).z].z = i;
				m_frame[(*it).z].w = nodeId;
			}

			++m_leafsCnt;
			updateDepths(nodeId);
			continue;
		}

		// determine split axis and position
		int axis{}, lCnt{}, rCnt{};
		float splitPos{};
		float cost{};
		cost = splitBinnedSAHStoh(node, axis, splitPos, lCnt, rCnt);

		if (m_algBuild != 0 && cost >= node.bb.area() * node.leftCntPar.y) {
			auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
			for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
				(*it).w = nodeId;
				m_frame[(*it).z].z = i;
				m_frame[(*it).z].w = nodeId;
			}

			++m_leafsCnt;
			updateDepths(nodeId);
			continue;
		}

		// in-place partition
		for (auto l = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x),
			r = std::next(l, node.leftCntPar.y - 1); l != r;)
		{
			if (splitPos <= comp(m_prims[(*l).x].ctr, axis)) {
				std::swap((*l), (*r--));
			}
			else l++;
		}

		// create child nodes
		int leftIdx{ m_nodesUsed++ };
		m_nodes[leftIdx].leftCntPar = {
			node.leftCntPar.x, lCnt, nodeId, 0
		};
		updateNodeBoundsStoh(leftIdx);

		int rightIdx{ m_nodesUsed++ };
		m_nodes[rightIdx].leftCntPar = {
			node.leftCntPar.x + lCnt, rCnt, nodeId, 0
		};
		updateNodeBoundsStoh(rightIdx);

		node.leftCntPar = {
			leftIdx, 0, node.leftCntPar.z, 0
		};

		// recurse
		nodes.push(leftIdx);//subdivideStoh(leftIdx);
		nodes.push(rightIdx);//subdivideStoh(rightIdx);
	}
}

void BVH::subdivideStoh2(INT nodeId) {
	BVHNode& node{ m_nodes[nodeId] };
	if (node.leftCntPar.y < 2) {
		++m_leafsCnt;
		updateDepths(nodeId);
		return;
	}

	// determine split axis and position
	int axis{}, lCnt{}, rCnt{};
	float splitPos{};
	float cost{};
	cost = splitBinnedSAHStoh(node, axis, splitPos, lCnt, rCnt);

	if (m_algBuild != 0 && cost >= node.bb.area() * node.leftCntPar.y) {
		++m_leafsCnt;
		updateDepths(nodeId);
		return;
	}

	// in-place partition
	for (auto l = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x),
		r = std::next(l, node.leftCntPar.y - 1); l != r;)
	{
		if (splitPos <= comp(m_prims[(*(l)).x].ctr, axis))
			std::swap((*(l)), (*(r--)));
		else l++;
	}

	// create child nodes
	int leftIdx{ m_nodesUsed++ };
	m_nodes[leftIdx].leftCntPar = {
		node.leftCntPar.x, lCnt, nodeId, 0
	};
	updateNodeBoundsStoh(leftIdx);

	int rightIdx{ m_nodesUsed++ };
	m_nodes[rightIdx].leftCntPar = {
		node.leftCntPar.x + lCnt, rCnt, nodeId, 0
	};
	updateNodeBoundsStoh(rightIdx);

	node.leftCntPar = {
		leftIdx, 0, node.leftCntPar.z, 0
	};

	// recurse
	subdivideStoh2(leftIdx);
	subdivideStoh2(rightIdx);
}

float BVH::splitBinnedSAHStoh(BVHNode& node, int& axis, float& splitPos, int& leftCnt, int& rightCnt) {
	float bestCost{ std::numeric_limits<float>::max() };
	for (int a{}; a < 3; ++a) {
		float bmin{ comp(node.bb.bmin, a) };
		float bmax{ comp(node.bb.bmax, a) };
		if (bmin == bmax)
			continue;

		AABB bounds[MaxSteps]{};
		int m_primsCnt[MaxSteps]{};

		float step = m_sahSteps / (bmax - bmin);

		auto start = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
		auto end = std::next(start, node.leftCntPar.y);
		for (auto it = start; it != end; ++it) {
			Primitive& t = m_prims[(*it).x];
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
				leftCnt = lCnt[i];
				rightCnt = rCnt[i];
				bestCost = planeCost;
			}
		}
	}
	return bestCost;
}

void BVH::mortonSort() {
	// AABB of all primitives centroids
	AABB aabb{};
	for (const Primitive& tr : m_prims) {
		aabb.grow(tr.ctr);
	}

	// compute morton indices of primitives
	auto it = m_primMortonFrmLeaf.begin();
	for (int i{}; i < m_primsCnt; ++i) {
		int mortonScale{ 1 << 10 };
		Vector4 relateCtr{ aabb.relateVecPos(m_prims[i].ctr) };
		(*it++).y = encodeMorton(mortonScale * relateCtr);
	}

	// sort primitives
	std::sort(
		m_primMortonFrmLeaf.begin(),
		m_primMortonFrmLeaf.end(),
		[&](const XMUINT4& p1, const XMUINT4& p2) {
			return p1.y < p2.y;
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
		Primitive& leafTri = m_prims[m_primMortonFrmLeaf[node.leftCntPar.x + i].x];

		node.bb.grow(leafTri.v0);
		node.bb.grow(leafTri.v1);
		node.bb.grow(leafTri.v2);
	}
}

void BVH::updateNodeBoundsStoh(INT nodeIdx) {
	BVHNode& node = m_nodes[nodeIdx];
	node.bb = {};

	auto start = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
	auto end = std::next(start, node.leftCntPar.y);
	for (auto it = start; it != end; ++it) {
		Primitive& p = m_prims[(*it).x];

		node.bb.grow(p.v0);
		node.bb.grow(p.v1);
		node.bb.grow(p.v2);
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
		Primitive& t = m_prims[m_primMortonFrmLeaf[node.leftCntPar.x + i].x];

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
			Primitive& t = m_prims[m_primMortonFrmLeaf[node.leftCntPar.x + i].x];
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
			Primitive& t = m_prims[m_primMortonFrmLeaf[node.leftCntPar.x + i].x];
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

	switch (m_algBuild) {
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

	if (m_algBuild != 0 && cost >= node.bb.area() * node.leftCntPar.y) {
		++m_leafsCnt;
		updateDepths(nodeId);
		return;
	}

	// in-place partition
	INT i{ node.leftCntPar.x };
	INT j{ i + node.leftCntPar.y - 1 };
	while (i <= j) {
		if (splitPos <= comp(m_prims[m_primMortonFrmLeaf[i++].x].ctr, axis))
			std::swap(m_primMortonFrmLeaf[--i].x, m_primMortonFrmLeaf[j--].x);
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

float BVH::costSAH() {
	BVHNode& root = m_nodes[0];
	float sum{};

	preForEach(root.leftCntPar.x, [&](int nodeId) {
		BVHNode& node = m_nodes[nodeId];
		sum += node.bb.area() * std::max(1, node.leftCntPar.y);
		});
	preForEach(root.leftCntPar.x + 1, [&](int id) {
		BVHNode& n = m_nodes[id];
		sum += n.bb.area() * std::max(1, n.leftCntPar.y);
		});

	return sum / root.bb.area();
}
