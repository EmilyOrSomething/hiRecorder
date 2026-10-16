#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

enum class Message {
	Failure,
	Pause,
	None
};

struct AppConfig {
	wchar_t path[260] = L"\0";
};

struct AudioConfig {
	bool captureSys = true;
	bool captureMic = false;
};

struct VideoConfig {
	unsigned short framerate = 60;
	unsigned char bitrate = 50;
};

struct UserConfig {
	AppConfig app;
	AudioConfig audio;
	VideoConfig video;
};

struct InternalConfig {
	HWND hwnd = nullptr;
	HANDLE hPause = nullptr;

	unsigned int width = 0;
	unsigned int height = 0;
	wchar_t path[260] = L"\0";

	Message msgAudio = Message::None;
	Message msgVideo = Message::None;

	unsigned long long startTick = 0;

	unsigned long long sysAudioMs = 0;
	unsigned long long micAudioMs = 0;
};

class Config {
public:
	static UserConfig user;
	static InternalConfig internal;

	static bool Load();
	static bool Save();

	static const unsigned int GetBitrate() { return 1 + (50 * (100 - user.video.bitrate)) / 100; }
};