#pragma once

#include "framework.h"

#include <chrono>

#include "Camera.h"
#include "InputHandler.h"
#include "PostProcess.h"
#include "Geometry.h"

class Camera;
class InputHandler;
class PostProcess;
class Geometry;

class Renderer {
	const FLOAT m_near{ 0.1f };
	const FLOAT m_far{ 100.f };
	const FLOAT m_fov{ DirectX::XM_PI / 3.f };

	// window size
	UINT m_width{ 16 };
	UINT m_height{ 9 };

	ID3D11Device* m_pDevice{};
	ID3D11DeviceContext* m_pDeviceContext{};

	IDXGISwapChain* m_pSwapChain{};

	ID3D11RenderTargetView* m_pBackBufferRTV{};

	ID3D11Texture2D* m_pDepthBuffer{};
	ID3D11DepthStencilView* m_pDepthBufferDSV{};

	struct SceneBuffer {
		DirectX::SimpleMath::Matrix vp{};
		DirectX::SimpleMath::Vector4 cameraPos{};
	};
	SceneBuffer m_sceneBuffer{};
	ID3D11Buffer* m_pSceneBuffer{};

	struct RTBuffer {
		DirectX::SimpleMath::Vector4 whnf{
			16.f, 9.f, 0.1f, 100.f
		};
		DirectX::SimpleMath::Matrix pvInv{};
		DirectX::XMINT4 instancesIntsecalgLeafsTCheck{ 1, 0, 0, 1 };
		DirectX::SimpleMath::Vector4 camDir{};
	};
	RTBuffer m_rtBuffer{};
	ID3D11Buffer* m_pRTBuffer{};

	ID3D11RasterizerState* m_pRasterizerState{};

	ID3D11BlendState* m_pTransBlendState{};
	ID3D11BlendState* m_pOpaqueBlendState{};

	ID3D11DepthStencilState* m_pDepthState{};
	ID3D11DepthStencilState* m_pTransDepthState{};

	ID3D11SamplerState* m_pSampler{};

	// other classes
	PostProcess* m_pPostProcess{};
	Camera* m_pCamera{};
	Geometry* m_pGeom{};

	size_t m_prevTime{};

	// prifiling
	//CPUTimer m_CPUTimer{};
	UINT m_frameCounter{};
	double m_fps{};
	double m_bvhTime{};
	double m_bvhTimeAvg{};
	double m_cubeTime{};
	double m_cubeTimeAvg{};


public:
	InputHandler* m_pInputHandler{};

	void switchRotation();

	bool init(HWND hWnd);
	void term();

	bool resize(UINT width, UINT height);
	void update();
	bool render();

private:
	IDXGIAdapter* selectIDXGIAdapter(IDXGIFactory* factory);
	HRESULT createDeviceAndSwapChain(HWND hWnd, IDXGIAdapter* adapter);

	HRESULT setupBackBuffer();

	HRESULT initScene();
	void termScene();
};
