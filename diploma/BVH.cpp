#include "BVH.h"

#include <algorithm>
#include <queue>
#include <sstream>

#include "SobolMatrices.h"
#include "psr.h"

// ---------------
//	GRAPHICS PART
// ---------------
BVH::BVH(ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext, unsigned int primsCnt) :
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
			.ByteWidth{ (2 * primsCnt + 1) * sizeof(BVH::BVHNode) },
			.Usage{ D3D11_USAGE_DYNAMIC },
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
				.NumElements{ 2 * primsCnt + 1 }
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
			.ByteWidth{ (2 * primsCnt - 1) * sizeof(XMINT4) },
			.Usage{ D3D11_USAGE_DYNAMIC },
			.BindFlags{ D3D11_BIND_SHADER_RESOURCE },
			.CPUAccessFlags{ D3D11_CPU_ACCESS_WRITE },
			.MiscFlags{ D3D11_RESOURCE_MISC_BUFFER_STRUCTURED },
			.StructureByteStride{ sizeof(XMINT4) }
		};

		hr = m_pDevice->CreateBuffer(&desc, nullptr, &m_pPrimIdsBuffer);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pPrimIdsBuffer, "PrimIdsBuffer");
		THROW_IF_FAILED(hr);

		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV{
			.Format{ DXGI_FORMAT_UNKNOWN },
			.ViewDimension{ D3D11_SRV_DIMENSION_BUFFER },
			.Buffer{
				.FirstElement{ 0 },
				.NumElements{ (2 * primsCnt - 1) }
			}
		};

		hr = m_pDevice->CreateShaderResourceView(m_pPrimIdsBuffer, &descSRV, &m_pPrimIdsBufferSRV);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pPrimIdsBufferSRV, "PrimIdsBufferSRV");
		THROW_IF_FAILED(hr);
	}

	// create model buffer
	{
		D3D11_BUFFER_DESC desc{
			.ByteWidth{ sizeof(ModelBuffer) * (2 * primsCnt - 1) },
			.Usage{ D3D11_USAGE_DYNAMIC },
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
			.Buffer{.NumElements{ 2 * primsCnt - 1 } }
		};
		hr = m_pDevice->CreateShaderResourceView(m_pModelBuffer, &descSRV, &m_pModelBufferSRV);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pModelBufferSRV, "BVHRendererModelBufferSRV");
		THROW_IF_FAILED(hr);
	}

	sce::Psr::init();
}

void BVH::term() {
	sce::Psr::shutDown();

	SAFE_RELEASE(m_pPrimIdsBufferSRV);
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

	if (m_algBuild == 5 || m_toQBVH) {
		if (m_aabbHighlightAll) {
			preForEachQuad(0, [&](int nodeId) {
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

			preForEachQuad(m_aabbHighlightNode, [&](int nodeId) {
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

			if (m_aabbHighlightAllPrims) {
				preForEachQuad(nodeId, [&](int n) {
					for (int i{}; i < m_nodes[n].leftCntPar.y; ++i) {
						UINT index{ m_primMortonFrmLeaf[m_nodes[n].leftCntPar.x + i].x };
						m_primMortonFrmLeaf[index].y = 1;
					}
				});
			}

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
				for (int i{}; i < m_nodes[parent].leftCntPar.w; ++i) {
					int sibling = m_nodes[parent].leftCntPar.x + i;
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
			}

			if (m_aabbHighlightChildren && !node.leftCntPar.y) {
				for (int i{}; i < node.leftCntPar.w; ++i) {
					int child = node.leftCntPar.x + i;
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
	}
	else {
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

			if (m_aabbHighlightAllPrims) {
				preForEach(nodeId, [&](int n) {
					for (int i{}; i < m_nodes[n].leftCntPar.y; ++i) {
						UINT index{ m_primMortonFrmLeaf[m_nodes[n].leftCntPar.x + i].x };
						m_primMortonFrmLeaf[index].y = 1;
					}
				});
			}

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
	}

	D3D11_MAPPED_SUBRESOURCE subres{};
	THROW_IF_FAILED(m_pDeviceContext->Map(m_pModelBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subres));
	memcpy(subres.pData, m_modelBuffers.data(), m_modelBuffers.size() * sizeof(ModelBuffer));
	m_pDeviceContext->Unmap(m_pModelBuffer, 0);

	//m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, m_modelBuffers.data(), 0, 0);
}

void BVH::updateBuffers() {
	D3D11_MAPPED_SUBRESOURCE subres{};
	THROW_IF_FAILED(m_pDeviceContext->Map(m_pBVHBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subres));
	memcpy(subres.pData, m_nodes.data(), sizeof(BVHNode) * m_nodes.capacity());
	m_pDeviceContext->Unmap(m_pBVHBuffer, 0);

	subres = {};
	THROW_IF_FAILED(m_pDeviceContext->Map(m_pPrimIdsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subres));
	memcpy(subres.pData, m_primMortonFrmLeaf.data(), sizeof(XMINT4) * m_primsCnt);
	m_pDeviceContext->Unmap(m_pPrimIdsBuffer, 0);
}

void BVH::renderBVHImGui() {
	ImGui::Begin("BVH");

	ImGui::Text("Split algorithm:");

	bool isDichotomy{ m_algBuild == 0 };
	ImGui::Checkbox("Dichotomy", &isDichotomy);
	if (isDichotomy) {
		m_algBuild = 0;
		ImGui::DragInt("Primitives per leaf", &m_primsPerLeaf, 1, 1, 32);
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
		ImGui::DragInt("Primitives per leaf", &m_primsPerLeaf, 1, 1, 32);
	}

	bool isPSR{ m_algBuild == 5 };
	ImGui::Checkbox("PSR", &isPSR);
	if (isPSR) m_algBuild = 5;

	bool isStochastic{ m_algBuild == 4 };
	ImGui::Checkbox("Stochastic", &isStochastic);
	if (isStochastic) {
		m_algBuild = 4;

		ImGui::DragInt("SAH step", &m_sahSteps, 1, 2, 32);
		ImGui::DragInt("Primitives per leaf", &m_primsPerLeaf, 1, 1, 32);

		ImGui::Text("Prims weighting and clamping");

		ImGui::Text("Weights: %.3f ... %.3f", m_primWeightMin, m_primWeightMax);

		ImGui::DragFloat("Base", &m_clampBase, 0.1f, 0.f, 10.f);
		ImGui::DragInt("Offset", &m_clampOffset, 1, 0, 128);
		ImGui::DragInt("Bins count", &m_clampBinCnt, 1, 0, 256);

		ImGui::Text("Clamp: %.3f", m_clamp);
		ImGui::Text("# of clamped: %d", m_clampedCnt);
		ImGui::Text("# of splitted: %d", m_splitCnt);

		ImGui::Text("Frame size: %d", static_cast<int>(std::round(m_primsCnt * m_frmPart)));

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

		bool isInsertSmartBVH{ m_algInsert == 6 };
		ImGui::Checkbox("Smart BVH", &isInsertSmartBVH);
		if (isInsertSmartBVH) m_algInsert = 6;

		if (m_algInsert && m_algInsert != 6) {
			ImGui::DragInt(
				"Insert search window",
				&m_insertSearchWindow, 1, 0,
				static_cast<int>(std::round(m_primsCnt * m_frmPart))
			);
		}

		ImGui::Text("Insertion prims algorithm conditions:");

		bool isInsertNoExt{ m_algInsertConds == 0 };
		ImGui::Checkbox("Original insert conditions", &isInsertNoExt);
		if (isInsertNoExt) m_algInsertConds = 0;

		bool isInsertUpdCnt{ m_algInsertConds == 1 };
		ImGui::Checkbox("Update prims count", &isInsertUpdCnt);
		if (isInsertUpdCnt) m_algInsertConds = 1;

		bool isInsertUpdAABB{ m_algInsertConds == 2 };
		ImGui::Checkbox("Update AABB", &isInsertUpdAABB);
		if (isInsertUpdAABB) m_algInsertConds = 2;

		bool isInsertUpdAll{ m_algInsertConds == 3 };
		ImGui::Checkbox("Update all", &isInsertUpdAll);
		if (isInsertUpdAll) m_algInsertConds = 3;

		ImGui::Text("Prims splitting:");

		bool isNoSplit{ m_primSplitting == 0 };
		ImGui::Checkbox("No splitting", &isNoSplit);
		if (isNoSplit) m_primSplitting = 0;

		bool isSplitSubsetBeforeCluster{ m_primSplitting == 1 };
		ImGui::Checkbox("Subset only", &isSplitSubsetBeforeCluster);
		if (isSplitSubsetBeforeCluster) m_primSplitting = 1;

		bool isSplitNaive{ m_primSplitting == 2 };
		ImGui::Checkbox("Split prev hist interval", &isSplitNaive);
		if (isSplitNaive) m_primSplitting = 2;

		bool isSplitSmart{ m_primSplitting == 3 };
		ImGui::Checkbox("Split big without uniform", &isSplitSmart);
		if (isSplitSmart) m_primSplitting = 3;
	}

	if (m_algBuild != 5) {
		ImGui::Checkbox("BVH to QBVH", &m_toQBVH);
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

		ImGui::Checkbox("All primitives", &m_aabbHighlightAllPrims);
	}

	ImGui::End();

	updateRenderBVH();
	updateBuffers();
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


// ------------
//	LOGIC PART
// ------------
void BVH::init(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix) {
	m_primsCntOrig = m_primsCnt = idsCnt;

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
		m_prims[i].updCtrAndBB();

		m_aabbAllCtrs.grow(m_prims[i].ctr);
		m_aabbAllPrims.grow(m_prims[i].bb);

		m_primMortonFrmLeaf[i] = { i, 0, 0, 0 };
	}
}

void BVH::build(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix) {
	if (m_algBuild == 5) {
		buildPsr(vts, vtsCnt, ids, idsCnt, modelMatrix);
		return;
	}

	init(vts, vtsCnt, ids, idsCnt, modelMatrix);
	if (m_algBuild != 4) {
		m_nodes[0].leftCntPar = {
			0, m_primsCnt, -1, 0
		};
		if (m_algBuild == 3) {
			subdivideStohIntelQueue(0);
		}
		else {
			updateNodeBounds(0);
			subdivide(0);
		}
	}
	else {
		//float frmPartBest{}, uniformBest{};
		//float costBest{ std::numeric_limits<float>::max() };
		//for (int i{ 2 }; i < 100; ++i) {
		//	for (int j{}; j < 100; j += 10) {
		//		init(vts, vtsCnt, ids, idsCnt, modelMatrix);
		//		m_frmPart = i / 100.f;
		//		m_uniform = j / 100.f;
		//		buildStochastic();
		//		binaryBVH2QBVH();
		//		float cost{ costSAH() };

		//		std::wstringstream ss;
		//		ss << m_frmPart << TEXT("/") << m_uniform << TEXT(" -> ") << cost << std::endl;
		//		OutputDebugString(ss.str().c_str());

		//		if (cost < costBest) {
		//			ss.clear();
		//			ss << cost << TEXT(" < ") << costBest << TEXT(" ->  update") << std::endl;
		//			OutputDebugString(ss.str().c_str());

		//			costBest = cost;
		//			frmPartBest = m_frmPart;
		//			uniformBest = m_uniform;
		//		}
		//	}
		//}
		//m_frmPart = frmPartBest;
		//m_uniform = uniformBest;

		buildStochastic();
	}

	if (m_toQBVH)
		binaryBVH2QBVH();
}

void BVH::buildPsr(Vector4* vts, INT vtsCnt, XMINT4* ids, INT idsCnt, Matrix modelMatrix) {
	sce::Psr::BottomLevelBvhDescriptor descriptor{};

    sce::Psr::Cpu::BottomLevelBvhConfig builderConfig{};
    builderConfig.init();
	{
		// All nodes use FP32 encoding
		builderConfig.m_encodingMode = sce::Psr::Cpu::BvhEncodingMode::kFullPrecision;
		// High quality using triangle splits and fast-binned SAH estimation
		builderConfig.m_builder = sce::Psr::Cpu::BottomLevelBvhBuilder::kHighQualityBinWithSplit;
		// BVH can be refitted
		builderConfig.m_refittingMode = sce::Psr::RefittingMode::kNonRefittable;
		// The bottom-level BVH can be opened during rebraiding
		builderConfig.m_rebraidingMode = sce::Psr::RebraidingMode::kDisabled;
		// ???
		builderConfig.m_spatialSplitMode = sce::Psr::Cpu::SpatialSplitMode::kRecursiveExhaustive;
		// Builds a large BVH with few primitives per leaf
		builderConfig.m_size = sce::Psr::Cpu::BvhSize::kLarge;
		// The default spatial split budget used by the CPU-based builders
		builderConfig.m_recursiveSplitBudget = 1.7f;
	}
	sce::Psr::Cpu::checkBottomLevelBvhConfig(builderConfig);

	std::vector<XMUINT3> idsPsr(idsCnt);
	for (int i{}; i < idsCnt; ++i) idsPsr[i] = {
		static_cast<unsigned>(ids[i].x),
		static_cast<unsigned>(ids[i].y),
		static_cast<unsigned>(ids[i].z)
	};

	std::vector<XMFLOAT3> vtsPsr(vtsCnt);
	for (int i{}; i < vtsCnt; ++i) vtsPsr[i] = { vts[i].x, vts[i].y, vts[i].z };

	sce::Psr::Cpu::GeometryConfig geometries{};
	geometries.init();
	{
		geometries.m_kind = geometries.kMesh;
		geometries.m_flags = sce::Psr::GeometryFlags::kNone;

		sce::Psr::Cpu::GeometryMeshConfig mesh{};
		mesh.init();
		{
			mesh.m_vertexData = static_cast<void*>(vtsPsr.data());
			mesh.m_triangleData = static_cast<void*>(idsPsr.data());
			mesh.m_triangleFan = nullptr;
			mesh.m_triangleFanInput = sce::Psr::Cpu::TriangleFanInput::kHighQualityOnTheFly;
			mesh.m_objectToWorld[ 0] = modelMatrix._11; mesh.m_objectToWorld[ 1] = modelMatrix._12;
			mesh.m_objectToWorld[ 2] = modelMatrix._13; mesh.m_objectToWorld[ 3] = modelMatrix._14;
			mesh.m_objectToWorld[ 4] = modelMatrix._21; mesh.m_objectToWorld[ 5] = modelMatrix._22;
			mesh.m_objectToWorld[ 6] = modelMatrix._23; mesh.m_objectToWorld[ 7] = modelMatrix._24;
			mesh.m_objectToWorld[ 8] = modelMatrix._31; mesh.m_objectToWorld[ 9] = modelMatrix._32;
			mesh.m_objectToWorld[10] = modelMatrix._33; mesh.m_objectToWorld[11] = modelMatrix._34;
			mesh.m_objectToWorld[12] = modelMatrix._41; mesh.m_objectToWorld[13] = modelMatrix._42;
			mesh.m_objectToWorld[14] = modelMatrix._43; mesh.m_objectToWorld[15] = modelMatrix._44;
			mesh.m_vertexStride = 12;
			mesh.m_triangleStride = 12;
			mesh.m_vertexCount = vtsCnt;
			mesh.m_triangleCount = idsCnt;
			mesh.m_vertexFormat = sce::Psr::Cpu::VertexFormat::kFp32x3;
			mesh.m_triangleFormat = sce::Psr::Cpu::TriangleFormat::kUint32x3;
		}

		geometries.m_mesh = mesh;
	}

	uint32_t geometryCount{ 1 };

	size_t bvhSize{};
	sce::Psr::Cpu::getBottomLevelBvhSizeUpperBound(&bvhSize, builderConfig, &geometries, geometryCount);

	void* bvhMemory{ _aligned_malloc(bvhSize, 256) };

	size_t scratchSize{ bvhSize };
	void* scratchMemory{ _aligned_malloc(scratchSize, 256) };

	sce::Psr::Status status{ sce::Psr::Cpu::buildBottomLevelBvh(
		&descriptor,
		builderConfig,
		&geometries,
		geometryCount,
		bvhMemory,
		bvhSize,
		scratchMemory,
		scratchSize
	) };

	if (status != sce::Psr::Status::kSuccess)
		return;

	sce::Psr::BvhStatistics bvhStatistics{};
	status = sce::Psr::Cpu::getBottomLevelBvhStatistics(&bvhStatistics, descriptor);
	if (status != sce::Psr::Status::kSuccess)
		return;

	sce::Psr::CompleteBvhStatistics completeBvhStatistics{};
	status = sce::Psr::Cpu::getBottomLevelCompleteBvhStatistics(&completeBvhStatistics, descriptor, scratchMemory, scratchSize);
	if (status != sce::Psr::Status::kSuccess)
		return;

	sce::Psr::BottomLevelBvhView bottomLevelBvhView{ sce::Psr::getBottomLevelBvhView(descriptor) };

	// psr2my
	m_primsCntOrig = idsCnt;
	m_primsCnt = 0;

	m_nodesUsed = 1;
	m_leafsCnt = completeBvhStatistics.m_leafNodeCount;
	m_depthMin = -1;
	m_depthMax = completeBvhStatistics.m_depth;

	//m_prims.resize(m_primsCnt); ?

	m_primMortonFrmLeaf.clear();
	m_primMortonFrmLeaf.resize(completeBvhStatistics.m_primReferenceCount);

	m_nodes.clear();
	m_nodes.resize(1 + completeBvhStatistics.m_internalNodeCount + completeBvhStatistics.m_leafNodeCount);

	m_nodes[0].bb = {
		.bmin{
			completeBvhStatistics.m_bounds.m_minX,
			completeBvhStatistics.m_bounds.m_minY,
			completeBvhStatistics.m_bounds.m_minZ,
			0.f
		},
		.bmax{
			completeBvhStatistics.m_bounds.m_maxX,
			completeBvhStatistics.m_bounds.m_maxY,
			completeBvhStatistics.m_bounds.m_maxZ,
			0.f
		}
	};
	m_nodes[0].leftCntPar = { -1, 0, -1, 0 };

	auto rootPsrId = sce::Psr::BvhNode::kEncodedRootIndex >> sce::Psr::BvhNode::kShift;

	std::queue<std::pair<size_t, size_t>> nodeIds{};
	nodeIds.push({ 0, rootPsrId });

	while (!nodeIds.empty()) {
		size_t nodeId{ nodeIds.front().first };
		size_t nodePsrId{ nodeIds.front().second };
		nodeIds.pop();

		BVHNode& node{ m_nodes[nodeId] };

		sce::Psr::BvhNode& bvhPsrNode{ bottomLevelBvhView.m_node[nodePsrId] };
		sce::Psr::BvhNodeFp32* nodePsr{ reinterpret_cast<sce::Psr::BvhNodeFp32*>(bvhPsrNode.m_storage) };
		//sce::Psr::BvhNodeFp32& nodePsr{ *reinterpret_cast<sce::Psr::BvhNodeFp32*>(bottomLevelBvhView.m_node[nodePsrId].m_storage) };

		node.leftCntPar.x = m_nodesUsed;

		for (int i{}; i < 4; ++i) {
			if (nodePsr->m_index[i] == sce::Psr::BvhNode::kInvalidIndex)
				continue;

			unsigned childId{ nodePsr->m_index[i] >> sce::Psr::BvhNode::kShift };

			// leaf child node
			if (bottomLevelBvhView.m_firstPrimitiveIndex <= childId && childId < bottomLevelBvhView.m_prim.m_len) {
				BVHNode& leaf{ m_nodes[m_nodesUsed++] };
				leaf.bb = {
					.bmin{
						nodePsr->m_box[i].m_min.x,
						nodePsr->m_box[i].m_min.y,
						nodePsr->m_box[i].m_min.z,
						0.f
					},
					.bmax{
						nodePsr->m_box[i].m_max.x,
						nodePsr->m_box[i].m_max.y,
						nodePsr->m_box[i].m_max.z,
						0.f
					}
				};
				leaf.leftCntPar = { m_primsCnt, 0, static_cast<int>(nodeId), 0 };

				sce::Psr::BottomLevelBvhPrim prim{};
				do {
					prim = bottomLevelBvhView.m_prim[childId + leaf.leftCntPar.y++];
					m_primMortonFrmLeaf[m_primsCnt++].x = prim.getOriginIndex();
				} while (!prim.isLastPrim());

				++node.leftCntPar.w;
				continue;
			}
			
			// internal child node
			if (rootPsrId < childId && childId < bottomLevelBvhView.m_firstTriNodeIndex) {
				BVHNode& child{ m_nodes[m_nodesUsed] };
				child.bb = {
					.bmin{
						nodePsr->m_box[i].m_min.x,
						nodePsr->m_box[i].m_min.y,
						nodePsr->m_box[i].m_min.z,
						0.f
					},
					.bmax{
						nodePsr->m_box[i].m_max.x,
						nodePsr->m_box[i].m_max.y,
						nodePsr->m_box[i].m_max.z,
						0.f
					}
				};
				child.leftCntPar = { -1, 0, static_cast<int>(nodeId), 0 };

				nodeIds.push({ m_nodesUsed++, childId });

				++node.leftCntPar.w;
				continue;
			}
		}

	}
	
	m_nodes[0].leftCntPar.z = -2;
	m_nodes[0] = m_nodes[0];
}

void BVH::binaryBVH2QBVH() {
	std::vector<BVHNode> newNodes(m_nodesUsed);
	int newNodesUsed{ 1 };
	newNodes[0] = m_nodes[0];

	std::queue<std::pair<int, int>> nodeIds{};
	nodeIds.push({ 0, 0 });

	while (!nodeIds.empty()) {
		int oldNodeId{ nodeIds.front().first };
		int newNodeId{ nodeIds.front().second };
		nodeIds.pop();

		BVHNode& oldNode{ m_nodes[oldNodeId] };
		BVHNode& newNode{ newNodes[newNodeId] };

		if (oldNode.leftCntPar.y) {
			continue;
		}

		int l{ oldNode.leftCntPar.x }, r{ l + 1 };
		if (m_nodes[l].leftCntPar.y) {
			newNode.leftCntPar.x = newNodesUsed;
			++newNode.leftCntPar.w;

			newNodes[newNodesUsed] = m_nodes[l];
			newNodes[newNodesUsed].leftCntPar.z = newNodeId;
			nodeIds.push({ l, newNodesUsed++ });
		}
		else {
			int ll{ m_nodes[l].leftCntPar.x }, lr{ ll + 1 };
			newNode.leftCntPar.x = newNodesUsed;
			newNode.leftCntPar.w += 2;

			newNodes[newNodesUsed] = m_nodes[ll];
			newNodes[newNodesUsed].leftCntPar.z = newNodeId;
			nodeIds.push({ ll, newNodesUsed++ });

			newNodes[newNodesUsed] = m_nodes[lr];
			newNodes[newNodesUsed].leftCntPar.z = newNodeId;
			nodeIds.push({ lr, newNodesUsed++ });
		}

		if (m_nodes[r].leftCntPar.y) {
			++newNode.leftCntPar.w;

			newNodes[newNodesUsed] = m_nodes[r];
			newNodes[newNodesUsed].leftCntPar.z = newNodeId;
			nodeIds.push({ r, newNodesUsed++ });
		}
		else {
			int rl{ m_nodes[r].leftCntPar.x }, rr{ rl + 1 };
			newNode.leftCntPar.w += 2;

			newNodes[newNodesUsed] = m_nodes[rl];
			newNodes[newNodesUsed].leftCntPar.z = newNodeId;
			nodeIds.push({ rl, newNodesUsed++ });

			newNodes[newNodesUsed] = m_nodes[rr];
			newNodes[newNodesUsed].leftCntPar.z = newNodeId;
			nodeIds.push({ rr, newNodesUsed++ });
		}
	}

	m_nodes = newNodes;
	m_nodesUsed = newNodesUsed;
	m_nodes[0].leftCntPar.z = -2;
}

void BVH::buildStochastic() {
	auto it = m_primMortonFrmLeaf.begin();
	// compute morton indices of primitives
	for (int i{}; i < m_primsCnt; ++i) {
		(*it++).y = encodeMorton((1 << 10) * m_aabbAllCtrs.relateVecPos(m_prims[i].ctr));
	}

	// sort primitives
	std::sort(
		m_primMortonFrmLeaf.begin(),
		m_primMortonFrmLeaf.end(),
		[&](const XMUINT4& p1, const XMUINT4& p2) {
			return p1.y < p2.y;
		}
	);

	// init weights
	std::vector<float> cdf(m_primsCnt);
	float sum{};
	float wmin{ std::numeric_limits<float>::max() };
	float wmax{ std::numeric_limits<float>::lowest() };

	// algorithm 1: histogram weight clamping
	std::vector<int> bin_cnts(m_clampBinCnt);

	// cdf init and weight clamping histogram building (alg 1)
	it = m_primMortonFrmLeaf.begin();
	for (int i{}; i < m_primsCnt; ++i) {
		sum += (cdf[i] = m_prims[(*(it++)).x].bb.area());
		wmin = std::min<float>(wmin, cdf[i]);
		wmax = std::max<float>(wmax, cdf[i]);

		++bin_cnts[static_cast<size_t>(std::min<float>(
			std::max<float>(
				m_clampOffset + std::floor(std::log(cdf[i]) / std::log(m_clampBase)),
				0.f
			),
			m_clampBinCnt - 1.f
		))];
	}

	m_primWeightMin = wmin;
	m_primWeightMax = wmax;

	if (!m_primSplitting || m_primSplitting == 1) {
		// primitive probability, compensate uniformity
		float s{ 1.f / (m_primsCnt * m_frmPart) };
		s = (s - m_uniform / m_primsCnt) / std::max<float>(1.f - m_uniform, std::numeric_limits<float>::epsilon());

		// unclamped & clamped sums, clamp
		float uSum{}, cSum{ static_cast<float>(m_primsCnt) };
		float clamp{ std::numeric_limits<float>::max() };

		// selection of clamp
		for (int i{}; i < m_clampBinCnt - 1; ++i) {
			float c{ powf(m_clampBase, i - m_clampOffset + 1) };
			float val{ c / (uSum + c * cSum) };
			if (c / (uSum + c * cSum) >= s) {
				clamp = c;
				break;
			}
			uSum += c * bin_cnts[i];
			cSum -= bin_cnts[i];
		}
		m_clamp = clamp;

		// reweighting if found
		m_clampedCnt = 0;
		if (clamp != std::numeric_limits<float>::max()) {
			sum = 0.f;
			for (int i{}; i < m_primsCnt; ++i) {
				if (clamp < cdf[i]) {
					cdf[i] = clamp;
					if (m_primSplitting) m_primMortonFrmLeaf[i].w = 1;
					++m_clampedCnt;
				}
				sum += cdf[i];
			}
		}
		else {
			m_clamp = -1.f;
		}
	}
	else {
		// primitive probability, compensate uniformity
		float s{ 1.f / (m_primsCnt * m_frmPart) };
		float su{
			(s - m_uniform / m_primsCnt) / std::max<float>(1.f - m_uniform, std::numeric_limits<float>::epsilon())
		};

		// unclamped & clamped sums, clamp
		float uSum{}, cSum{ static_cast<float>(m_primsCnt) };
		float clamp{ std::numeric_limits<float>::max() };
		bool clampFind{};
		float clampu{ std::numeric_limits<float>::max() };

		// selection of clamp
		for (int i{}; i < m_clampBinCnt - 1; ++i) {
			float c{ powf(m_clampBase, i - m_clampOffset + 1) };
			float val{ c / (uSum + c * cSum) };
			if (m_primSplitting == 3 && !clampFind && c / (uSum + c * cSum) >= s) {
				clamp = c;
				clampFind = true;
			}
			if (c / (uSum + c * cSum) >= su) {
				clampu = c;
				break;
			}
			uSum += c * bin_cnts[i];
			cSum -= bin_cnts[i];
			if (m_primSplitting == 2)
				clamp = c;
		}
		m_clamp = clampu;

		// reweighting if found
		m_clampedCnt = 0;
		if (clampu != std::numeric_limits<float>::max()) {
			sum = 0.f;
			for (int i{}; i < m_primsCnt; ++i) {
				if (clampu < cdf[i]) {
					cdf[i] = clampu;
					++m_clampedCnt;
				}
				else if (clamp < cdf[i]) {
					m_primMortonFrmLeaf[i].w = 1;
				}
				sum += cdf[i];
			}
		}
		else {
			m_clamp = -1.f;
		}
	}

	// reweighting with uniform dist & calc cdf
	cdf[0] = cdf[0] * (1.f - m_uniform) + sum * m_uniform / m_primsCnt;
	for (int i{ 1 }; i < m_primsCnt; ++i) {
		cdf[i] = cdf[i - 1] + cdf[i] * (1.f - m_uniform) + sum * m_uniform / m_primsCnt;
	}
	sum = cdf[m_primsCnt - 1];

	// selecting for carcass
	m_splitCnt = 0;
	int frmSize{}, frmExpSize{ static_cast<int>(std::round(m_primsCnt * m_frmPart)) };
	m_frame.clear();
	m_frame.resize(frmExpSize);
	float prob{ 1.f * (1.f * frmSize) / frmExpSize };
	it = m_edge = m_primMortonFrmLeaf.begin();
	for (int i{}; i < m_primsCnt; ++i) {
		m_splitCnt += (*it).w;

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
		else if (m_algInsert == 6)
			leaf = findBestLeafSmartBVH((*m_edge).x, (*m_edge).z);
		else
			leaf = findBestLeafBruteforce((*m_edge).x);
		
		++m_nodes[leaf].leftCntPar.w;
		if (m_algInsertConds == 2 || m_algInsertConds == 3)
			m_nodes[leaf].bb.grow(m_prims[(*m_edge).x].bb);

		XMUINT4& frmPrim{ m_primMortonFrmLeaf[m_nodes[leaf].leftCntPar.x] };
		m_primMortonFrmLeaf[i].y = frmPrim.y;
		frmPrim.y = i;

		++m_edge;
	}

	std::vector<XMUINT4> temp;
	if (!m_primSplitting)
		temp.resize(m_primsCnt);
	else {
		temp.resize(2 * m_primsCnt - 1);
		m_prims.resize(2 * m_primsCnt - 1);
	}

	for (int i{}, j{}; j < m_primsCntOrig; ++i, j = m_primMortonFrmLeaf[j].y) {
		if (!m_primSplitting || !m_primMortonFrmLeaf[j].w) {
			temp[i] = m_primMortonFrmLeaf[j];
			continue;
		}

		Primitive& p{ m_prims[m_primMortonFrmLeaf[j].x] };
		m_nodes[m_frame[m_primMortonFrmLeaf[j].z].w].leftCntPar.w += 3;

		Primitive p0{ .v0{ p.v0 }, .v1{ (p.v0 + p.v1) / 2.f }, .v2{ (p.v0 + p.v2) / 2.f } };
		p0.updCtrAndBB();
		m_prims[m_primsCnt] = p0;
		temp[i++] = { static_cast<UINT>(m_primsCnt++), m_primMortonFrmLeaf[j].x, 0, 0 };

		Primitive p1{ .v0{ (p.v0 + p.v1) / 2.f }, .v1{ p.v1 }, .v2{ (p.v1 + p.v2) / 2.f } };
		p1.updCtrAndBB();
		m_prims[m_primsCnt] = p1;
		temp[i++] = { static_cast<UINT>(m_primsCnt++), m_primMortonFrmLeaf[j].x, 0, 0 };

		Primitive p2{ .v0{ (p.v0 + p.v2) / 2.f }, .v1{ (p.v1 + p.v2) / 2.f }, .v2{ p.v2 } };
		p2.updCtrAndBB();
		m_prims[m_primsCnt] = p2;
		temp[i++] = { static_cast<UINT>(m_primsCnt++), m_primMortonFrmLeaf[j].x, 0, 0 };

		Primitive p3{ .v0{ (p.v0 + p.v1) / 2.f }, .v1{ (p.v0 + p.v2) / 2.f }, .v2{ (p.v1 + p.v2) / 2.f } };
		p3.updCtrAndBB();
		m_prims[m_primsCnt] = p3;
		temp[i] = { static_cast<UINT>(m_primsCnt++), m_primMortonFrmLeaf[j].x, 0, 0 };
	}
	m_primMortonFrmLeaf = temp;

	m_leafsCnt = 0;
	int firstOffset{};
	postForEach(0, [&](int nodeId) {
		if (!m_nodes[nodeId].leftCntPar.y) {
			m_nodes[nodeId].bb = AABB::bbUnion(
				m_nodes[m_nodes[nodeId].leftCntPar.x].bb,
				m_nodes[m_nodes[nodeId].leftCntPar.x + 1].bb
			);
			return;
		}

		m_nodes[nodeId].leftCntPar.x += firstOffset;
		m_nodes[nodeId].leftCntPar.y += m_nodes[nodeId].leftCntPar.w;
		firstOffset += m_nodes[nodeId].leftCntPar.w;

		updateNodeBoundsStoh(nodeId);
		subdivideStohQueue(nodeId);
	});

	if (m_primSplitting) {
		for (int i{}; i < m_primsCnt; ++i) {
			bool isSplitted{ m_primMortonFrmLeaf[i].x >= m_primsCntOrig };
			if (isSplitted) {
				unsigned int primId{ m_primMortonFrmLeaf[i].y };
				m_primMortonFrmLeaf[i].x = primId;
				m_primMortonFrmLeaf[primId].w = 1;
			}
		}
	}
	  
	m_nodes[0] = m_nodes[0];
}

float BVH::primInsertMetric(int primId, int nodeId) {
	Primitive prim = m_prims[primId];
	BVHNode node = m_nodes[nodeId];

	int leafPrimsCnt{ node.leftCntPar.y };
	if (m_algInsertConds == 1 || m_algInsertConds == 3)
		leafPrimsCnt += node.leftCntPar.w;

	float cost{
		(leafPrimsCnt + 1) * AABB::bbUnion(node.bb, prim.bb).area()
			- (leafPrimsCnt) * node.bb.area()
	};
	if (node.leftCntPar.z != -1) // TODO
	do {
		node = m_nodes[node.leftCntPar.z];
		cost += AABB::bbUnion(node.bb, prim.bb).area() - node.bb.area();
	} while (node.leftCntPar.z != -1);

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
		float currmin = primInsertMetric(primId, m_frame[m_primMortonFrmLeaf[i].z].w);
		if (currmin < mincost) {
			mincost = currmin;
			best = m_frame[m_primMortonFrmLeaf[i].z].w;
		}
	}

	return best != -1 ? best : m_frame[frmNearest].w;
}

int BVH::findBestLeafBVHPrimsPlus(int primId, int frmNearest) {
	int best{ static_cast<int>(m_frame[frmNearest].w) };
	float mincost{ primInsertMetric(primId, best) };

	int id{ static_cast<int>(m_frame[frmNearest].z - 1) };
	for (int cnt{}; cnt < m_insertSearchWindow && id != -1; ++cnt) {
		int node{ static_cast<int>(m_frame[m_primMortonFrmLeaf[id--].z].w) };
		float cost{ primInsertMetric(primId, node) };
		if (cost < mincost) {
			mincost = cost;
			best = node;
		}

		while (id != -1 && m_primMortonFrmLeaf[id--].w == node);
	}

	id = m_frame[frmNearest].z + 1;
	for (int cnt{}; cnt < m_insertSearchWindow && id < m_frmSize; ++cnt) {
		int node{ static_cast<int>(m_frame[m_primMortonFrmLeaf[id++].z].w) };
		float cost{ primInsertMetric(primId, node) };
		if (cost < mincost) {
			mincost = cost;
			best = node;
		}

		while (id < m_frmSize && m_frame[m_primMortonFrmLeaf[id++].z].w == node);
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

int BVH::findBestLeafSmartBVH(int primId, int frmNearest) {
	Primitive prim = m_prims[primId];

	int bestLeaf{ static_cast<int>(m_frame[frmNearest].w) };
	//float bestCost{ std::numeric_limits<float>::max() };
	float bestCost{ primInsertMetric(primId, m_frame[frmNearest].w) };

	//std::queue<std::pair<int, float>> nodes{};
	auto cmp = [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
		return a.second > b.second; // 1st - lowest
	};
	std::priority_queue<std::pair<int, float>, std::vector<std::pair<int, float>>, decltype(cmp)> nodes(cmp);

	nodes.push({0, AABB::bbUnion(m_nodes[0].bb, prim.bb).area() - m_nodes[0].bb.area()});

	while (!nodes.empty()) {
		auto nodeCost = nodes.top();
		nodes.pop();

		int x{ nodeCost.first };
		float cost{ nodeCost.second };

		BVHNode& node{ m_nodes[x] };

		if (cost - std::numeric_limits<float>::epsilon() >= bestCost)
			break;
		
		if (node.leftCntPar.y) {
			bestLeaf = x;
			bestCost = cost;
			continue;
		}

		int l{ node.leftCntPar.x };
		float lCost{ cost };
		if (int cnt = m_nodes[l].leftCntPar.y; cnt)
			lCost += (cnt + 1) * AABB::bbUnion(m_nodes[l].bb, prim.bb).area() - cnt * m_nodes[l].bb.area();
		else
			lCost += AABB::bbUnion(m_nodes[l].bb, prim.bb).area() - m_nodes[l].bb.area();
		nodes.push({ l, lCost });
		//nodes.push({ l, cost + AABB::bbUnion(m_nodes[l].bb, prim.bb).area() - m_nodes[l].bb.area() });


		int r{ node.leftCntPar.x + 1 };
		float rCost{ cost };
		if (int cnt = m_nodes[r].leftCntPar.y; cnt)
			rCost += (cnt + 1) * AABB::bbUnion(m_nodes[r].bb, prim.bb).area() - cnt * m_nodes[r].bb.area();
		else
			rCost += AABB::bbUnion(m_nodes[r].bb, prim.bb).area() - m_nodes[r].bb.area();
		nodes.push({ r, rCost });
		//nodes.push({ r, cost + AABB::bbUnion(m_nodes[r].bb, prim.bb).area() - m_nodes[r].bb.area() });
	}

	return bestLeaf;
}

//void BVH::subdivideStohIntel(int nodeId) {
//	BVHNode& node{ m_nodes[nodeId] };
//	updateNodeBounds(nodeId);
//
//	int nPrims{ node.leftCntPar.y };
//	if (nPrims <= 2) {
//		// init leaf
//		auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
//		for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
//			//(*it).w = nodeId;
//			m_frame[(*it).z].z = i;
//			m_frame[(*it).z].w = nodeId;
//		}
//
//		++m_leafsCnt;
//		updateDepths(nodeId);
//		return;
//	}
//
//	AABB bbCtrs{};
//	for (int i{}; i < node.leftCntPar.y; ++i) {
//		bbCtrs.grow(m_prims[m_primMortonFrmLeaf[node.leftCntPar.x + i].x].ctr);
//	}
//
//	int dim{ bbCtrs.extentMax() };
//
//	int mid{ (node.leftCntPar.x + node.leftCntPar.y) / 2 };
//	if (comp(bbCtrs.bmin, dim) == comp(bbCtrs.bmax, dim)) {
//		// init leaf
//		auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
//		for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
//			//(*it).w = nodeId;
//			m_frame[(*it).z].z = i;
//			m_frame[(*it).z].w = nodeId;
//		}
//
//		++m_leafsCnt;
//		updateDepths(nodeId);
//		return;
//	}
//
//	// partition prims based on binned sah
//	std::vector<std::vector<float>> binsCnt(3);
//	std::vector<std::vector<AABB>> binsBBs(3);
//	std::vector<std::vector<float>> binsCosts(3);
//	for (int i{}; i < 3; ++i) {
//		binsCnt[i] = std::vector<float>(m_sahSteps);
//		binsBBs[i] = std::vector<AABB>(m_sahSteps);
//		binsCosts[i] = std::vector<float>(m_sahSteps);
//	}
//
//	std::vector<bool> skipDim(3);
//	for (int i{}; i < 3; ++i)
//		skipDim[i] = comp(bbCtrs.bmin, i) == comp(bbCtrs.bmax, i);
//
//	for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i) {
//		Vector4 offset{ bbCtrs.relateVecPos(m_prims[m_primMortonFrmLeaf[i].x].ctr) };
//		for (int a{}; a < 3; ++a) {
//			if (skipDim[a]) continue;
//			int b{ static_cast<int>(m_sahSteps * comp(offset, a)) };
//			if (b == m_sahSteps) --b;
//			assert(b >= 0);
//			assert(b < m_sahSteps);
//			++binsCnt[a][b];
//			binsBBs[a][b] = AABB::bbUnion(binsBBs[a][b], m_prims[m_primMortonFrmLeaf[i].x].bb);
//		}
//	}
//
//	// compute costs for splitting after each bucket
//	for (int a{}; a < 3; ++a) {
//		if (skipDim[a]) continue;
//		for (int i{}; i < m_sahSteps - 1; ++i) {
//			AABB b0{}, b1{};
//			float cnt0{}, cnt1{};
//			for (int j{}; j <= i; ++j) {
//				b0 = AABB::bbUnion(b0, binsBBs[a][j]);
//				cnt0 += binsCnt[a][j];
//			}
//			for (int j{ i + 1 }; j < m_sahSteps; ++j) {
//				b1 = AABB::bbUnion(b1, binsBBs[a][j]);
//				cnt1 += binsCnt[a][j];
//			}
//			if (cnt0 == 0.f || cnt1 == 0.f) {
//				binsCosts[a][i] = std::numeric_limits<float>::max();
//				continue;
//			}
//			binsCosts[a][i] = 1 + (cnt0 * b0.area() + cnt1 * b1.area()) / node.bb.area();
//		}
//	}
//
//	// find bucket to split at that min SAH metric
//	float minCost{ std::numeric_limits<float>::max() };
//	int minCostDim{}, minCostSplitBucket{};
//	for (int a{}; a < 3; ++a) {
//		if (skipDim[a]) continue;
//		for (int i{}; i < m_sahSteps - 1; ++i) {
//			if (binsCosts[a][i] < minCost) {
//				minCost = binsCosts[a][i];
//				minCostDim = a;
//				minCostSplitBucket = i;
//			}
//		}
//	}
//
//	assert(minCost != std::numeric_limits<float>::max());
//
//	// either create leaf or split prims at selected
//	float leafCost{ static_cast<float>(nPrims) };
//	if (nPrims > 2 /*max prims per leaf*/ || minCost < leafCost) {
//		XMUINT4* pmid = std::partition(
//			&m_primMortonFrmLeaf[node.leftCntPar.x],
//			&m_primMortonFrmLeaf[node.leftCntPar.x + node.leftCntPar.y - 1] + 1,
//			[=](const XMUINT4& pi) {
//				int b = m_sahSteps * comp(bbCtrs.relateVecPos(m_prims[pi.x].ctr), minCostDim);
//				if (b == m_sahSteps) --b;
//				assert(b >= 0);
//				assert(b < m_sahSteps);
//				return b <= minCostSplitBucket;
//			}
//		);
//		mid = pmid - &m_primMortonFrmLeaf[0];
//	}
//	else {
//		// init leaf
//		auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
//		for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
//			//(*it).w = nodeId;
//			m_frame[(*it).z].z = i;
//			m_frame[(*it).z].w = nodeId;
//		}
//
//		++m_leafsCnt;
//		updateDepths(nodeId);
//		return;
//	}
//
//	int leftId{ m_nodesUsed++ };
//	m_nodes[leftId].leftCntPar = {
//		node.leftCntPar.x, mid - node.leftCntPar.x, nodeId, 0
//	};
//
//	int rightId{ m_nodesUsed++ };
//	m_nodes[rightId].leftCntPar = {
//		mid, node.leftCntPar.x + node.leftCntPar.y - mid, nodeId, 0
//	};
//
//	node.leftCntPar = {
//		leftId, 0, node.leftCntPar.z, 0
//	};
//
//	subdivideStohIntel(leftId);
//	subdivideStohIntel(rightId);
//}

void BVH::subdivideStohIntelQueue(int rootId) {
	std::queue<int> nodes{};
	nodes.push(rootId);

	while (!nodes.empty()) {
		int nodeId{ nodes.front() };
		nodes.pop();

		BVHNode& node{ m_nodes[nodeId] };
		updateNodeBounds(nodeId);

		int nPrims{ node.leftCntPar.y };
		// <= m_primsPerLeaf or == 1 ? TODO
		if (nPrims <= m_primsPerLeaf) {
			// init leaf
			auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
			for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
				//(*it).w = nodeId;
				m_frame[(*it).z].z = i;
				m_frame[(*it).z].w = nodeId;
			}

			++m_leafsCnt;
			updateDepths(nodeId);
			continue;
		}

		AABB bbCtrs{};
		for (int i{}; i < node.leftCntPar.y; ++i) {
			bbCtrs.grow(m_prims[m_primMortonFrmLeaf[node.leftCntPar.x + i].x].ctr);
		}

		int dim{ bbCtrs.extentMax() };

		int mid{ (node.leftCntPar.x + node.leftCntPar.y) / 2 };
		if (comp(bbCtrs.bmin, dim) == comp(bbCtrs.bmax, dim)) {
			// init leaf
			auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
			for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
				//(*it).w = nodeId;
				m_frame[(*it).z].z = i;
				m_frame[(*it).z].w = nodeId;
			}

			++m_leafsCnt;
			updateDepths(nodeId);
			continue;
		}

		// partition prims based on binned sah
		std::vector<std::vector<float>> binsCnt(3);
		std::vector<std::vector<AABB>> binsBBs(3);
		std::vector<std::vector<float>> binsCosts(3);
		for (int i{}; i < 3; ++i) {
			binsCnt[i] = std::vector<float>(m_sahSteps);
			binsBBs[i] = std::vector<AABB>(m_sahSteps);
			binsCosts[i] = std::vector<float>(m_sahSteps);
		}

		std::vector<bool> skipDim(3);
		for (int i{}; i < 3; ++i)
			skipDim[i] = comp(bbCtrs.bmin, i) == comp(bbCtrs.bmax, i);

		for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i) {
			Vector4 offset{ bbCtrs.relateVecPos(m_prims[m_primMortonFrmLeaf[i].x].ctr) };
			for (int a{}; a < 3; ++a) {
				if (skipDim[a]) continue;
				int b{ static_cast<int>(m_sahSteps * comp(offset, a)) };
				if (b == m_sahSteps) --b;
				assert(b >= 0);
				assert(b < m_sahSteps);
				++binsCnt[a][b];
				binsBBs[a][b] = AABB::bbUnion(binsBBs[a][b], m_prims[m_primMortonFrmLeaf[i].x].bb);
			}
		}

		// compute costs for splitting after each bucket
		for (int a{}; a < 3; ++a) {
			if (skipDim[a]) continue;
			for (int i{}; i < m_sahSteps - 1; ++i) {
				AABB b0{}, b1{};
				float cnt0{}, cnt1{};
				for (int j{}; j <= i; ++j) {
					b0 = AABB::bbUnion(b0, binsBBs[a][j]);
					cnt0 += binsCnt[a][j];
				}
				for (int j{ i + 1 }; j < m_sahSteps; ++j) {
					b1 = AABB::bbUnion(b1, binsBBs[a][j]);
					cnt1 += binsCnt[a][j];
				}
				if (cnt0 == 0.f || cnt1 == 0.f) {
					binsCosts[a][i] = std::numeric_limits<float>::max();
					continue;
				}
				binsCosts[a][i] = 1 + (cnt0 * b0.area() + cnt1 * b1.area()) / node.bb.area();
			}
		}

		// find bucket to split at that min SAH metric
		float minCost{ std::numeric_limits<float>::max() };
		int minCostDim{}, minCostSplitBucket{};
		for (int a{}; a < 3; ++a) {
			if (skipDim[a]) continue;
			for (int i{}; i < m_sahSteps - 1; ++i) {
				if (binsCosts[a][i] < minCost) {
					minCost = binsCosts[a][i];
					minCostDim = a;
					minCostSplitBucket = i;
				}
			}
		}

		assert(minCost != std::numeric_limits<float>::max());

		// either create leaf or split prims at selected
		float leafCost{ static_cast<float>(nPrims) };
		if (nPrims > m_primsPerLeaf /*max prims per leaf*/ || minCost < leafCost) {
			XMUINT4* pmid = std::partition(
				&m_primMortonFrmLeaf[node.leftCntPar.x],
				&m_primMortonFrmLeaf[node.leftCntPar.x + node.leftCntPar.y - 1] + 1,
				[=](const XMUINT4& pi) {
					Vector4 relateVecPos{ bbCtrs.relateVecPos(m_prims[pi.x].ctr) };
					int b = m_sahSteps * comp(relateVecPos, minCostDim);
					if (b == m_sahSteps) --b;
					assert(b >= 0);
					assert(b < m_sahSteps);
					return b <= minCostSplitBucket;
				}
			);
			mid = pmid - &m_primMortonFrmLeaf[0];
		}
		else {
			// init leaf
			auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
			for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
				//(*it).w = nodeId;
				m_frame[(*it).z].z = i;
				m_frame[(*it).z].w = nodeId;
			}

			++m_leafsCnt;
			updateDepths(nodeId);
			continue;
		}

		int leftId{ m_nodesUsed++ };
		m_nodes[leftId].leftCntPar = {
			node.leftCntPar.x, mid - node.leftCntPar.x, nodeId, 0
		};

		int rightId{ m_nodesUsed++ };
		m_nodes[rightId].leftCntPar = {
			mid, node.leftCntPar.x + node.leftCntPar.y - mid, nodeId, 0
		};

		node.leftCntPar = {
			leftId, 0, node.leftCntPar.z, 0
		};

		nodes.push(leftId);
		nodes.push(rightId);
	}
}

//void BVH::subdivideStoh(INT nodeId) {
//	BVHNode& node{ m_nodes[nodeId] };
//	if (node.leftCntPar.y < 2) {
//		auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
//		for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
//			//(*it).w = nodeId;
//			m_frame[(*it).z].z = i;
//			m_frame[(*it).z].w = nodeId;
//		}
//
//		++m_leafsCnt;
//		updateDepths(nodeId);
//		return;
//	}
//
//	// determine split axis and position
//	int axis{}, lCnt{}, rCnt{};
//	float splitPos{};
//	float cost{};
//	cost = splitBinnedSAHStoh(node, axis, splitPos, lCnt, rCnt);
//
//	if (m_algBuild != 0 && cost >= node.bb.area() * node.leftCntPar.y) {
//		auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
//		for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
//			//(*it).w = nodeId;
//			m_frame[(*it).z].z = i;
//			m_frame[(*it).z].w = nodeId;
//		}
//
//		++m_leafsCnt;
//		updateDepths(nodeId);
//		return;
//	}
//
//	// in-place partition
//	for (auto l = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x),
//		r = std::next(l, node.leftCntPar.y - 1); l != r;)
//	{
//		if (splitPos <= comp(m_prims[(*l).x].ctr, axis)) {
//			std::swap((*l), (*r--));
//		}
//		else l++;
//	}
//
//	// create child nodes
//	int leftIdx{ m_nodesUsed++ };
//	m_nodes[leftIdx].leftCntPar = {
//		node.leftCntPar.x, lCnt, nodeId, 0
//	};
//	updateNodeBoundsStoh(leftIdx);
//
//	int rightIdx{ m_nodesUsed++ };
//	m_nodes[rightIdx].leftCntPar = {
//		node.leftCntPar.x + lCnt, rCnt, nodeId, 0
//	};
//	updateNodeBoundsStoh(rightIdx);
//
//	node.leftCntPar = {
//		leftIdx, 0, node.leftCntPar.z, 0
//	};
//
//	// recurse
//	subdivideStoh(leftIdx);
//	subdivideStoh(rightIdx);
//}

void BVH::subdivideStohQueue(INT rootId) {
	std::queue<int> nodes{};
	nodes.push(rootId);

	while (!nodes.empty()) {
		int nodeId{ nodes.front()};
		nodes.pop();

		BVHNode& node{ m_nodes[nodeId] };

		if (node.leftCntPar.y <= m_primsPerLeaf) {
			auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
			for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
				//(*it).w = nodeId;
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
		cost = splitSBVH(node, axis, splitPos, lCnt, rCnt);

		//if (m_algBuild != 0 && cost >= node.bb.area() * node.leftCntPar.y) {
		if (m_algBuild != 0 && cost >= node.leftCntPar.y) {
			auto it = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
			for (int i{ node.leftCntPar.x }; i < node.leftCntPar.x + node.leftCntPar.y; ++i, ++it) {
				//(*it).w = nodeId;
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
	if (node.leftCntPar.y < m_primsPerLeaf) {
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

		std::vector<AABB> bounds(m_sahSteps);
		std::vector<int> primsCnt(m_sahSteps);

		float step = m_sahSteps / (bmax - bmin);

		auto start = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
		auto end = std::next(start, node.leftCntPar.y);
		for (auto it = start; it != end; ++it) {
			Primitive& t = m_prims[(*it).x];
			int id{ std::min(
				m_sahSteps - 1,
				static_cast<int>((comp(t.ctr, a) - bmin) * step)
			) };
			id = std::max<int>(0, id);
			++primsCnt[id];
			bounds[id].grow(t.v0);
			bounds[id].grow(t.v1);
			bounds[id].grow(t.v2);
		}

		std::vector<float> lArea(m_sahSteps - 1);
		std::vector<float> rArea(m_sahSteps - 1);
		std::vector<int> lCnt(m_sahSteps - 1);
		std::vector<int> rCnt(m_sahSteps - 1);
		AABB lBox{}, rBox{};
		int lSum{}, rSum{};

		for (int i{}; i < m_sahSteps - 1; ++i) {
			lSum += primsCnt[i];
			lCnt[i] = lSum;
			lBox.grow(bounds[i]);
			lArea[i] = lBox.area();

			rSum += primsCnt[m_sahSteps - 1 - i];
			rCnt[m_sahSteps - 2 - i] = rSum;
			rBox.grow(bounds[m_sahSteps - 1 - i]);
			rArea[m_sahSteps - 2 - i] = rBox.area();
		}
		step = (bmax - bmin) / m_sahSteps;
		for (int i{}; i < m_sahSteps - 1; ++i) {
			//float planeCost{ lCnt[i] * lArea[i] + rCnt[i] * rArea[i] };
			float planeCost{ 1.f + (lCnt[i] * lArea[i] + rCnt[i] * rArea[i]) / node.bb.area() };
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

float BVH::splitSBVH(BVHNode& node, int& axis, float& splitPos, int& leftCnt, int& rightCnt) {
	float bestCost{ std::numeric_limits<float>::max() };
	for (int a{}; a < 3; ++a) {
		float bmin{ comp(node.bb.bmin, a) };
		float bmax{ comp(node.bb.bmax, a) };
		if (bmin == bmax)
			continue;

		std::vector<AABB> bounds(m_sahSteps);
		std::vector<int> primsCnt(m_sahSteps);

		float step = (bmax - bmin) / m_sahSteps;

		std::vector<AABB> binsBounds(m_sahSteps);
		binsBounds[0] = node.bb;
		comp(binsBounds[0].bmax, a) = bmin + step;
		for (int i{ 1 }; i < m_sahSteps; ++i) {
			binsBounds[i] = node.bb;

			comp(binsBounds[i].bmin, a) = comp(binsBounds[i - 1].bmax, a) + std::numeric_limits<float>::epsilon();
			comp(binsBounds[i].bmax, a) = comp(binsBounds[i].bmin, a) + step;
		}

		auto start = std::next(m_primMortonFrmLeaf.begin(), node.leftCntPar.x);
		auto end = std::next(start, node.leftCntPar.y);
		for (auto it = start; it != end; ++it) {
			Primitive& t = m_prims[(*it).x];
			int id{ std::min(
				m_sahSteps - 1,
				static_cast<int>((comp(t.ctr, a) - bmin) / step)
			) };
			id = std::max<int>(0, id);
			++primsCnt[id];
			bounds[id].grow(t.v0);
			bounds[id].grow(t.v1);
			bounds[id].grow(t.v2);
		}

		std::vector<float> lArea(m_sahSteps - 1);
		std::vector<float> rArea(m_sahSteps - 1);
		std::vector<int> lCnt(m_sahSteps - 1);
		std::vector<int> rCnt(m_sahSteps - 1);
		AABB lBox{}, rBox{};
		int lSum{}, rSum{};

		for (int i{}; i < m_sahSteps - 1; ++i) {
			lSum += primsCnt[i];
			lCnt[i] = lSum;
			lBox.grow(bounds[i]);
			lArea[i] = lBox.area();

			rSum += primsCnt[m_sahSteps - 1 - i];
			rCnt[m_sahSteps - 2 - i] = rSum;
			rBox.grow(bounds[m_sahSteps - 1 - i]);
			rArea[m_sahSteps - 2 - i] = rBox.area();
		}
		step = (bmax - bmin) / m_sahSteps;
		for (int i{}; i < m_sahSteps - 1; ++i) {
			//float planeCost{ lCnt[i] * lArea[i] + rCnt[i] * rArea[i] };
			float planeCost{ 1.f + (lCnt[i] * lArea[i] + rCnt[i] * rArea[i]) / node.bb.area() };
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

float& BVH::comp(Vector4& v, INT idx) {
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
	Vector4 mid{ node.bb.bmin + e / 2.f };
	splitPos = comp(mid, axis);
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
			id = std::max<int>(0, id);
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

float BVH::costSAH(int nodeId) {
	/*sah*/
	//BVHNode& node{ m_nodes[nodeId] };

	//if (node.leftCntPar.y)
	//	return node.leftCntPar.y;

	//if (m_algBuild != 5 && !m_toQBVH) {
	//	int l{ node.leftCntPar.x }, r{ l + 1 };
	//	return 1.f + (
	//		m_nodes[l].bb.area() * costSAH(l) +
	//		m_nodes[r].bb.area() * costSAH(r)
	//		) / node.bb.area();
	//}

	//float childSah{};
	//for (int i{}; i < node.leftCntPar.w; ++i) {
	//	int child{ node.leftCntPar.x + i };
	//	childSah += m_nodes[child].bb.area() * costSAH(child);
	//}
	//return 1.f + childSah / node.bb.area();


	/*sa2*/
	double cost{};
	if (m_algBuild != 5 && !m_toQBVH) {
		postForEach(nodeId, [&](int n) {
			BVHNode& node{ m_nodes[n] };
			cost += node.bb.area() * std::max<int>(1, node.leftCntPar.y);
		});
	}
	else {
		preForEachQuad(nodeId, [&](int n) {
			BVHNode& node{ m_nodes[n] };
			cost += node.bb.area() * std::max<int>(1, node.leftCntPar.y);
		});
	}
	return static_cast<float>(cost / m_nodes[nodeId].bb.area());

	/*my*/
	//BVHNode& root = m_nodes[0];
	//float sum{};

	//preForEach(root.leftCntPar.x, [&](int nodeId) {
	//	BVHNode& node = m_nodes[nodeId];
	//	sum += node.bb.area() * std::max(1, node.leftCntPar.y);
	//	});
	//preForEach(root.leftCntPar.x + 1, [&](int id) {
	//	BVHNode& n = m_nodes[id];
	//	sum += n.bb.area() * std::max(1, n.leftCntPar.y);
	//	});

	//return sum / root.bb.area();
}
