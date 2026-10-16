#include "VideoCapture.hpp"
#include "../Utils/FileHandle.hpp"
#include "../Config.hpp"
#include "../Resources/Shader.h"

static ID3D11ShaderResourceView* const NullSRV[1] = { nullptr };
static ID3D11UnorderedAccessView* const NullUAV[1] = { nullptr };

void VideoCapture::ClearCache() {
    for (size_t i = 0; i < 4; ++i) {
        if (srvCache[i].srv) {
            srvCache[i].srv.Reset();
        }
        srvCache[i].texPtr = nullptr;
    }
}

ID3D11ShaderResourceView* VideoCapture::GetOrCreateSRV(ID3D11Texture2D* texturePointer) {
    size_t emptySlot = 4;

    for (size_t i = 0; i < 4; ++i) {
        if (srvCache[i].texPtr == texturePointer) { return srvCache[i].srv.Get(); }
        if (srvCache[i].texPtr == nullptr && emptySlot == 4) { emptySlot = i; }
    }

    if (emptySlot == 4) emptySlot = 0;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* newSRV = nullptr;
    if (FAILED(device->CreateShaderResourceView(texturePointer, &srvDesc, &newSRV))) { return nullptr; }

    if (srvCache[emptySlot].srv) {
        srvCache[emptySlot].srv.Reset();
    }

    srvCache[emptySlot].texPtr = texturePointer;
    srvCache[emptySlot].srv = newSRV;

    return newSRV;
}

bool VideoCapture::Initialize() {
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags,
        featureLevels, 2, D3D11_SDK_VERSION, &device, nullptr, &d3dContext))) {
        return false;
    }

    ComPtr<ID3D10Multithread> pMultithread;
    if (SUCCEEDED(device.As(&pMultithread))) { pMultithread->SetMultithreadProtected(TRUE); }

    ComPtr<IDXGIDevice> dxgiDevice;
    device.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> dxgiAdapter;
    dxgiDevice->GetParent(IID_PPV_ARGS(&dxgiAdapter));

    ComPtr<IDXGIOutput> dxgiOutput;
    dxgiAdapter->EnumOutputs(0, &dxgiOutput);

    ComPtr<IDXGIOutput1> dxgiOutput1;
    dxgiOutput.As(&dxgiOutput1);

    if (FAILED(dxgiOutput1->DuplicateOutput(device.Get(), &deskDupl))) { return false; }

    DXGI_OUTDUPL_DESC duplDesc;
    deskDupl->GetDesc(&duplDesc);
    Config::internal.width = duplDesc.ModeDesc.Width;
    Config::internal.height = duplDesc.ModeDesc.Height;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = Config::internal.width;
    desc.Height = Config::internal.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_AYUV;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    desc.CPUAccessFlags = 0;

    if (FAILED(device->CreateTexture2D(&desc, nullptr, &texture))) { return false; }
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &emptyTexture))) { return false; }

    ullong count = static_cast<ullong>(Config::internal.width * Config::internal.height);
    uint* buffer = new uint[count];
    for (size_t i = 0; i < count; ++i) { buffer[i] = 0xFF108080; }

    d3dContext->UpdateSubresource(emptyTexture.Get(), 0, nullptr, buffer, Config::internal.width * sizeof(uint), 0);
    delete[] buffer;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;

    if (FAILED(device->CreateUnorderedAccessView(texture.Get(), &uavDesc, &outputUAV))) { return false; }

    return SUCCEEDED(device->CreateComputeShader(ShaderByteCode, sizeof(ShaderByteCode), nullptr, &computeShader));
}

bool VideoCapture::Recover() {
    ClearCache();

    if (deskDupl) {
        deskDupl->ReleaseFrame();
        deskDupl.Reset();
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(device.As(&dxgiDevice))) { return false; }

    ComPtr<IDXGIAdapter> dxgiAdapter;
    if (FAILED(dxgiDevice->GetParent(IID_PPV_ARGS(&dxgiAdapter)))) { return false; }

    ComPtr<IDXGIOutput> dxgiOutput;
    if (FAILED(dxgiAdapter->EnumOutputs(targetDisplay, &dxgiOutput))) { return false; }

    ComPtr<IDXGIOutput1> dxgiOutput1;
    if (FAILED(dxgiOutput.As(&dxgiOutput1))) { return false; }

    if (FAILED(dxgiOutput1->DuplicateOutput(device.Get(), &deskDupl))) { return false; }

    return true;
}


bool VideoCapture::CaptureFrame() {
    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    ComPtr<IDXGIResource> desktopResource;

    if (!deskDupl) {
        if (!Recover()) {
            d3dContext->CopyResource(texture.Get(), emptyTexture.Get());
            return true;
        }
        d3dContext->CopyResource(texture.Get(), emptyTexture.Get());
        return true;
    }

    HRESULT hr = deskDupl->AcquireNextFrame(0, &frameInfo, &desktopResource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) { return true; }

    if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_ACCESS_DENIED) {
        deskDupl->ReleaseFrame();

        if (!Recover()) {
            d3dContext->CopyResource(texture.Get(), emptyTexture.Get());
            return true;
        }

        d3dContext->CopyResource(texture.Get(), emptyTexture.Get());
        return true;
    }

    if (FAILED(hr)) {
        deskDupl->ReleaseFrame();
        d3dContext->CopyResource(texture.Get(), emptyTexture.Get());
        return false;
    }

    ComPtr<ID3D11Texture2D> bgraTexture;
    if (FAILED(desktopResource.As(&bgraTexture))) {
        deskDupl->ReleaseFrame();
        return false;
    }

    ID3D11ShaderResourceView* inputSRV = GetOrCreateSRV(bgraTexture.Get());

    if (inputSRV) {
        UINT threadGroupsX = (Config::internal.width + 15) / 16;
        UINT threadGroupsY = (Config::internal.height + 15) / 16;

        d3dContext->CSSetShader(computeShader.Get(), nullptr, 0);
        d3dContext->CSSetShaderResources(0, 1, &inputSRV);
        d3dContext->CSSetUnorderedAccessViews(0, 1, outputUAV.GetAddressOf(), nullptr);

        d3dContext->Dispatch(threadGroupsX, threadGroupsY, 1);

        d3dContext->CSSetShaderResources(0, 1, NullSRV);
        d3dContext->CSSetUnorderedAccessViews(0, 1, NullUAV, nullptr);
    }

    deskDupl->ReleaseFrame();
    return true;
}
