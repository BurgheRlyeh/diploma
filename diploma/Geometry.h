#pragma once

#include "framework.h"

#define NOMINMAX
#include <limits>

#include "Camera.h"
#include "AABB.h"
#include "Timer.h"
#include "BVH.h"

class Renderer;
class Camera;
struct AABB;
class CPUTimer;
class GPUTimer;
class BVH;

#define LIMIT_V 1013
#define LIMIT_I 1107

class Geometry {
	ID3D11Device* m_pDevice{};
	ID3D11DeviceContext* m_pDeviceContext{};

	// index buffer
	DirectX::XMINT4* indices{};
	UINT idsCnt{};
	ID3D11Buffer* m_pIdsConstBuffer{};

	// vertices
	DirectX::SimpleMath::Vector4* vertices{};
	UINT vtsCnt{};
	ID3D11Buffer* m_pVtsConstBuffer{};

	// model buffer
	struct ModelBuffer {
		DirectX::SimpleMath::Matrix mModel{};
		DirectX::SimpleMath::Matrix mModelInv{};

		void updateMatrices();
	};
	ModelBuffer m_modelBuffer{};
	ID3D11Buffer* m_pModelBuffer{};

	ID3D11Buffer* m_pBVHBuffer{};
	ID3D11ShaderResourceView* m_pBVHBufferSRV{};

	ID3D11Buffer* m_pTriIdsBuffer{};

	ID3D11ComputeShader* m_pRayTracingCS{};

	ID3D11UnorderedAccessView* m_pUAVTexture{};

public:
	BVH bvh{};

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
};
