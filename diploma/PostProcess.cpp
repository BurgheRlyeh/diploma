#include "PostProcess.h"

ID3D11Texture2D* PostProcess::getTexture() {
    return m_pBuffer;
}

ID3D11RenderTargetView* PostProcess::getBufferRTV() {
    return m_pBufferRTV;
}

HRESULT PostProcess::init() {
    HRESULT hr{ S_OK };

    //D3DReadFileToBlob("")

    hr = compileAndCreateShader(
        m_pDevice, L"Sepia.vs", (ID3D11DeviceChild**)&m_pVertexShader
    );
    THROW_IF_FAILED(hr);

    hr = compileAndCreateShader(
        m_pDevice, L"Sepia.ps", (ID3D11DeviceChild**)&m_pPixelShader
    );
    THROW_IF_FAILED(hr);

    return hr;
}

void PostProcess::term() {
    SAFE_RELEASE(m_pBuffer);
    SAFE_RELEASE(m_pBufferRTV);
    SAFE_RELEASE(m_pBufferSRV);
    SAFE_RELEASE(m_pVertexShader);
    SAFE_RELEASE(m_pVertexShader);
}

void PostProcess::render(
    ID3D11RenderTargetView* m_pBackBufferRTV,
    ID3D11SamplerState* m_pSampler
) {
    ID3D11RenderTargetView* views[] = { m_pBackBufferRTV };
    m_pDeviceContext->OMSetRenderTargets(1, views, nullptr);

    ID3D11SamplerState* samplers[] = { m_pSampler };
    m_pDeviceContext->PSSetSamplers(0, 1, samplers);

    ID3D11ShaderResourceView* resources[] = { m_pBufferSRV };
    m_pDeviceContext->PSSetShaderResources(0, 1, resources);

    m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);
    m_pDeviceContext->RSSetState(nullptr);
    m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    m_pDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    m_pDeviceContext->IASetInputLayout(nullptr);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    m_pDeviceContext->Draw(3, 0);
}

HRESULT PostProcess::setupBuffer(int width, int height) {
    SAFE_RELEASE(m_pBuffer);
    SAFE_RELEASE(m_pBufferRTV);
    SAFE_RELEASE(m_pBufferSRV);

    HRESULT hr{ S_OK };

    hr = createPostProcessBuffer(width, height);
    THROW_IF_FAILED(hr);

    hr = SetResourceName(m_pBuffer, "PostProcessBuffer");
    THROW_IF_FAILED(hr);

    hr = m_pDevice->CreateRenderTargetView(
        m_pBuffer, nullptr, &m_pBufferRTV
    );
    THROW_IF_FAILED(hr);

    hr = SetResourceName(m_pBufferRTV, "PostProcessBufferRTV");
    THROW_IF_FAILED(hr);

    hr = m_pDevice->CreateShaderResourceView(
        m_pBuffer, nullptr, &m_pBufferSRV
    );
    THROW_IF_FAILED(hr);

    hr = SetResourceName(m_pBufferSRV, "PostProcessBufferSRV");
    THROW_IF_FAILED(hr);

    return hr;
}

HRESULT PostProcess::createPostProcessBuffer(int width, int height) {
    D3D11_TEXTURE2D_DESC desc{
        .Width{ static_cast<UINT>(width) },
        .Height{ static_cast<UINT>(height) },
        .MipLevels{ 1 },
        .ArraySize{ 1 },
        .Format{ DXGI_FORMAT_R8G8B8A8_UNORM },
        .SampleDesc{ 1, 0 },
        .Usage{ D3D11_USAGE_DEFAULT },
        .BindFlags{ D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS }
    };

    return m_pDevice->CreateTexture2D(&desc, nullptr, &m_pBuffer);
}