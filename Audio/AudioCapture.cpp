#include "AudioCapture.hpp"

bool AudioCapture::Initialize(bool isMicrophone) {
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) { return false; }

    EDataFlow dataFlow = isMicrophone ? eCapture : eRender;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(dataFlow, eConsole, &endpoint))) { return false; }

    if (FAILED(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audioClient))) { return false; }

    hAudioEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!hAudioEvent) { return false; }

    DWORD streamFlags = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY | AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    if (!isMicrophone) { streamFlags |= AUDCLNT_STREAMFLAGS_LOOPBACK; }

    WAVEFORMATEX pcmFormat{};
    pcmFormat.wFormatTag = WAVE_FORMAT_PCM;
    pcmFormat.nChannels = 2;
    pcmFormat.nSamplesPerSec = 48000;
    pcmFormat.wBitsPerSample = 16;
    pcmFormat.nBlockAlign = pcmFormat.nChannels * pcmFormat.wBitsPerSample / 8;
    pcmFormat.nAvgBytesPerSec = pcmFormat.nSamplesPerSec * pcmFormat.nBlockAlign;

    if (FAILED(audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags, 100000, 0, &pcmFormat, nullptr))) { return false; }
    if (FAILED(audioClient->SetEventHandle(hAudioEvent))) { return false; }
    if (FAILED(audioClient->GetService(IID_PPV_ARGS(&captureClient)))) { return false; }

    return SUCCEEDED(audioClient->Start());
}

bool AudioCapture::ReadPackets(AudioPacket* packets, unsigned long long& packetCount) {
    packetCount = 0;

    UINT32 packetLength = 0;
    if (FAILED(captureClient->GetNextPacketSize(&packetLength)))
        return false;

    for (;;) {
        if (!packetLength) { break; }

        unsigned char* pData = nullptr;
        UINT32 framesAvailable = 0;
        DWORD flags = 0;
        UINT64 qpcPosition = 0;

        if (FAILED(captureClient->GetBuffer(&pData, &framesAvailable, &flags, nullptr, &qpcPosition))) { return false; }

        AudioPacket& packet = packets[packetCount++];

        packet.tick = qpcPosition;

        const UINT32 sampleCount = framesAvailable << 1;
        const UINT32 byteCount = sampleCount << 1;

        packet.sampleCount = sampleCount;

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) { memset(packet.samples, 0, byteCount); }
        else { memcpy(packet.samples, pData, byteCount); }

        if (FAILED(captureClient->ReleaseBuffer(framesAvailable))) { return false; }
        if (FAILED(captureClient->GetNextPacketSize(&packetLength))) { return false; }
    }

    return true;
}
