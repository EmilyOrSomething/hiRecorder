#pragma once
#define WIN32_LEAN_AND_MEAN
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct AudioPacket {
	unsigned long long tick = 0;
	unsigned int sampleCount = 0;
	unsigned int pad = 0;
	short samples[960]{};
};

class AudioCapture {
public:
	~AudioCapture() {
		if (audioClient) audioClient->Stop();
		if (hAudioEvent) CloseHandle(hAudioEvent);
	}

	bool Initialize(bool isMicrophone);
	bool ReadPackets(AudioPacket* packets, unsigned long long& packetCount);

	HANDLE hAudioEvent = nullptr;


private:
	ComPtr<IMMDevice> endpoint;
	ComPtr<IAudioClient> audioClient;
	ComPtr<IAudioCaptureClient> captureClient;
};