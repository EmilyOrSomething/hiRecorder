#pragma once
#include <wrl.h>
#include <d3d11_4.h>
#include <dxgi.h>

using Microsoft::WRL::ComPtr;

class VideoCapture {
public:
    void ClearCache();

    bool Initialize();
    bool CaptureFrame();
    bool Recover();

    UINT targetDisplay = 0;

    ID3D11Device* GetDevice() const { return device.Get(); }
    ID3D11Texture2D* GetTexture() const { return texture.Get(); }
    ComPtr<ID3D11DeviceContext> d3dContext;

    ~VideoCapture() {
        ClearCache();
    }

private:
    struct CachedSRV {
        ID3D11Texture2D* texPtr = nullptr;
        ComPtr<ID3D11ShaderResourceView> srv;
    };

    CachedSRV srvCache[4]{};

    ID3D11ShaderResourceView* GetOrCreateSRV(ID3D11Texture2D* texturePointer);

    ComPtr<ID3D11ComputeShader> computeShader;
    ComPtr<ID3D11UnorderedAccessView> outputUAV;

    ComPtr<ID3D11Device> device;
    ComPtr<IDXGIOutputDuplication> deskDupl;

    ComPtr<ID3D11VideoDevice> videoDevice;
    ComPtr<ID3D11VideoContext> videoContext;

    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11Texture2D> emptyTexture;
};