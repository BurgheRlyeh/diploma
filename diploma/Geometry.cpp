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
	//mModel =
	//	Matrix::CreateScale(3.f) * 
	//	Matrix::CreateRotationX(-XM_PIDIV2) * 
	//	Matrix::CreateRotationY(posAngle.w) * 
	//	Matrix::CreateTranslation({ posAngle.x, posAngle.y, posAngle.z });
	mModelInv = mModel.Invert();
}

HRESULT Geometry::init(ID3D11Texture2D* tex) {
	HRESULT hr{ S_OK };

	// upload geometry
	{
		// main - 11715
		CSVGeometryLoader geom = CSVGeometryLoader::loadFrom("11715.csv");

		indices = new XMINT4[idsCnt = geom.indices.size()];
		std::copy(geom.indices.begin(), geom.indices.end(), indices);

		vertices = new Vector4[vtsCnt = geom.vertices.size()];
		std::copy(geom.vertices.begin(), geom.vertices.end(), vertices);
	}

	// create indices constant buffer
	{
		D3D11_BUFFER_DESC desc{
			.ByteWidth{ idsCnt * sizeof(XMINT4) },
			.Usage{ D3D11_USAGE_DEFAULT },
			.BindFlags{ D3D11_BIND_CONSTANT_BUFFER },
			.StructureByteStride{ sizeof(XMINT4) }
		};

		D3D11_SUBRESOURCE_DATA data{ indices, idsCnt * sizeof(XMINT4) };

		hr = m_pDevice->CreateBuffer(&desc, &data, &m_pIdsConstBuffer);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pIdsConstBuffer, "IndexConstBuffer");
		THROW_IF_FAILED(hr);
	}

	// create vertices constant buffer
	{
		D3D11_BUFFER_DESC desc{
			.ByteWidth{ vtsCnt * sizeof(Vector4) },
			.Usage{ D3D11_USAGE_DEFAULT },
			.BindFlags{ D3D11_BIND_CONSTANT_BUFFER },
			.StructureByteStride{ sizeof(Vector4) }
		};

		D3D11_SUBRESOURCE_DATA data{ vertices, vtsCnt * sizeof(Vector4) };

		hr = m_pDevice->CreateBuffer(&desc, &data, &m_pVtsConstBuffer);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pVtsConstBuffer, "VertexConstBuffer");
		THROW_IF_FAILED(hr);
	}

	// create model buffer
	{
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

	// bvh structured buffer
	{
		D3D11_BUFFER_DESC desc{
			.ByteWidth{ (2 * idsCnt + 1) * sizeof(BVH::BVHNode) },
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
				.NumElements{ 2 * idsCnt + 1 }
			}
		};

		hr = m_pDevice->CreateShaderResourceView(m_pBVHBuffer, &descSRV, &m_pBVHBufferSRV);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pBVHBufferSRV, "BVHBufferSRV");
		THROW_IF_FAILED(hr);
	}

	// create triangle indices buffer
	{
		D3D11_BUFFER_DESC desc{
			.ByteWidth{ idsCnt * sizeof(XMINT4) },
			.Usage{ D3D11_USAGE_DEFAULT },
			.BindFlags{ D3D11_BIND_CONSTANT_BUFFER }
		};

		hr = m_pDevice->CreateBuffer(&desc, nullptr, &m_pTriIdsBuffer);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pTriIdsBuffer, "TriIdsBuffer");
		THROW_IF_FAILED(hr);
	}

	// shader processing
	{
		ID3DBlob* pBlob{};
		//hr = loadShaderBlob(L"RayTracingCS.cso", &pBlob);
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

	// timers init
	m_pGPUTimer = new GPUTimer(m_pDevice, m_pDeviceContext);
	m_pCPUTimer = new CPUTimer();

	updateBVH();

	return hr;
}

void Geometry::term() {
	SAFE_RELEASE(m_pUAVTexture);
	SAFE_RELEASE(m_pRayTracingCS);
	SAFE_RELEASE(m_pTriIdsBuffer);
	SAFE_RELEASE(m_pBVHBufferSRV);
	SAFE_RELEASE(m_pBVHBuffer);
	SAFE_RELEASE(m_pModelBuffer);
	SAFE_RELEASE(m_pIdsConstBuffer);
	SAFE_RELEASE(m_pVtsConstBuffer);
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

	bvh.init(vertices, vtsCnt, indices, idsCnt, m_modelBuffer.mModel);
	bvh.build();

	m_pCPUTimer->stop();

	m_pDeviceContext->UpdateSubresource(m_pBVHBuffer, 0, nullptr, bvh.m_nodes.data(), 0, 0);
	m_pDeviceContext->UpdateSubresource(m_pTriIdsBuffer, 0, nullptr, bvh.m_bvhPrims.data(), 0, 0);
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
	ID3D11Buffer* constBuffers[5]{
		m_pVtsConstBuffer,
		m_pIdsConstBuffer,
		m_pModelBuffer,
		m_pTriIdsBuffer,
		m_pRTBuf
	};
	m_pDeviceContext->CSSetConstantBuffers(0, 5, constBuffers);

	// bind srv
	ID3D11ShaderResourceView* srvBuffers[]{ m_pBVHBufferSRV };
	m_pDeviceContext->CSSetShaderResources(0, 1, srvBuffers);

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
