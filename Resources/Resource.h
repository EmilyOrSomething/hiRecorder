#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define IDD_MAIN			100
#define IDC_PATH			101
#define IDC_PATH_BROWSE		102

#define IDC_VIDEO_BITRATE	201
#define IDC_VIDEO_FRAMERATE	202

#define IDC_AUDIO_SYS		301
#define IDC_AUDIO_MIC		302

#define IDC_HIDE			401
#define IDC_PAUSE_RESUME	402
#define IDC_START_STOP		403

#define IDC_NONE			-1

#define APP_NAME			L"hiRecorder"
#define IDI_APP_ICON		LoadIconW(NULL, IDI_APPLICATION);

#define WM_APP_VIEW			(WM_USER + 0)
#define WM_APP_MSG			(WM_USER + 1)
#define WM_APP_UPDATE		(WM_USER + 2)

#define APP_TITLE_NORMAL	L"hiRecorder"
#define APP_TITLE_STARTING	L"hiRecorder - Starting"
#define APP_TITLE_RECORDING	L"hiRecorder - Recording"
#define APP_TITLE_PAUSING	L"hiRecorder - Pausing"
#define APP_TITLE_PAUSED	L"hiRecorder - Paused"
#define APP_TITLE_STOPPING	L"hiRecorder - Stopping"
#define APP_TITLE_SAVING	L"hiRecorder - Saving"

#define APP_STOPPED			0
#define APP_PAUSED			1
#define APP_RECORDING		2
