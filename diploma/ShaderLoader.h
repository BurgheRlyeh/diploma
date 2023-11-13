#pragma once

#include "framework.h"

HRESULT loadShaderBlob(const std::wstring& filename, ID3DBlob** pBlob) {
    std::wstring dir;
    dir.resize(MAX_PATH + 1);
    GetCurrentDirectory(MAX_PATH + 1, &dir[0]);
    dir.resize(dir.find(L'\0'));

    dir += L"\\";
#ifdef _WIN64
    dir += L"x64";
#else
    dir += L"Win32";
#endif
    dir += L"\\";
#ifdef _DEBUG
    dir += L"Debug";
#else
    dir += L"Release";
#endif
    dir += L"\\";

    std::wstring filepath{ dir + filename };
    return D3DReadFileToBlob(filepath.c_str(), pBlob);
}
