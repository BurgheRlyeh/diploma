#include "Renderer.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void Renderer::switchRotation() {
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

	//m_CPUTimer.start();

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
	//m_pGeom->resizeUAV(m_pPostProcess->getTexture());

	D3D11_MAPPED_SUBRESOURCE subres;
	hr = m_pDeviceContext->Map(
		m_pRTBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subres
	);
	THROW_IF_FAILED(hr);

	m_rtBuffer.whnf.x = static_cast<FLOAT>(m_width);
	m_rtBuffer.whnf.y = static_cast<FLOAT>(m_height);

	memcpy(subres.pData, &m_rtBuffer, sizeof(RTBuffer));
	m_pDeviceContext->Unmap(m_pRTBuffer, 0);

	return SUCCEEDED(hr);
}

void Renderer::update() {
	size_t time{ static_cast<size_t>(std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now().time_since_epoch()
	).count()) };

	if (!m_prevTime) {
		m_prevTime = time;
	}

	// move camera
	m_pCamera->updatePosition((time - m_prevTime) / 1e6f);

	//m_pCube->update((time - m_prevTime) / 1e6f, m_isModelRotate);
	//m_pGeom->update((time - m_prevTime) / 1e6f, m_isModelRotate);
	//m_pGeom->updateBVH(true);

	m_prevTime = time;

	Vector3 cameraPos{ m_pCamera->getPosition() };

	// Setup camera
	Matrix v{ XMMatrixLookAtLH(cameraPos, m_pCamera->getPoi(), m_pCamera->getUp()) };
	Matrix p{ XMMatrixPerspectiveLH(
		2 * m_far * tanf(m_fov / 2),
		2 * m_far * tanf(m_fov / 2) * m_height / m_width,
		m_far,
		m_near
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

	// cube ray tracing buffer update
	{
		THROW_IF_FAILED(m_pDeviceContext->Map(
			m_pRTBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subres
		));

		(v * p).Invert(m_rtBuffer.pvInv);

		// update direction vector
		Vector4 f = { 1.f / m_width, -1.f / m_height, 0.f, 1.f };
		f = Vector4::Transform(f, m_rtBuffer.pvInv);

		Vector4 n = { f.x, f.y, 1.f / (m_near - m_far), f.w };
		n = Vector4::Transform(n, m_rtBuffer.pvInv);

		(f / f.w - n / n.w).Normalize(m_rtBuffer.camDir);

		memcpy(subres.pData, &m_rtBuffer, sizeof(RTBuffer));
		m_pDeviceContext->Unmap(m_pRTBuffer, 0);
	}
}

bool Renderer::render() {
	m_pDeviceContext->ClearState();

	ID3D11RenderTargetView* views[]{ m_pPostProcess->getBufferRTV() };
	m_pDeviceContext->OMSetRenderTargets(1, views, m_pDepthBufferDSV);

	static const FLOAT BackColor[4]{ 0.25f, 0.25f, 0.25f, 1.0f };
	m_pDeviceContext->ClearRenderTargetView(m_pPostProcess->getBufferRTV(), BackColor);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthBufferDSV, D3D11_CLEAR_DEPTH, 0.0f, 0);

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

	// CUBE
	//if (m_pCube->getIsRayTracing()) {
	//	m_pCube->rayTracing(m_pSampler, m_pSceneBuffer, m_pRTBuffer, m_width, m_height);
	//	// bind render target
	//	m_pDeviceContext->OMSetRenderTargets(1, views, m_isUseZBuffer ? m_pDepthBufferDSV : nullptr);
	//}

	//m_pGeom->rayTracing(m_pSceneBuffer, m_pRTBuffer, m_width, m_height);

	// CUBE END

	/*m_pRect->render(
		m_pSampler,
		m_pSceneBuffer,
		m_pTransDepthState,
		m_pTransBlendState,
		m_camera.getPosition()
	);*/

	m_pPostProcess->render(m_pBackBufferRTV, m_pSampler);

	//m_pCube->readQueries();

	/*++m_frameCounter;
	double bvhTime{ m_pGeom->m_pCPUTimer->getTime() };
	m_bvhTime += bvhTime;
	double cubeTime{ m_pCube->m_pGPUTimer->getTime() };
	m_cubeTime += cubeTime;*/

	/*double currTime{ m_CPUTimer.getCurrent() };
	if (currTime > 1e3) {
		m_fps = m_frameCounter / (currTime / 1e3);
		m_bvhTimeAvg = m_bvhTime / m_frameCounter;
		m_cubeTimeAvg = m_cubeTime / m_frameCounter;
		m_bvhTime = 0;
		m_cubeTime = 0;
		m_frameCounter = 0;
		m_CPUTimer.start();
	}*/

	// Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	{
		ImGui::Begin("Stats");

		ImGui::Text("FPS: %.1f", m_fps);

		ImGui::Text("");

		ImGui::Text("AVG CubeBVH time (ms): %.3f", m_bvhTimeAvg);
		ImGui::Text("AVG Cube time (ms): %.3f", m_cubeTimeAvg);

		ImGui::Text("");

		ImGui::Text("Width: %d", m_width);
		ImGui::Text("Height: %d", m_height);

		ImGui::End();
	}

	/*{
		ImGui::Begin("CubeBVH");

		ImGui::Text("Split alg:");

		bool isSAH{ m_pCube->bvh.isSAH };
		ImGui::Checkbox("SAH", &isSAH);
		if (m_pCube->bvh.isSAH != isSAH) {
			m_pCube->bvh.isSAH = isSAH;
			m_pCube->updateBVH(true);
		}

		if (isSAH) {
			bool isStepSAH{ m_pCube->bvh.isStepSAH };
			ImGui::Checkbox("SAH, fixed planes", &isStepSAH);
			if (m_pCube->bvh.isStepSAH != isStepSAH) {
				m_pCube->bvh.isStepSAH = isStepSAH;
				m_pCube->updateBVH(true);
			}

			if (isStepSAH) {
				bool isBinsSAH{ m_pCube->bvh.isBinsSAH };
				ImGui::Checkbox("dynamic SAH, fixed planes", &isBinsSAH);
				if (m_pCube->bvh.isBinsSAH != isBinsSAH) {
					m_pCube->bvh.isBinsSAH = isBinsSAH;
					m_pCube->updateBVH(true);
				}
			}
		}

		ImGui::Text(" ");
		ImGui::Text("Stats:");

		ImGui::Text("Cubes: %d", m_pCube->bvh.cnt);
		ImGui::Text("Nodes: %d", m_pCube->bvh.nodesUsed);
		ImGui::Text("Primitives: %d", m_pCube->bvh.cnt * 12);
		ImGui::Text("Leafs: %d", m_pCube->bvh.leafs);
		ImGui::Text("Average prims per leaf: %.3f", 12.0 * m_pCube->bvh.cnt / m_pCube->bvh.leafs);
		ImGui::Text("Depth: %d ... %d", m_pCube->bvh.depthMin, m_pCube->bvh.depthMax);

		ImGui::Text("");

		if (m_pCube->bvh.isStepSAH) {
			ImGui::DragInt("SAH planes", &m_pCube->bvh.sahStep, 1, 2, 25);
		}
		else if (!m_pCube->bvh.isSAH) {
			ImGui::DragInt("ppl", &m_pCube->bvh.trianglesPerLeaf, 1, 1, 12);
		}

		ImGui::End();
	}*/

	//{
	//	ImGui::Begin("GeomBVH");

	//	ImGui::Text("Split alg:");

	//	bool isSAH{ m_pGeom->bvh.isSAH };
	//	ImGui::Checkbox("SAH", &isSAH);
	//	if (m_pGeom->bvh.isSAH != isSAH) {
	//		m_pGeom->bvh.isSAH = isSAH;
	//		m_pGeom->updateBVH(true);
	//	}

	//	if (isSAH) {
	//		bool isStepSAH{ m_pGeom->bvh.isStepSAH };
	//		ImGui::Checkbox("SAH, fixed planes", &isStepSAH);
	//		if (m_pGeom->bvh.isStepSAH != isStepSAH) {
	//			m_pGeom->bvh.isStepSAH = isStepSAH;
	//			m_pGeom->updateBVH(true);
	//		}

	//		if (isStepSAH) {
	//			bool isBinsSAH{ m_pGeom->bvh.isBinsSAH };
	//			ImGui::Checkbox("dynamic SAH, fixed planes", &isBinsSAH);
	//			if (m_pGeom->bvh.isBinsSAH != isBinsSAH) {
	//				m_pGeom->bvh.isBinsSAH = isBinsSAH;
	//				m_pGeom->updateBVH(true);
	//			}
	//		}
	//	}

	//	ImGui::Text(" ");
	//	ImGui::Text("Stats:");

	//	ImGui::Text("Geoms: %d", m_pGeom->bvh.cnt);
	//	ImGui::Text("Nodes: %d", m_pGeom->bvh.nodesUsed);
	//	ImGui::Text("Primitives: %d", m_pGeom->bvh.cnt * 1107);
	//	ImGui::Text("Leafs: %d", m_pGeom->bvh.leafs);
	//	ImGui::Text("Average prims per leaf: %.3f", 1107.0 / m_pGeom->bvh.leafs);
	//	ImGui::Text("Depth: %d ... %d", m_pGeom->bvh.depthMin, m_pGeom->bvh.depthMax);

	//	ImGui::Text("");

	//	if (m_pGeom->bvh.isStepSAH) {
	//		ImGui::DragInt("SAH planes", &m_pGeom->bvh.sahStep, 1, 2, 32);
	//	}
	//	else if (!m_pGeom->bvh.isSAH) {
	//		ImGui::DragInt("ppl", &m_pGeom->bvh.trianglesPerLeaf, 1, 1, 12);
	//	}

	//	ImGui::End();
	//}

	{
		ImGui::Begin("RT");

		bool intsecStackless{ m_rtBuffer.instancesIntsecalgLeafsTCheck.y == 0 };
		bool intsecStack{ m_rtBuffer.instancesIntsecalgLeafsTCheck.y == 1 };
		bool intsecNaive{ m_rtBuffer.instancesIntsecalgLeafsTCheck.y == 2 };

		ImGui::Checkbox("Stackless intersection", &intsecStackless);
		ImGui::Checkbox("Stack intersection", &intsecStack);
		ImGui::Checkbox("Naive intersection", &intsecNaive);

		if (m_rtBuffer.instancesIntsecalgLeafsTCheck.y == 0)
			m_rtBuffer.instancesIntsecalgLeafsTCheck.y = intsecStack ? 1 : (intsecNaive ? 2 : 0);
		else if (m_rtBuffer.instancesIntsecalgLeafsTCheck.y == 1)
			m_rtBuffer.instancesIntsecalgLeafsTCheck.y = intsecStackless ? 0 : (intsecNaive ? 2 : 1);
		else
			m_rtBuffer.instancesIntsecalgLeafsTCheck.y = intsecStackless ? 0 : (intsecStack ? 1 : 2);

		ImGui::Text(" ");

		ImGui::Text("Stackless settings:");

		bool notProcLeafs{ m_rtBuffer.instancesIntsecalgLeafsTCheck.z == 1 };
		ImGui::Checkbox("Not process leafs", &notProcLeafs);
		m_rtBuffer.instancesIntsecalgLeafsTCheck.z = notProcLeafs ? 1 : 0;

		bool checkT{ m_rtBuffer.instancesIntsecalgLeafsTCheck.w == 1 };
		ImGui::Checkbox("Check T", &checkT);
		m_rtBuffer.instancesIntsecalgLeafsTCheck.w = checkT ? 1 : 0;

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
	THROW_IF_FAILED(hr);

	//m_pCube->rayTracingInit(m_pPostProcess->getTexture());

	return hr;
}

void Renderer::termScene() {
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
