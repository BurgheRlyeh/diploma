#include "Geometry.h"

#include "ShaderLoader.h"
#include "BVH.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "CSVGeometryLoader.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void Geometry::ModelBuffer::updateMatrices() {
	mModel =
		Matrix::CreateScale(3.1f)
		//Matrix::CreateScale(3.f)
		//* Matrix::CreateRotationX(-XM_PIDIV2)
		//* Matrix::CreateRotationY(posAngle.w)
		//* Matrix::CreateTranslation({ posAngle.x, posAngle.y, posAngle.z });
		;
	mModelInv = mModel.Invert();
}

HRESULT Geometry::init(ID3D11Texture2D* tex) {
	HRESULT hr{ S_OK };

	// upload geometry
	{
		// main - cube 11715 sponza sponzastructure grass
		CSVGeometryLoader::loadFrom("grass.csv", &m_indices, &m_vertices);
	}

	// create indices buffer
	{
		D3D11_BUFFER_DESC desc{
			.ByteWidth{ static_cast<UINT>(m_indices.size() * sizeof(XMINT4)) },
			.Usage{ D3D11_USAGE_IMMUTABLE },
			.BindFlags{ D3D11_BIND_SHADER_RESOURCE },
			.MiscFlags{ D3D11_RESOURCE_MISC_BUFFER_STRUCTURED },
			.StructureByteStride{ sizeof(XMINT4) }
		};

		D3D11_SUBRESOURCE_DATA data{ m_indices.data(), m_indices.size() * sizeof(XMINT4)};

		hr = m_pDevice->CreateBuffer(&desc, &data, &m_pIndexBuffer);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pIndexBuffer, "IndexBuffer");
		THROW_IF_FAILED(hr);

		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV{
			.Format{ DXGI_FORMAT_UNKNOWN },
			.ViewDimension{ D3D11_SRV_DIMENSION_BUFFER },
			.Buffer{
				.FirstElement{ 0 },
				.NumElements{ static_cast<UINT>(m_indices.size()) }
			}
		};

		hr = m_pDevice->CreateShaderResourceView(m_pIndexBuffer, &descSRV, &m_pIndexBufferSRV);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pIndexBufferSRV, "IndexBufferSRV");
		THROW_IF_FAILED(hr);
	}

	// create vertices buffer
	{
		D3D11_BUFFER_DESC desc{
			.ByteWidth{ static_cast<UINT>(m_vertices.size() * sizeof(Vector4)) },
			.Usage{ D3D11_USAGE_IMMUTABLE },
			.BindFlags{ D3D11_BIND_SHADER_RESOURCE },
			.MiscFlags{ D3D11_RESOURCE_MISC_BUFFER_STRUCTURED },
			.StructureByteStride{ sizeof(Vector4) }
		};

		D3D11_SUBRESOURCE_DATA data{ m_vertices.data(), m_vertices.size() * sizeof(Vector4)};

		hr = m_pDevice->CreateBuffer(&desc, &data, &m_pVertexBuffer);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pVertexBuffer, "VertexConstBuffer");
		THROW_IF_FAILED(hr);

		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV{
			.Format{ DXGI_FORMAT_UNKNOWN },
			.ViewDimension{ D3D11_SRV_DIMENSION_BUFFER },
			.Buffer{
				.FirstElement{ 0 },
				.NumElements{ static_cast<UINT>(m_vertices.size()) }
			}
		};

		hr = m_pDevice->CreateShaderResourceView(m_pVertexBuffer, &descSRV, &m_pVertexBufferSRV);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pVertexBufferSRV, "VertexBufferSRV");
		THROW_IF_FAILED(hr);
	}

	// create model const buffer
	{
		m_modelBuffer.primsCnt.x = m_indices.size();
		m_modelBuffer.updateMatrices();

		D3D11_BUFFER_DESC desc{
			.ByteWidth{ sizeof(ModelBuffer) },
			.Usage{ D3D11_USAGE_DEFAULT },
			.BindFlags{ D3D11_BIND_CONSTANT_BUFFER }
		};

		D3D11_SUBRESOURCE_DATA data{ &m_modelBuffer, sizeof(m_modelBuffer) };

		hr = m_pDevice->CreateBuffer(&desc, &data, &m_pModelBuffer);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pModelBuffer, "ModelBuffer");
		THROW_IF_FAILED(hr);
	}

	// shader processing
	{
		ID3DBlob* pBlob{};
		std::wstring filepath{ L"RayTracingCS.cso" };
		hr = D3DReadFileToBlob(filepath.c_str(), &pBlob);
		THROW_IF_FAILED(hr);

		hr = m_pDevice->CreateComputeShader(
			pBlob->GetBufferPointer(),
			pBlob->GetBufferSize(),
			nullptr,
			&m_pRayTracingCS
		);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pRayTracingCS, "RayTracingComputeShader");
		THROW_IF_FAILED(hr);
	}

	resizeUAV(tex);

	m_pBVH = new BVH(m_pDevice, m_pDeviceContext, m_indices.size());

	// timers init
	m_pGPUTimer = new GPUTimer(m_pDevice, m_pDeviceContext);
	m_pCPUTimer = new CPUTimer();

	updateBVH();

	return hr;
}

void Geometry::term() {
	m_pBVH->term();

	SAFE_RELEASE(m_pUAVTexture);
	SAFE_RELEASE(m_pRayTracingCS);
	SAFE_RELEASE(m_pModelBuffer);
	SAFE_RELEASE(m_pIndexBuffer);
	SAFE_RELEASE(m_pVertexBuffer);
}

void Geometry::update(float delta, bool isRotate) {
	if (!isRotate) {
		return;
	}

	m_modelBuffer.posAngle.w += delta * DirectX::XM_PIDIV4;
	m_modelBuffer.updateMatrices();

	m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &m_modelBuffer, 0, 0);

	updateBVH();
}

void Geometry::updateBVH() {
	m_pCPUTimer->start();

	m_pBVH->build(m_vertices.data(), m_vertices.size(), m_indices.data(), m_indices.size(), m_modelBuffer.mModel);

	m_pCPUTimer->stop();

	m_pBVH->updateRenderBVH();
	m_pBVH->updateBuffers();
}

void Geometry::resizeUAV(ID3D11Texture2D* tex) {
	D3D11_UNORDERED_ACCESS_VIEW_DESC desc{
		.Format{ DXGI_FORMAT_UNKNOWN },
		.ViewDimension{ D3D11_UAV_DIMENSION_TEXTURE2D },
		.Texture2D{}
	};

	THROW_IF_FAILED(m_pDevice->CreateUnorderedAccessView(tex, &desc, &m_pUAVTexture));
	THROW_IF_FAILED(setResourceName(m_pUAVTexture, "UAVTexture"));
}

void Geometry::rayTracing(ID3D11Buffer* m_pSBuf, ID3D11Buffer* m_pRTBuf, int w, int h) {
	ID3D11Buffer* constBuffers[2]{
		m_pModelBuffer,
		m_pRTBuf
	};
	m_pDeviceContext->CSSetConstantBuffers(0, 2, constBuffers);

	// bind srv
	ID3D11ShaderResourceView* srvBuffers[]{ m_pVertexBufferSRV, m_pIndexBufferSRV, m_pBVH->getPrimIdsBufferSRV(), m_pBVH->getBVHBufferSRV()};
	m_pDeviceContext->CSSetShaderResources(0, 4, srvBuffers);

	// unbind rtv
	ID3D11RenderTargetView* nullRtv{};
	m_pDeviceContext->OMSetRenderTargets(1, &nullRtv, nullptr);

	// bind uav
	ID3D11UnorderedAccessView* uavBuffers[]{ m_pUAVTexture };
	m_pDeviceContext->CSSetUnorderedAccessViews(0, 1, uavBuffers, nullptr);

	m_pDeviceContext->CSSetShader(m_pRayTracingCS, nullptr, 0);

	m_pGPUTimer->start();
	m_pDeviceContext->Dispatch(w, h, 1);
	m_pGPUTimer->stop();

	// unbind uav
	ID3D11UnorderedAccessView* nullUav{};
	m_pDeviceContext->CSSetUnorderedAccessViews(0, 1, &nullUav, nullptr);
}

void Geometry::renderBVH(ID3D11SamplerState* pSampler, ID3D11Buffer* pSceneBuffer) {
	m_pBVH->render(pSampler, pSceneBuffer);
}
