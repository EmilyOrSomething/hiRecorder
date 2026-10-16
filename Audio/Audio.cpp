#include "Audio.hpp"
#include "AudioCapture.hpp"
#include "../Utils/Time.hpp"
#include "../Config.hpp"
#include "../Utils/FileHandle.hpp"

#include <process.h>

using namespace Data::Types;
ullong WriteAudioPackets(AudioCapture& capture, FileHandle& file, bool& audioStarted,AudioPacket* packets);

bool Audio::Initialize() {
    Config::internal.hPause = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!Config::internal.hPause) { return false; }

    return true;
}

HANDLE Audio::Start() {
    stop = false;
    return (HANDLE)_beginthreadex(nullptr, 0, &ThreadWrapper, this, 0, nullptr);
}

ullong WriteAudioPackets(AudioCapture& capture, FileHandle& file, bool& audioStarted, AudioPacket* packets) {
    ullong audioLength = 0;
    ullong packetCount = 0;
    if (!capture.ReadPackets(packets, packetCount)) return audioLength;

    static constexpr ullong TICK_PER_FRAME = 208;
    const ullong startTick = Config::internal.startTick;

    static constexpr size_t MaxPayload = 480 * 4;
    static constexpr size_t MaxWrite = sizeof(uint) + sizeof(ullong) + MaxPayload;

    byte buffer[MaxWrite];

    for (ullong i = 0; i < packetCount; ++i) {
        const AudioPacket& packet = packets[i];
        const ullong frameCount = packet.sampleCount >> 1;

        ullong firstFrame = 0;

        if (packet.tick < startTick) {
            const ullong diff = startTick - packet.tick;
            firstFrame = (diff * 48000 + 9999999) / 10000000;
        }

        if (!audioStarted) {
            const short* s = packet.samples;
            while (firstFrame < frameCount &&
                *(uint*)(s + (firstFrame << 1)) == 0)
            { ++firstFrame; }
        }

        if (firstFrame >= frameCount) { continue; }

        const ullong sampleTime = packet.tick + firstFrame * TICK_PER_FRAME;
        if (sampleTime < startTick) continue;

        const ullong timestampMs = (sampleTime - startTick) / 10000;

        const ullong payloadFrames = frameCount - firstFrame;
        const uint payloadSize = static_cast<uint>(payloadFrames << 2);

        const short* payload = packet.samples + (firstFrame << 1);

        uint* pSize = (uint*)buffer;
        ullong* pTime = (ullong*)(buffer + sizeof(uint));
        byte* pPayload = buffer + sizeof(uint) + sizeof(ullong);

        *pSize = payloadSize;
        *pTime = timestampMs;

        audioLength = timestampMs;

        memcpy(pPayload, payload, payloadSize);

        if (!file.Write(buffer, sizeof(uint) + sizeof(ullong) + payloadSize)) { return audioLength; }

        audioStarted = true;
    }

    return audioLength;
}


unsigned int Audio::Thread() {
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        Config::internal.msgAudio = Message::Failure;
        Sync::SyncAudio();
        return 1;
    }

    FileHandle syspcm;
    FileHandle micpcm;

    AudioCapture sysAudio;
    AudioCapture micAudio;

    HANDLE waitHandles[2] = {};
    DWORD handleCount = 0;

    wchar_t path[260]{};
    memcpy(path, Config::internal.path, 260);

    if (Config::user.audio.captureSys) {
        if (!sysAudio.Initialize(false)) {
            Config::internal.msgAudio = Message::Failure;
            Sync::SyncAudio();
            return 1;
        }

        wchar_t f[260]{};
        memcpy(f, path, 260);
        wcscat_s(f, 260, L".syspcm");

        syspcm.Initialize(f, false);
        if (syspcm.lastError) {
            Config::internal.msgAudio = Message::Failure;
            Sync::SyncAudio();
            return 1;
        }

        waitHandles[handleCount++] = sysAudio.hAudioEvent;
    }

    if (Config::user.audio.captureMic) {
        if (!micAudio.Initialize(true)) {
            Config::internal.msgAudio = Message::Failure;
            Sync::SyncAudio();
            return 1;
        }
        wchar_t f[260]{};
        memcpy(f, path, 260);
        wcscat_s(f, 260, L".micpcm");

        micpcm.Initialize(f, false);
        if (micpcm.lastError) {
            Config::internal.msgAudio = Message::Failure;
            Sync::SyncAudio();
            return 1;
        }

        waitHandles[handleCount++] = micAudio.hAudioEvent;
    }

    bool sysAudioStarted = false;
    bool micAudioStarted = false;
    AudioPacket sysPackets[100]{};
    AudioPacket micPackets[100]{};

    bool active = true;
    Sync::SyncAudio();

    while (active) {
        switch (Config::internal.msgVideo) {
        case Message::Failure:
            active = false;
            continue;

        case Message::Pause:
            Config::internal.msgAudio = Message::Pause;
            Sync::SyncAudio();
            WaitForSingleObject(Config::internal.hPause, INFINITE);
            Config::internal.msgAudio = Message::None;
            break;
        }

        DWORD waitResult = WaitForMultipleObjects(handleCount, waitHandles, FALSE, 20);

        if ((waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + handleCount) ||
            waitResult == WAIT_TIMEOUT) {

            if (Config::user.audio.captureSys) {
                Config::internal.sysAudioMs = WriteAudioPackets(sysAudio, syspcm, sysAudioStarted, sysPackets);
            }

            if (Config::user.audio.captureMic) {
                Config::internal.micAudioMs = WriteAudioPackets(micAudio, micpcm, micAudioStarted, micPackets);
            }
        }
        else {
            Config::internal.msgAudio = Message::Failure;
            Sync::SyncAudio();
        }
    }

    CoUninitialize();

    micpcm.Reset();
    syspcm.Reset();

    Config::internal.msgVideo = Message::None;
    Sync::SyncVideo();

    return 0;
}

void Audio::Stop() {
    stop = true;
}

unsigned __stdcall Audio::ThreadWrapper(void* pParam) {
    if (!pParam) { return 1; }
    Audio* pThis = static_cast<Audio*>(pParam);
    return pThis->Thread();
}