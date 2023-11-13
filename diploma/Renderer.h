#pragma once

#include "framework.h"

#include <chrono>

#include "Camera.h"
#include "InputHandler.h"
#include "PostProcess.h"

class Camera;
class InputHandler;
class PostProcess;

class Renderer {
	struct SceneBuffer {
		DirectX::SimpleMath::Matrix vp{};
		DirectX::SimpleMath::Vector4 cameraPos{};
	};

	struct RTBuffer {
		DirectX::SimpleMath::Vector4 whnf{};
		DirectX::SimpleMath::Matrix pvInv{};
		DirectX::XMINT4 instancesIntsecalgLeafsTCheck{ 2, 0, 0, 1 };
		DirectX::SimpleMath::Vector4 camDir{};
	};
	RTBuffer m_rtBuffer{};
	ID3D11Buffer* m_pRTBuffer{};

	ID3D11Device* m_pDevice{};
	ID3D11DeviceContext* m_pDeviceContext{};

	IDXGISwapChain* m_pSwapChain{};
	ID3D11RenderTargetView* m_pBackBufferRTV{};

	Camera* m_pCamera{};

	PostProcess* m_pPostProcess{};

	float m_cubeAngleRotation{};

	ID3D11Buffer* m_pSceneBuffer{};
	ID3D11RasterizerState* m_pRasterizerState{};
	ID3D11SamplerState* m_pSampler{};

	ID3D11Texture2D* m_pDepthBuffer{};
	ID3D11DepthStencilView* m_pDepthBufferDSV{};
	bool m_isUseZBuffer{  };

	ID3D11DepthStencilState* m_pDepthState{};
	ID3D11DepthStencilState* m_pTransDepthState{};

	ID3D11BlendState* m_pTransBlendState{};
	ID3D11BlendState* m_pOpaqueBlendState{};

	UINT m_width{ 16 };
	UINT m_height{ 16 };

	bool m_isModelRotate{ false };

	size_t m_prevTime{};

	SceneBuffer m_sceneBuffer{};

	bool m_isShowLights{ true };
	bool m_isUseNormalMaps{ true };
	bool m_isShowNormals{};
	bool m_isUseAmbient{ true };
	bool m_useSepia{};
	bool m_isShowCubemap{ true };

	DirectX::SimpleMath::Matrix m_v{};
	DirectX::SimpleMath::Matrix m_p{};

	//CPUTimer m_CPUTimer{};
	UINT m_frameCounter{};
	double m_fps{};
	double m_bvhTime{};
	double m_bvhTimeAvg{};
	double m_cubeTime{};
	double m_cubeTimeAvg{};

	float m_near{ 0.1f };
	float m_far{ 100.f };
	float m_fov{ DirectX::XM_PI / 3.f };
	float m_aspectRatio{
		static_cast<float>(m_height) / m_width
	};

public:
	InputHandler* m_pInputHandler{};

	bool init(HWND hWnd);
	void term();

	bool resize(UINT width, UINT height);
	bool update();
	bool render();

	void switchRotation() {
		m_isModelRotate = !m_isModelRotate;
	}

private:
	IDXGIAdapter* selectIDXGIAdapter(IDXGIFactory* factory);
	HRESULT createDeviceAndSwapChain(HWND hWnd, IDXGIAdapter* adapter);

	HRESULT setupBackBuffer();
	HRESULT createDepthBuffer();

	HRESULT initScene();
	void termScene();

	HRESULT createSceneBuffer();
	HRESULT createRasterizerState();
	HRESULT createReversedDepthState();
	HRESULT createSampler();
};
