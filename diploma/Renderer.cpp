#include "Renderer.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void Renderer::switchRotation() {
	m_isModelRotate = !m_isModelRotate;
}

bool Renderer::init(HWND hWnd) {
	assert(hWnd);
	HRESULT hr{ S_OK };

	IDXGIFactory* factory{};
	hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
	assert(factory);
	THROW_IF_FAILED(hr);

	IDXGIAdapter* adapter{ selectIDXGIAdapter(factory) };
	assert(adapter);

	hr = createDeviceAndSwapChain(hWnd, adapter);
	THROW_IF_FAILED(hr);

	SAFE_RELEASE(adapter);
	SAFE_RELEASE(factory);

	m_pPostProcess = new PostProcess(m_pDevice, m_pDeviceContext);
	hr = m_pPostProcess->init();
	THROW_IF_FAILED(hr);

	hr = setupBackBuffer();
	THROW_IF_FAILED(hr);

	hr = initScene();
	THROW_IF_FAILED(hr);

	m_pCamera = new Camera();
	m_pInputHandler = new InputHandler(this, m_pCamera);

	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer backends
		ImGui_ImplWin32_Init(hWnd);
		ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);
	}

	m_CPUTimer.start();
	m_prevTime = m_CPUTimer.curr();

	if (FAILED(hr)) {
		term();
	}

	return SUCCEEDED(hr);
}

IDXGIAdapter* Renderer::selectIDXGIAdapter(IDXGIFactory* factory) {
	IDXGIAdapter* adapter{};
	for (UINT idx{}; factory->EnumAdapters(idx, &adapter) != DXGI_ERROR_NOT_FOUND; ++idx) {
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		if (wcscmp(desc.Description, L"Microsoft Basic Render Driver"))
			return adapter;

		adapter->Release();
	}

	return nullptr;
}

HRESULT Renderer::createDeviceAndSwapChain(HWND hWnd, IDXGIAdapter* adapter) {
	D3D_FEATURE_LEVEL level, levels[]{ D3D_FEATURE_LEVEL_11_0 };

	UINT flags{};
#ifdef _DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // _DEBUG

	DXGI_SWAP_CHAIN_DESC swapChainDesc{
		.BufferDesc{
			.Width{ m_width },
			.Height{ m_height },
			.RefreshRate{
				.Numerator{},
				.Denominator{ 1 }
			},
			.Format{ DXGI_FORMAT_R8G8B8A8_UNORM },
		},
		.SampleDesc{
			.Count{ 1 },
			.Quality{}
		},
		.BufferUsage{ DXGI_USAGE_RENDER_TARGET_OUTPUT },
		.BufferCount{ 2 },
		.OutputWindow{ hWnd },
		.Windowed{ true },
	};

	HRESULT hr{ D3D11CreateDeviceAndSwapChain(
		adapter,
		D3D_DRIVER_TYPE_UNKNOWN,
		nullptr,
		flags,
		levels,
		1,
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&m_pSwapChain,
		&m_pDevice,
		&level,
		&m_pDeviceContext
	) };

	THROW_IF_FAILED(hr);

	return hr;
}

HRESULT Renderer::setupBackBuffer() {
	HRESULT hr{ S_OK };

	ID3D11Texture2D* backBuffer{};
	hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);
	THROW_IF_FAILED(hr);

	hr = m_pDevice->CreateRenderTargetView(backBuffer, nullptr, &m_pBackBufferRTV);
	SAFE_RELEASE(backBuffer);
	THROW_IF_FAILED(hr);

	D3D11_TEXTURE2D_DESC desc{
		.Width{ m_width },
		.Height{ m_height },
		.MipLevels{ 1 },
		.ArraySize{ 1 },
		.Format{ DXGI_FORMAT_D32_FLOAT },
		.SampleDesc{ 1, 0 },
		.Usage{ D3D11_USAGE_DEFAULT },
		.BindFlags{ D3D11_BIND_DEPTH_STENCIL },
	};
	hr = m_pDevice->CreateTexture2D(&desc, nullptr, &m_pDepthBuffer);
	THROW_IF_FAILED(hr);

	hr = setResourceName(m_pDepthBuffer, "DepthBuffer");
	THROW_IF_FAILED(hr);

	hr = m_pDevice->CreateDepthStencilView(m_pDepthBuffer, nullptr, &m_pDepthBufferDSV);
	THROW_IF_FAILED(hr);

	hr = setResourceName(m_pDepthBufferDSV, "DepthBufferView");
	THROW_IF_FAILED(hr);

	hr = m_pPostProcess->setupBuffer(m_width, m_height);
	THROW_IF_FAILED(hr);

	return hr;
}

void Renderer::term() {
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	termScene();

	SAFE_RELEASE(m_pDepthBufferDSV);
	SAFE_RELEASE(m_pDepthBuffer);
	SAFE_RELEASE(m_pBackBufferRTV);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pDeviceContext);

#ifdef _DEBUG
	if (m_pDevice) {
		ID3D11Debug* debug{};
		THROW_IF_FAILED(m_pDevice->QueryInterface(__uuidof(ID3D11Debug), (void**)&debug));
		if (debug->AddRef() != 3) {
			debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
		}
		debug->Release();
		SAFE_RELEASE(debug);
	}
#endif // _DEBUG

	SAFE_RELEASE(m_pDevice);
}

bool Renderer::resize(UINT width, UINT height) {
	if (this->m_width == width && this->m_height == height) {
		return true;
	}

	HRESULT hr{ S_OK };

	SAFE_RELEASE(m_pBackBufferRTV);
	SAFE_RELEASE(m_pDepthBuffer);
	SAFE_RELEASE(m_pDepthBufferDSV);

	hr = m_pSwapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	THROW_IF_FAILED(hr);

	m_width = width;
	m_height = height;

	hr = setupBackBuffer();
	THROW_IF_FAILED(hr);

	// rt update TODO
	m_pGeom->resizeUAV(m_pPostProcess->getTexture());

	D3D11_MAPPED_SUBRESOURCE subres;
	hr = m_pDeviceContext->Map(
		m_pRTBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subres
	);
	THROW_IF_FAILED(hr);

	m_rtBuffer.whnf.x = static_cast<FLOAT>(m_width);
	m_rtBuffer.whnf.y = static_cast<FLOAT>(m_height);
	m_rtBuffer.whnf.z = m_near;
	m_rtBuffer.whnf.w = m_far;

	memcpy(subres.pData, &m_rtBuffer, sizeof(RTBuffer));
	m_pDeviceContext->Unmap(m_pRTBuffer, 0);

	return SUCCEEDED(hr);
}

void Renderer::update() {
	double time{ m_CPUTimer.curr() };

	// move camera
	m_pCamera->updatePosition((time - m_prevTime) / 1e3f);

	// update models
	m_pGeom->update((time - m_prevTime) / 1e3f, m_isModelRotate);

	m_prevTime = time;

	Vector3 cameraPos{ m_pCamera->getPosition() };

	// Setup camera
	Matrix v{ XMMatrixLookAtLH(cameraPos, m_pCamera->getPoi(), m_pCamera->getUp()) };
	/*Matrix p{ XMMatrixPerspectiveLH(
		2 * m_far * tanf(m_fov / 2),
		2 * m_far * tanf(m_fov / 2) * m_height / m_width,
		m_far,
		m_near
	) };*/
	Matrix p{ XMMatrixPerspectiveLH(
		2 * m_near * tanf(m_fov / 2),
		2 * m_near * tanf(m_fov / 2) * m_height / m_width,
		m_near,
		m_far
	) };

	D3D11_MAPPED_SUBRESOURCE subres;

	// update scene buffer
	{
		THROW_IF_FAILED(m_pDeviceContext->Map(
			m_pSceneBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subres
		));

		m_sceneBuffer.vp = v * p;
		m_sceneBuffer.cameraPos = { cameraPos.x, cameraPos.y, cameraPos.z, 0.0f };

		memcpy(subres.pData, &m_sceneBuffer, sizeof(SceneBuffer));
		m_pDeviceContext->Unmap(m_pSceneBuffer, 0);
	}

	// ray tracing buffer update
	{
		THROW_IF_FAILED(m_pDeviceContext->Map(
			m_pRTBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subres
		));

		(v * p).Invert(m_rtBuffer.pvInv);

		// update direction vector
		Vector4 n = { 1.f / m_width, -1.f / m_height, 1.f / (m_near - m_far), 1.f };
		n = Vector4::Transform(n, m_rtBuffer.pvInv);

		Vector4 f = { 1.f / m_width, -1.f / m_height, 0.f, 1.f };
		f = Vector4::Transform(f, m_rtBuffer.pvInv);

		(f / f.w - n / n.w).Normalize(m_rtBuffer.camDir);

		m_rtBuffer.highligths = {
			m_pGeom->bvh.m_mortonPrims[m_highlights.x].primId,
			m_pGeom->bvh.m_mortonPrims[m_highlights.y].primId,
			m_pGeom->bvh.m_mortonPrims[m_highlights.z].primId,
			-1
		};

		memcpy(subres.pData, &m_rtBuffer, sizeof(RTBuffer));
		m_pDeviceContext->Unmap(m_pRTBuffer, 0);
	}
}

void Renderer::updateBVH() {
	m_pGeom->updateBVH();
}

bool Renderer::render() {
	m_pDeviceContext->ClearState();

	ID3D11RenderTargetView* views[]{ m_pPostProcess->getBufferRTV() };
	m_pDeviceContext->OMSetRenderTargets(1, views, nullptr);

	static const FLOAT BackColor[4]{ 0.6f, 0.4f, 0.4f, 1.0f };
	m_pDeviceContext->ClearRenderTargetView(m_pPostProcess->getBufferRTV(), BackColor);
	//m_pDeviceContext->ClearDepthStencilView(m_pDepthBufferDSV, D3D11_CLEAR_DEPTH, 0.0f, 0);

	D3D11_VIEWPORT viewport{
		.Width{ static_cast<FLOAT>(m_width) },
		.Height{ static_cast<FLOAT>(m_height) },
		.MaxDepth{ 1.0f }
	};
	m_pDeviceContext->RSSetViewports(1, &viewport);

	D3D11_RECT rect{
		.right{ static_cast<LONG>(m_width) },
		.bottom{ static_cast<LONG>(m_width) }
	};
	m_pDeviceContext->RSSetScissorRects(1, &rect);

	m_pDeviceContext->OMSetDepthStencilState(m_pDepthState, 0);
	m_pDeviceContext->RSSetState(m_pRasterizerState);
	m_pDeviceContext->OMSetBlendState(m_pOpaqueBlendState, nullptr, 0xFFFFFFFF);

	m_pGeom->rayTracing(m_pSceneBuffer, m_pRTBuffer, m_width, m_height);
	m_pDeviceContext->OMSetRenderTargets(1, views, nullptr);

	m_pPostProcess->render(m_pBackBufferRTV, m_pSampler);

	++m_frameCnt;
	double time{ m_CPUTimer.curr() };
	if (time - m_prevSec > 1e3) {
		m_fps = 1e3 * m_frameCnt / (time - m_prevSec);
		m_prevSec = time;

		double bvhTimeAvg{ m_pGeom->m_pCPUTimer->getAcc() };
		if (bvhTimeAvg)
			m_geomCPUAvgTime = bvhTimeAvg / m_frameCnt;
		
		
		m_geomGPUAvgTime = m_pGeom->m_pGPUTimer->getAcc() / m_frameCnt;

		m_frameCnt = 0;
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	{
		ImGui::Begin("App Statistics");

		ImGui::Text("FPS: %.1f", m_fps);

		ImGui::Text("");

		ImGui::Text("Width: %d", m_width);
		ImGui::Text("Height: %d", m_height);

		ImGui::End();
	}

	{
		ImGui::Begin("BVH");

		ImGui::Text("Split algorithm:");

		bool isDichotomy{ m_pGeom->bvh.m_alg == 0 };
		ImGui::Checkbox("Dichotomy", &isDichotomy);
		if (isDichotomy) {
			m_pGeom->bvh.m_alg = 0;
			ImGui::DragInt("Primitives per leaf", &m_pGeom->bvh.m_primsPerLeaf, 1, 2, 32);
		}

		bool isSAH{ m_pGeom->bvh.m_alg == 1 };
		ImGui::Checkbox("SAH", &isSAH);
		if (isSAH) {
			m_pGeom->bvh.m_alg = 1;
		}

		bool isFixedStepSAH{ m_pGeom->bvh.m_alg == 2 };
		ImGui::Checkbox("FixedStepSAH", &isFixedStepSAH);
		if (isFixedStepSAH) {
			m_pGeom->bvh.m_alg = 2;
		}

		bool isBinnedSAH{ m_pGeom->bvh.m_alg == 3 };
		ImGui::Checkbox("BinnedSAH", &isBinnedSAH);
		if (isBinnedSAH) {
			m_pGeom->bvh.m_alg = 3;
		}

		if (isFixedStepSAH || isBinnedSAH) {
			ImGui::DragInt("SAH step", &m_pGeom->bvh.m_sahSteps, 1, 2, 32);
		}

		ImGui::Text(" ");

		float carcassPart{ 100.f * m_pGeom->bvh.m_frmPart };
		ImGui::DragFloat("Part for carcass", &carcassPart, 1.f, 0.f, 100.f);
		m_pGeom->bvh.m_frmPart = carcassPart / 100.f;

		float carcassUniform{ 100.f * m_pGeom->bvh.m_uniform };
		ImGui::DragFloat("Carcass unifrom", &carcassUniform, 1.f, 0.f, 100.f);
		m_pGeom->bvh.m_uniform = carcassUniform / 100.f;

		ImGui::Text(" ");

		ImGui::Text("Statistics:");

		ImGui::Text("Average BVH time (ms): %.3f", m_geomCPUAvgTime);
		ImGui::Text(" ");
		ImGui::Text("Nodes: %d", m_pGeom->bvh.m_nodesUsed);
		ImGui::Text("Leafs: %d", m_pGeom->bvh.m_leafsCnt);
		ImGui::Text(" ");
		ImGui::Text("Primitives: %d", m_pGeom->bvh.m_primsCnt);
		ImGui::Text("Average primitives per leaf: %.3f", 1.f * m_pGeom->bvh.m_primsCnt / m_pGeom->bvh.m_leafsCnt);
		ImGui::Text(" ");
		ImGui::Text("Min depth: %d", m_pGeom->bvh.m_depthMin);
		ImGui::Text("Max depth: %d", m_pGeom->bvh.m_depthMax);

		ImGui::End();
	}

	{
		ImGui::Begin("Ray Tracing");

		ImGui::Text("Intersection algorithm:");

		bool isNaive{ m_rtBuffer.instsAlgLeafsTCheck.y == 0 };
		ImGui::Checkbox("Naive", &isNaive);
		if (isNaive) {
			m_rtBuffer.instsAlgLeafsTCheck.y = 0;
		}

		bool isBVHStack{ m_rtBuffer.instsAlgLeafsTCheck.y == 1 };
		ImGui::Checkbox("Stack", &isBVHStack);
		if (isBVHStack) {
			m_rtBuffer.instsAlgLeafsTCheck.y = 1;
		}

		bool isBVHStackless{ m_rtBuffer.instsAlgLeafsTCheck.y == 2 };
		ImGui::Checkbox("Stackless", &isBVHStackless);
		if (isBVHStackless) {
			m_rtBuffer.instsAlgLeafsTCheck.y = 2;

			ImGui::Text(" ");

			bool isProcessLeafs{ m_rtBuffer.instsAlgLeafsTCheck.z == 1 };
			ImGui::Checkbox("Process m_leafsCnt", &isProcessLeafs);
			m_rtBuffer.instsAlgLeafsTCheck.z = isProcessLeafs ? 1 : 0;

			bool isTCheck{ m_rtBuffer.instsAlgLeafsTCheck.w == 1 };
			ImGui::Checkbox("Check T", &isTCheck);
			m_rtBuffer.instsAlgLeafsTCheck.w = isTCheck ? 1 : 0;
		}

		ImGui::Text(" ");

		ImGui::Text("Statistics:");

		ImGui::Text("Average BVH traverse time (ms): %.3f", m_geomGPUAvgTime);

		ImGui::End();
	}

	{
		ImGui::Begin("Mortons");

		XMINT4 highlights{ m_highlights };

		ImGui::DragInt("Morton primitive id", &highlights.x, 1, 0, m_pGeom->bvh.m_primsCnt - 1);
		ImGui::Text("Buffer primitive id: %i", m_pGeom->bvh.m_mortonPrims[highlights.x].primId);
		ImGui::Text(" ");
		ImGui::DragInt("Morton primitive 2 id", &highlights.y, 1, 0, m_pGeom->bvh.m_primsCnt - 1);
		ImGui::Text("Buffer primitive id: %i", m_pGeom->bvh.m_mortonPrims[highlights.y].primId);
		ImGui::Text(" ");
		ImGui::DragInt("Morton primitive 3 id", &highlights.z, 1, 0, m_pGeom->bvh.m_primsCnt - 1);
		ImGui::Text("Buffer primitive id: %i", m_pGeom->bvh.m_mortonPrims[highlights.z].primId);

		m_highlights = highlights;

		ImGui::End();
	}

	// Rendering
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return SUCCEEDED(m_pSwapChain->Present(0, 0));
}

HRESULT Renderer::initScene() {
	HRESULT hr{ S_OK };

	// create scene buffer
	{
		D3D11_BUFFER_DESC desc{
			.ByteWidth{ sizeof(SceneBuffer) },
			.Usage{ D3D11_USAGE_DYNAMIC },
			.BindFlags{ D3D11_BIND_CONSTANT_BUFFER },
			.CPUAccessFlags{ D3D11_CPU_ACCESS_WRITE }
		};

		hr = m_pDevice->CreateBuffer(&desc, nullptr, &m_pSceneBuffer);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pSceneBuffer, "SceneBuffer");
		THROW_IF_FAILED(hr);
	}

	// create rt buffer
	{
		D3D11_BUFFER_DESC desc{
			.ByteWidth{ sizeof(RTBuffer) },
			.Usage{ D3D11_USAGE_DYNAMIC },
			.BindFlags{ D3D11_BIND_CONSTANT_BUFFER },
			.CPUAccessFlags{ D3D11_CPU_ACCESS_WRITE }
		};

		hr = m_pDevice->CreateBuffer(&desc, nullptr, &m_pRTBuffer);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pRTBuffer, "RTBuffer");
		THROW_IF_FAILED(hr);
	}

	// No culling rasterizer state
	{
		D3D11_RASTERIZER_DESC desc{
			.FillMode{ D3D11_FILL_SOLID },
			.CullMode{ D3D11_CULL_NONE },
			.DepthClipEnable{ TRUE }
		};

		hr = m_pDevice->CreateRasterizerState(&desc, &m_pRasterizerState);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pRasterizerState, "RasterizerState");
		THROW_IF_FAILED(hr);
	}

	// create blend states
	{
		D3D11_BLEND_DESC desc{
			.AlphaToCoverageEnable{},
			.IndependentBlendEnable{},
			.RenderTarget{ {
				.BlendEnable{ TRUE },
				.SrcBlend{ D3D11_BLEND_SRC_ALPHA }, // alpha
				.DestBlend{ D3D11_BLEND_INV_SRC_ALPHA }, // 1 - alpha
				.BlendOp{ D3D11_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D11_BLEND_ONE },
				.DestBlendAlpha{ D3D11_BLEND_ZERO },
				.BlendOpAlpha{ D3D11_BLEND_OP_ADD },
				.RenderTargetWriteMask{ D3D11_COLOR_WRITE_ENABLE_ALL }
			} }
		};

		hr = m_pDevice->CreateBlendState(&desc, &m_pTransBlendState);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pTransBlendState, "TransBlendState");
		THROW_IF_FAILED(hr);

		desc.RenderTarget[0].BlendEnable = FALSE;
		hr = m_pDevice->CreateBlendState(&desc, &m_pOpaqueBlendState);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pOpaqueBlendState, "OpaqueBlendState");
		THROW_IF_FAILED(hr);
	}

	// create reverse depth state
	{
		D3D11_DEPTH_STENCIL_DESC desc{
			.DepthEnable{ TRUE },
			.DepthWriteMask{ D3D11_DEPTH_WRITE_MASK_ALL },
			.DepthFunc{ D3D11_COMPARISON_GREATER_EQUAL }
		};

		hr = m_pDevice->CreateDepthStencilState(&desc, &m_pDepthState);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pDepthState, "DephtState");
		THROW_IF_FAILED(hr);
	}

	// create reverse transparent depth state
	{
		D3D11_DEPTH_STENCIL_DESC desc{
			.DepthEnable{ TRUE },
			.DepthWriteMask{ D3D11_DEPTH_WRITE_MASK_ZERO },
			.DepthFunc{ D3D11_COMPARISON_GREATER },
			.StencilEnable{ FALSE }
		};

		hr = m_pDevice->CreateDepthStencilState(&desc, &m_pTransDepthState);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pTransDepthState, "TransDepthState");
		THROW_IF_FAILED(hr);
	}

	// create sampler
	{
		D3D11_SAMPLER_DESC desc{
			.Filter{ D3D11_FILTER_ANISOTROPIC },
			// wrapping - repeating texture outside borders
			.AddressU{ D3D11_TEXTURE_ADDRESS_WRAP },
			.AddressV{ D3D11_TEXTURE_ADDRESS_WRAP },
			.AddressW{ D3D11_TEXTURE_ADDRESS_WRAP },
			.MipLODBias{}, // mipmap offset
			.MaxAnisotropy{ 16 },
			.ComparisonFunc{ D3D11_COMPARISON_NEVER },
			// address border color
			.BorderColor{ 1.0f, 1.0f, 1.0f, 1.0f },
			// mipmap
			.MinLOD{ -FLT_MAX },
			.MaxLOD{ FLT_MAX }
		};

		hr = m_pDevice->CreateSamplerState(&desc, &m_pSampler);
		THROW_IF_FAILED(hr);

		hr = setResourceName(m_pSampler, "Sampler");
		THROW_IF_FAILED(hr);
	}

	//hr = m_pCube->initCull();
	m_pGeom = new Geometry(m_pDevice, m_pDeviceContext);
	hr = m_pGeom->init(m_pPostProcess->getTexture());
	THROW_IF_FAILED(hr);

	//m_pCube->rayTracingInit(m_pPostProcess->getTexture());

	return hr;
}

void Renderer::termScene() {
	m_pGeom->term();
	m_pPostProcess->term();

	SAFE_RELEASE(m_pSampler);

	SAFE_RELEASE(m_pTransDepthState);
	SAFE_RELEASE(m_pDepthState);

	SAFE_RELEASE(m_pOpaqueBlendState);
	SAFE_RELEASE(m_pTransBlendState);

	SAFE_RELEASE(m_pRasterizerState);

	SAFE_RELEASE(m_pRTBuffer);
	SAFE_RELEASE(m_pSceneBuffer);
}
