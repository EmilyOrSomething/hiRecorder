#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../Config.hpp"

class Time {
public:
	static void Sleep(unsigned long long intervals);
	inline static void SleepMilli(unsigned long long milliseconds) { Sleep(milliseconds * 10000ULL); }
	inline static void SleepMicro(unsigned long long microseconds) { Sleep(microseconds * 10ULL); }
};

class Sync {
public:
	bool Initialize();
	static void SyncVideo();
	static void SyncAudio();
	static void togglePause();

private:
	static HANDLE handles[2];
	
};