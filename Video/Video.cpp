#include "Video.hpp"
#include "../Utils/Time.hpp"
#include "../Utils/FileHandle.hpp"
#include "../Utils/Error.hpp"
#include "..\Resources\Resource.h"

#include <process.h>

bool Video::Initialize() {
    loader = MFXLoad();
    if (!loader) { return false; }

    if (!capture.Initialize()) { return false; }

    cfg[0] = MFXCreateConfig(loader);
    cfgVal[0].Type = MFX_VARIANT_TYPE_U32;
    cfgVal[0].Data.U32 = MFX_IMPL_TYPE_HARDWARE;
    status = MFXSetConfigFilterProperty(cfg[0], (mfxU8*)"mfxImplDescription.Impl", cfgVal[0]);
    if (status != MFX_ERR_NONE) {
        Error::MsgBox(L"Failed to find compatible hardware!");
        return false;
    }

    cfg[1] = MFXCreateConfig(loader);
    cfgVal[1].Type = MFX_VARIANT_TYPE_U32;
    cfgVal[1].Data.U32 = MFX_CODEC_HEVC;
    status = MFXSetConfigFilterProperty(cfg[1], (mfxU8*)"mfxImplDescription.mfxEncoderDescription.encoder.CodecID", cfgVal[1]);
    if (status != MFX_ERR_NONE) {
        Error::MsgBox(L"Hardware doesn't support HEVC codec!");
        return false;
    }

    cfg[2] = MFXCreateConfig(loader);
    cfgVal[2].Type = MFX_VARIANT_TYPE_U32;
    cfgVal[2].Data.U32 = 131074; // 2.2
    status = MFXSetConfigFilterProperty(cfg[2], (mfxU8*)"mfxImplDescription.ApiVersion.Version", cfgVal[2]);
    if (status != MFX_ERR_NONE) {
        Error::MsgBox(L"Failed to find compatible api vesrion!");
        return false;
    }

    cfg[3] = MFXCreateConfig(loader);
    cfgVal[3].Type = MFX_VARIANT_TYPE_U32;
    cfgVal[3].Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
    status = MFXSetConfigFilterProperty(cfg[3], (mfxU8*)"mfxImplDescription.AccelerationMode", cfgVal[3]);
    if (status != MFX_ERR_NONE) {
        Error::MsgBox(L"Failed to find hardware that supports Direct3D11!");
        return false;
    }

    extOps.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    extOps.Header.BufferSz = sizeof(mfxExtCodingOption);
    extOps.RateDistortionOpt = MFX_CODINGOPTION_OFF;

    extOps2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    extOps2.Header.BufferSz = sizeof(mfxExtCodingOption2);
    extOps2.DisableDeblockingIdc = MFX_CODINGOPTION_ON;

    extOps3.Header.BufferId = MFX_EXTBUFF_CODING_OPTION3;
    extOps3.Header.BufferSz = sizeof(mfxExtCodingOption3);
    extOps3.ScenarioInfo = MFX_SCENARIO_REMOTE_GAMING;

    signalInfo.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
    signalInfo.Header.BufferSz = sizeof(mfxExtVideoSignalInfo);
    signalInfo.VideoFullRange = 0;
    signalInfo.ColourDescriptionPresent = 1;
    signalInfo.ColourPrimaries = 1;
    signalInfo.TransferCharacteristics = 13;
    signalInfo.MatrixCoefficients = 1;

    extBuffers[0] = (mfxExtBuffer*)&extOps;
    extBuffers[1] = (mfxExtBuffer*)&extOps2;
    extBuffers[2] = (mfxExtBuffer*)&extOps3;
    extBuffers[3] = (mfxExtBuffer*)&signalInfo;

    encodeParams.ExtParam = extBuffers;
    encodeParams.NumExtParam = 4;

    encodeParams.mfx.CodecId = MFX_CODEC_HEVC;
    encodeParams.mfx.RateControlMethod = MFX_RATECONTROL_ICQ;
    encodeParams.mfx.FrameInfo.FrameRateExtD = 1;
    encodeParams.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
    encodeParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
    encodeParams.mfx.CodecId = MFX_CODEC_HEVC;
    encodeParams.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
    encodeParams.mfx.FrameInfo.CropW = Config::internal.width;
    encodeParams.mfx.FrameInfo.CropH = Config::internal.height;
    encodeParams.mfx.FrameInfo.Width = ((Config::internal.width + 15) >> 4) << 4;
    encodeParams.mfx.FrameInfo.Height = ((Config::internal.height + 15) >> 4) << 4;
    encodeParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    encodeParams.mfx.GopRefDist = 1;
    encodeParams.mfx.IdrInterval = 1;
    encodeParams.AsyncDepth = 2;

    return true;
}

HANDLE Video::Start() {
    status = MFXCreateSession(loader, 0, &session);
    if (status != MFX_ERR_NONE) { return nullptr; }

    status = MFXVideoCORE_SetHandle(session, MFX_HANDLE_D3D11_DEVICE, capture.GetDevice());
    if (status != MFX_ERR_NONE) { return nullptr; }

    encodeParams.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
    encodeParams.mfx.ICQQuality = Config::GetBitrate();
    encodeParams.mfx.FrameInfo.FrameRateExtN = Config::user.video.framerate;
    encodeParams.mfx.GopPicSize = Config::user.video.framerate;

    status = MFXVideoENCODE_Query(session, &encodeParams, &encodeParams);
    if (status == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM || status < MFX_ERR_NONE) { return nullptr; }

    status = MFXVideoENCODE_Init(session, &encodeParams);
    if (status != MFX_ERR_NONE) { return nullptr; }

    MFXVideoENCODE_GetVideoParam(session, &encodeParams);
    if (status != MFX_ERR_NONE) { return nullptr; }

    stop = false;

    return (HANDLE)_beginthreadex(nullptr, 0, &ThreadWrapper, this, 0, nullptr);
}

uint Video::Thread() {
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        PostMessageW(Config::internal.hwnd, WM_APP_UPDATE, 0, APP_STOPPED);
        Config::internal.msgVideo = Message::Failure;
        Sync::SyncVideo();
        Error::MsgBoxRemote(L"Failed to Initialize the COM library!");
        return 1;
    }

    const mfxU32 bufferSize = encodeParams.mfx.BufferSizeInKB * 1000;

    mfxBitstream bitstream{};
    bitstream.Data = new mfxU8[bufferSize];
    bitstream.MaxLength = bufferSize;
    if (!bitstream.Data) {
        PostMessageW(Config::internal.hwnd, WM_APP_UPDATE, 0, APP_STOPPED);
        Config::internal.msgVideo = Message::Failure;
        Sync::SyncVideo();
        Error::MsgBoxRemote(L"Failed to allocate memory!");
        return 1;
    }

    mfxU8* frameBuffer = new mfxU8[bufferSize + 8];
    if (!frameBuffer) {
        delete[] bitstream.Data;
        PostMessageW(Config::internal.hwnd, WM_APP_UPDATE, 0, APP_STOPPED);
        Config::internal.msgVideo = Message::Failure;
        Sync::SyncVideo();
        Error::MsgBoxRemote(L"Failed to allocate memory!");
        return 1;
    }

    wchar_t path[260]{};
    memcpy(path, Config::internal.path, 260);
    wcscat_s(path, 260, L".hevc");

    FileHandle file(path, false);
    if (file.lastError) {
        delete[] bitstream.Data;
        delete[] frameBuffer;
        PostMessageW(Config::internal.hwnd, WM_APP_UPDATE, 0, APP_STOPPED);
        Config::internal.msgVideo = Message::Failure;
        Sync::SyncVideo();
        Error::MsgBoxRemote(L"Failed to create a file!");
        return 1;
    }

    LARGE_INTEGER recordingStart{};
    LARGE_INTEGER performanceFrequency{};
    
    QueryPerformanceCounter(&recordingStart);
    QueryPerformanceFrequency(&performanceFrequency);

    const long double freq1k = 1000.0L / performanceFrequency.QuadPart;
    const long double freq90k = 90000.0L / performanceFrequency.QuadPart;
    ullong nextTick = Config::internal.startTick = recordingStart.QuadPart;
    ullong ticksPerFrame = performanceFrequency.QuadPart / Config::user.video.framerate;

    ullong timestamp = 0;

    bool active = true;
    Sync::SyncVideo();
    PostMessageW(Config::internal.hwnd, WM_APP_UPDATE, 0, APP_RECORDING);

    while (active) {
        switch (Config::internal.msgAudio) {
        case Message::Failure:
            Config::internal.msgVideo = Message::Failure;
            Sync::SyncVideo();
            active = false;
            continue;

        case Message::Pause:
            Config::internal.msgVideo = Message::Pause;
            Sync::SyncVideo();
            PostMessageW(Config::internal.hwnd, WM_APP_UPDATE, 0, APP_PAUSED);
            WaitForSingleObject(Config::internal.hPause, INFINITE);
            Config::internal.msgVideo = Message::None;
            Config::internal.msgAudio = Message::None;
            break;
        }

        LARGE_INTEGER currentTime{};
        QueryPerformanceCounter(&currentTime);

        const ullong elapsedTicks = static_cast<ullong>(currentTime.QuadPart - recordingStart.QuadPart);
        timestamp = static_cast<ullong>(elapsedTicks * freq1k);
        const ullong timestamp90k = static_cast<ullong>(elapsedTicks * freq90k);

        const llong left = ticksPerFrame - (currentTime.QuadPart - nextTick);
        if (left > 0) { Time::Sleep(left); }
        nextTick = currentTime.QuadPart;

        mfxSyncPoint syncp = nullptr;

        if (!stop) {
            if (!capture.CaptureFrame()) {
                Config::internal.msgVideo = Message::Failure;
                Sync::SyncVideo();
                active = false;
                continue;
            }

            mfxFrameSurface1* pSurfaceIn = nullptr;
            status = MFXMemory_GetSurfaceForEncode(session, &pSurfaceIn);

            if (status == MFX_ERR_NONE && pSurfaceIn) {
                mfxHDL hdl = nullptr;
                mfxResourceType resType = {};
                pSurfaceIn->FrameInterface->GetNativeHandle(pSurfaceIn, &hdl, &resType);
                capture.d3dContext->CopyResource(static_cast<ID3D11Texture2D*>(hdl), capture.GetTexture());
                pSurfaceIn->Data.TimeStamp = timestamp90k;
                status = MFXVideoENCODE_EncodeFrameAsync(session, nullptr, pSurfaceIn, &bitstream, &syncp);
                pSurfaceIn->FrameInterface->Release(pSurfaceIn);
            }
        }
        else {
            status = MFXVideoENCODE_EncodeFrameAsync(session, nullptr, nullptr, &bitstream, &syncp);
        }
        
        switch (status) {
        case MFX_ERR_NONE:
            if (!syncp) { break; }
            do {
                status = MFXVideoCORE_SyncOperation(session, syncp, MFX_INFINITE);
                if (status != MFX_ERR_NONE) {
                    if (status != MFX_WRN_IN_EXECUTION) {
                        Config::internal.msgVideo = Message::Failure;
                        Sync::SyncVideo();
                        active = false;
                    }
                    break;
                }
                if (bitstream.DataLength <= 0) { continue; }

                int length = bitstream.DataLength;
                if ((bitstream.FrameType & MFX_FRAMETYPE_IDR) || (bitstream.FrameType & MFX_FRAMETYPE_I)) { length = -length; }

                const uint frameTimestamp = static_cast<uint>(bitstream.TimeStamp == MFX_TIMESTAMP_UNKNOWN ? timestamp : bitstream.TimeStamp / 90ULL);
                memcpy(frameBuffer, &length, 4);
                memcpy(frameBuffer + 4, &frameTimestamp, 4);
                memcpy(frameBuffer + 8, bitstream.Data + bitstream.DataOffset, bitstream.DataLength);

                if (!file.Write(frameBuffer, bitstream.DataLength + 8)) {
                    Config::internal.msgVideo = Message::Failure;
                    Sync::SyncVideo();
                    active = false;
                    break;
                }

                bitstream.DataLength = 0;
                bitstream.DataOffset = 0;
            } while (status == MFX_WRN_IN_EXECUTION);
            break;

        case MFX_ERR_MORE_DATA:
            if (stop) {
                Config::internal.msgVideo = Message::Failure;
                Sync::SyncVideo();
                active = false;
            }
            continue;

        case MFX_WRN_DEVICE_BUSY:
            Time::SleepMilli(1);
            continue;

        default:
            if (status < 0) {
                Config::internal.msgVideo = Message::Failure;
                Sync::SyncVideo();
                active = false;
            }
            break;
        }
    }

    delete[] bitstream.Data;
    delete[] frameBuffer;

    CoUninitialize();

    MFXVideoENCODE_Close(session);
    MFXClose(session);

    LARGE_INTEGER recordingEnd{};
    if (QueryPerformanceCounter(&recordingEnd)) {
        timestamp = static_cast<ullong>((recordingEnd.QuadPart - recordingStart.QuadPart) * freq1k);
    }
    file.Write(&timestamp, sizeof(timestamp));
    file.Reset();

    Config::internal.msgVideo = Message::None;
    Sync::SyncVideo();
    PostMessageW(Config::internal.hwnd, WM_APP_UPDATE, 0, APP_STOPPED);
    
    return 0;
}

void Video::Stop() {
    stop = true;
}

unsigned __stdcall Video::ThreadWrapper(void* pParam) {
    if (!pParam) { return 1; }
    Video* pThis = static_cast<Video*>(pParam);
    return pThis->Thread();
}