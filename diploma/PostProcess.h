#pragma once

#include "framework.h"

class PostProcess {
    ID3D11Texture2D* m_pBuffer{};
    ID3D11RenderTargetView* m_pBufferRTV{};
    ID3D11ShaderResourceView* m_pBufferSRV{};
    ID3D11VertexShader* m_pVertexShader{};
    ID3D11PixelShader* m_pPixelShader{};

    ID3D11Device* m_pDevice{};
    ID3D11DeviceContext* m_pDeviceContext{};

public:
    bool m_useSepia{};

    PostProcess(ID3D11Device* device, ID3D11DeviceContext* deviceContext) :
        m_pDevice(device),
        m_pDeviceContext(deviceContext)
    {}

    ID3D11Texture2D* getTexture();
    ID3D11RenderTargetView* getBufferRTV();

    HRESULT init();
    void term();

    void render(
        ID3D11RenderTargetView* m_pBackBufferRTV,
        ID3D11SamplerState* m_pSampler
    );
    HRESULT setupBuffer(int width, int height);

private:
    HRESULT createPostProcessBuffer(int width, int height);
};