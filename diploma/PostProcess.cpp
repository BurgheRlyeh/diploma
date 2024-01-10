#include "PostProcess.h"

#include "ShaderLoader.h"

ID3D11Texture2D* PostProcess::getTexture() {
    return m_pBuffer;
}

ID3D11RenderTargetView* PostProcess::getBufferRTV() {
    return m_pBufferRTV;
}

HRESULT PostProcess::init() {
    HRESULT hr{ S_OK };

    ID3DBlob* pBlob{};
    std::wstring filepath{ L"PostProcessVS.cso" };
    hr = D3DReadFileToBlob(filepath.c_str(), &pBlob);
    THROW_IF_FAILED(hr);

    hr = m_pDevice->CreateVertexShader(
        pBlob->GetBufferPointer(),
        pBlob->GetBufferSize(),
        nullptr,
        &m_pVertexShader
    );
    THROW_IF_FAILED(hr);

    filepath = L"PostProcessPS.cso";
    hr = D3DReadFileToBlob(filepath.c_str(), &pBlob);
    THROW_IF_FAILED(hr);

    hr = m_pDevice->CreatePixelShader(
        pBlob->GetBufferPointer(),
        pBlob->GetBufferSize(),
        nullptr,
        &m_pPixelShader
    );
    THROW_IF_FAILED(hr);

    return hr;
}

void PostProcess::term() {
    SAFE_RELEASE(m_pBufferSRV);
    SAFE_RELEASE(m_pBufferRTV);
    SAFE_RELEASE(m_pBuffer);

    SAFE_RELEASE(m_pPixelShader);
    SAFE_RELEASE(m_pVertexShader);
}

HRESULT PostProcess::setupBuffer(int width, int height) {
    SAFE_RELEASE(m_pBuffer);
    SAFE_RELEASE(m_pBufferRTV);
    SAFE_RELEASE(m_pBufferSRV);

    HRESULT hr{ S_OK };

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
    hr = m_pDevice->CreateTexture2D(&desc, nullptr, &m_pBuffer);
    THROW_IF_FAILED(hr);

    hr = setResourceName(m_pBuffer, "PostProcessBuffer");
    THROW_IF_FAILED(hr);

    hr = m_pDevice->CreateRenderTargetView(
        m_pBuffer, nullptr, &m_pBufferRTV
    );
    THROW_IF_FAILED(hr);

    hr = setResourceName(m_pBufferRTV, "PostProcessBufferRTV");
    THROW_IF_FAILED(hr);

    hr = m_pDevice->CreateShaderResourceView(
        m_pBuffer, nullptr, &m_pBufferSRV
    );
    THROW_IF_FAILED(hr);

    hr = setResourceName(m_pBufferSRV, "PostProcessBufferSRV");
    THROW_IF_FAILED(hr);

    return hr;
}

void PostProcess::render(
    ID3D11RenderTargetView* pBackBufferRTV,
    ID3D11SamplerState* pSampler
) {
    ID3D11RenderTargetView* views[] = { pBackBufferRTV };
    m_pDeviceContext->OMSetRenderTargets(1, views, nullptr);

    ID3D11SamplerState* samplers[] = { pSampler };
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
