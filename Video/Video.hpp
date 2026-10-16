#pragma once
#include "VideoCapture.hpp"

#include <mfx.h>

class Video {
public:
	~Video() {
		if (loader) { MFXUnload(loader); }
	}
	bool Initialize();

	HANDLE Start();
	void Stop();

private:
	mfxStatus status{};
	mfxLoader loader{};
	mfxSession session{};
	mfxVideoParam encodeParams{};
	mfxHDLPair handles{};

	mfxConfig cfg[4]{};
	mfxVariant cfgVal[4]{};
	mfxExtBuffer* extBuffers[4]{};

	mfxExtCodingOption extOps{};
	mfxExtCodingOption2 extOps2{};
	mfxExtCodingOption3 extOps3{};
	mfxExtVideoSignalInfo signalInfo{};

	VideoCapture capture;

	bool stop = false;

	unsigned int Thread();
	static unsigned __stdcall ThreadWrapper(void* pParam);
};