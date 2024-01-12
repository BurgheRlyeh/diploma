#pragma once

#include "framework.h"

#include "AABB.h"

#define LIMIT_V 1013
#define LIMIT_I 1107

using namespace DirectX;
using namespace DirectX::SimpleMath;

class BVHRenderer {
	static const int MaxInst{ 2 * LIMIT_I - 1 };

	struct ModelBuffer {
		AABB bb{ {}, { 1.f, 1.f, 1.f, 0.f } };
		DirectX::SimpleMath::Color cl{ 0.f, 0.f, 0.f, 0.f };
	};

	ID3D11Device* m_pDevice{};
	ID3D11DeviceContext* m_pDeviceContext{};

	ID3D11Buffer* m_pVertexBuffer{};
	ID3D11Buffer* m_pIndexBuffer{};

	std::vector<ModelBuffer> m_modelBuffers{};
	ID3D11Buffer* m_pModelBuffer{};
	ID3D11ShaderResourceView* m_pModelBufferSRV{};

	ID3D11VertexShader* m_pVertexShader{};
	ID3D11PixelShader* m_pPixelShader{};
	ID3D11InputLayout* m_pInputLayout{};

	bool isUpd{ true };

public:
	BVHRenderer() = delete;
	BVHRenderer(
		ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext
	) : m_pDevice(pDevice), m_pDeviceContext(pDeviceContext) {};

	HRESULT init() {
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

	void term() {
		SAFE_RELEASE(m_pModelBuffer);
		SAFE_RELEASE(m_pVertexShader);
		SAFE_RELEASE(m_pPixelShader);
		SAFE_RELEASE(m_pInputLayout);
		SAFE_RELEASE(m_pIndexBuffer);
		SAFE_RELEASE(m_pVertexBuffer);
	}

	void reset() {
		m_modelBuffers.clear();
		isUpd = true;
	}

	void add(
		AABB aabb = {
			{ 0.f, 0.f, 0.f, 0.f },
			{ 1.f, 1.f, 1.f, 0.f }
		},
		Color cl = { 1.f, 0.f, 0.f, 0.f }
	) {
		m_modelBuffers.push_back({ aabb, cl });
		isUpd = true;
	}

	void update() {
		if (!isUpd) return;

		m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, m_modelBuffers.data(), 0, 0);
	}

	void render(ID3D11SamplerState* pSampler, ID3D11Buffer* pSceneBuffer) {
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
};