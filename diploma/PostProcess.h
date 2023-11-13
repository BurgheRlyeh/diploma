#pragma once

#include "framework.h"

class PostProcess {
    ID3D11Device* m_pDevice{};
    ID3D11DeviceContext* m_pDeviceContext{};

    ID3D11Texture2D* m_pBuffer{};
    ID3D11RenderTargetView* m_pBufferRTV{};
    ID3D11ShaderResourceView* m_pBufferSRV{};

    ID3D11VertexShader* m_pVertexShader{};
    ID3D11PixelShader* m_pPixelShader{};

public:
    PostProcess(ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext) :
        m_pDevice(pDevice),
        m_pDeviceContext(pDeviceContext)
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
};