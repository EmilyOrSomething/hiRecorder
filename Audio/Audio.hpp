#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

class Audio {
public:
	bool Initialize();

	HANDLE Start();
	void Stop();

private:
	bool stop = false;

	unsigned int Thread();
	static unsigned __stdcall ThreadWrapper(void* pParam);
};