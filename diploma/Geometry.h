#pragma once

#include "framework.h"

#define NOMINMAX
#include <limits>

#include "Camera.h"
#include "AABB.h"
#include "Timer.h"
#include "BVH.h"
//#include "BVHRenderer.h"

class Renderer;
class Camera;
struct AABB;
class CPUTimer;
class GPUTimer;
class BVH;
//class BVHRenderer;

#define LIMIT_V 1013
#define LIMIT_I 1107

class Geometry {
	ID3D11Device* m_pDevice{};
	ID3D11DeviceContext* m_pDeviceContext{};

	// index buffer
	std::vector<DirectX::XMINT4> m_indices{};
	ID3D11Buffer* m_pIndexBuffer{};
	ID3D11ShaderResourceView* m_pIndexBufferSRV{};

	// vertices
	std::vector<DirectX::SimpleMath::Vector4> m_vertices{};
	ID3D11Buffer* m_pVertexBuffer{};
	ID3D11ShaderResourceView* m_pVertexBufferSRV{};

	// model buffer
	struct ModelBuffer {
		DirectX::XMINT4 primsCnt{};
		DirectX::SimpleMath::Matrix mModel{};
		DirectX::SimpleMath::Matrix mModelInv{};
		DirectX::SimpleMath::Vector4 posAngle{};

		void updateMatrices();
	};
	ModelBuffer m_modelBuffer{};
	ID3D11Buffer* m_pModelBuffer{};

	ID3D11ComputeShader* m_pRayTracingCS{};

	ID3D11UnorderedAccessView* m_pUAVTexture{};

	//BVHRenderer* m_pBVHRenderer{};

public:
	BVH* m_pBVH{};

	Geometry() = delete;
	Geometry(ID3D11Device* device, ID3D11DeviceContext* deviceContext) :
		m_pDevice(device),
		m_pDeviceContext(deviceContext) {}

	GPUTimer* m_pGPUTimer{};
	CPUTimer* m_pCPUTimer{};

	HRESULT init(ID3D11Texture2D* tex);
	void term();

	void update(float delta, bool isRotate);
	void updateBVH();

	void resizeUAV(ID3D11Texture2D* texture);
	void rayTracing(ID3D11Buffer* m_pSceneBuffer, ID3D11Buffer* m_pRTBuffer, int width, int height);

	void renderBVH(ID3D11SamplerState* pSampler, ID3D11Buffer* pSceneBuffer);
};
