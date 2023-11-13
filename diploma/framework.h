// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

// Windows Header Files
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// DirectX
#include <dxgi.h>
#include <d3d11.h>
//#include <d3dcommon.h>
#include <d3dcompiler.h>

#include <DirectXMath.h>

// DirectXTK
#include <SimpleMath.h>

// ImGui
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

// macros
#define SAFE_RELEASE(p)\
{\
    if (p != nullptr)\
    {\
        p->Release();\
        p = nullptr;\
    }\
}

#define THROW_IF_FAILED(hr) {\
    if (FAILED(hr)) {\
        throw std::exception();\
    }\
}

inline HRESULT SetResourceName(ID3D11DeviceChild* pResource, const std::string& name) {
    return pResource->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.length()), name.c_str());
}


