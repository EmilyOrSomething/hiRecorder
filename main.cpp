#include "Resources/Resource.h"
#include "Video/Video.hpp"
#include "Audio/Audio.hpp"
#include "Muxer/Muxer.hpp"
#include "Config.hpp"

#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl_core.h>
#include <wchar.h>
#include <dwmapi.h>

INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);
void SetConfigValues();
void BrowsePath();
void EnableClose(bool enable);
void SetBorderColor(COLORREF color);

Audio audio;
Video video;
Muxer muxer;

HANDLE handles[2] = { nullptr };

bool recording = false;
bool paused = false;

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {

	HANDLE hMutex = CreateMutexW(nullptr, FALSE, L"hiRecorderMutex");
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		HWND hwnd = FindWindowW(nullptr, APP_NAME);
		if (hwnd) { SendMessageW(hwnd, WM_APP_VIEW, 0, WM_LBUTTONUP); }
		return 0;
	}

	if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) { return 1; }

	INITCOMMONCONTROLSEX icc{};
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_BAR_CLASSES;
	InitCommonControlsEx(&icc);

	Config::Load();
	if (!audio.Initialize()) { return 2; }
	if (!video.Initialize()) { return 3; }

	Config::internal.hwnd = CreateDialogW(hInstance, MAKEINTRESOURCE(IDD_MAIN), nullptr, DialogProc);
	if (Config::internal.hwnd == INVALID_HANDLE_VALUE) { return 3; }

	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0)) {
		if (!IsDialogMessageW(Config::internal.hwnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}
	video.Stop();
	audio.Stop();

	Config::Save();
	CoUninitialize();

	WaitForMultipleObjects(2, handles, TRUE, 10000);
	CloseHandle(handles[0]);
	CloseHandle(handles[1]);

	return 0;
}

INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
	switch (msg) {
	case WM_INITDIALOG: {
		//HICON hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCE(IDI_APP_ICON));
		HICON hIcon = IDI_APP_ICON;
		SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
		SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

		NOTIFYICONDATAW nid = { 0 };
		nid.cbSize = sizeof(NOTIFYICONDATAW);
		nid.hWnd = hwnd;
		nid.uID = 1;
		nid.uCallbackMessage = WM_APP_VIEW;
		nid.uFlags = NIF_TIP | NIF_ICON | NIF_MESSAGE;
		nid.hIcon = hIcon;
		wcscpy_s(nid.szTip, L"High-Performance Intel Recorder");

		Shell_NotifyIconW(NIM_ADD, &nid);

		SendDlgItemMessageW(hwnd, IDC_PATH, WM_SETTEXT, 0, (LPARAM)Config::user.app.path);

		SendDlgItemMessageW(hwnd, IDC_VIDEO_BITRATE, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
		SendDlgItemMessageW(hwnd, IDC_VIDEO_BITRATE, TBM_SETPOS, TRUE, Config::user.video.bitrate);

		SetDlgItemInt(hwnd, IDC_VIDEO_FRAMERATE, Config::user.video.framerate, FALSE);
		SendDlgItemMessageW(hwnd, IDC_VIDEO_FRAMERATE, EM_SETLIMITTEXT, 3, 0);

		SendDlgItemMessageW(hwnd, IDC_AUDIO_SYS, BM_SETCHECK, Config::user.audio.captureSys, 0);
		SendDlgItemMessageW(hwnd, IDC_AUDIO_MIC, BM_SETCHECK, Config::user.audio.captureMic, 0);

		EnableWindow(GetDlgItem(hwnd, IDC_PAUSE_RESUME), FALSE);

		RECT r{};
		GetWindowRect(hwnd, &r);

		int x = (GetSystemMetrics(SM_CXSCREEN) - r.right + r.left) / 2;
		int y = (GetSystemMetrics(SM_CYSCREEN) - r.bottom + r.top) / 2;

		SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
		ShowWindow(hwnd, SW_SHOW);
		return TRUE;
	}
	case WM_APP_UPDATE:
		switch (l) {
		case APP_RECORDING:
			SetBorderColor(RGB(255, 0, 0));
			SetWindowTextW(hwnd, APP_TITLE_RECORDING);
			
			EnableWindow(GetDlgItem(hwnd, IDC_START_STOP), TRUE);
			SetDlgItemTextW(hwnd, IDC_START_STOP, L"Stop");

			EnableWindow(GetDlgItem(hwnd, IDC_PAUSE_RESUME), TRUE);
			SetDlgItemTextW(hwnd, IDC_PAUSE_RESUME, L"Pause");

			recording = true;
			return TRUE;

		case APP_PAUSED:
			SetBorderColor(RGB(0, 255, 0));
			SetWindowTextW(hwnd, APP_TITLE_PAUSED);

			EnableWindow(GetDlgItem(hwnd, IDC_START_STOP), FALSE);
			SetDlgItemTextW(hwnd, IDC_START_STOP, L"Stop");

			EnableWindow(GetDlgItem(hwnd, IDC_PAUSE_RESUME), TRUE);
			SetDlgItemTextW(hwnd, IDC_PAUSE_RESUME, L"Resume");
			
			paused = true;
			return TRUE;

		case APP_STOPPED:
			SetBorderColor(RGB(255, 255, 0));
			SetWindowTextW(hwnd, APP_TITLE_SAVING);
			EnableWindow(hwnd, FALSE);

			muxer.Save();

			EnableWindow(hwnd, TRUE);

			SetBorderColor(DWMWA_COLOR_DEFAULT);
			SetWindowTextW(hwnd, APP_TITLE_NORMAL);

			EnableWindow(GetDlgItem(hwnd, IDC_START_STOP), TRUE);
			SetDlgItemTextW(hwnd, IDC_START_STOP, L"Start");

			EnableWindow(GetDlgItem(hwnd, IDC_PAUSE_RESUME), FALSE);
			SetDlgItemTextW(hwnd, IDC_PAUSE_RESUME, L"Pause");

			recording = false;
			EnableClose(true);
			return TRUE;
		}
		break;
	
	case WM_APP_MSG:
		MessageBoxW(hwnd, (const wchar_t*)l, APP_NAME, MB_OK | (UINT)w);
		return TRUE;

	case WM_APP_VIEW:
		if (l == WM_LBUTTONUP || l == WM_RBUTTONUP) {
			if (IsWindowVisible(hwnd)) { ShowWindow(hwnd, SW_HIDE); }
			else { ShowWindow(hwnd, SW_SHOW); }
		}
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(w)) {
		case IDC_HIDE:
			ShowWindow(hwnd, SW_HIDE);
			return TRUE;

		case IDC_PAUSE_RESUME:
			if (paused) {
				SetBorderColor(RGB(255, 0, 0));
				SetWindowTextW(hwnd, APP_TITLE_RECORDING);
				EnableWindow(GetDlgItem(hwnd, IDC_PAUSE_RESUME), FALSE);

				EnableWindow(GetDlgItem(hwnd, IDC_START_STOP), TRUE);
				SetDlgItemTextW(hwnd, IDC_START_STOP, L"Stop");

				EnableWindow(GetDlgItem(hwnd, IDC_PAUSE_RESUME), TRUE);
				SetDlgItemTextW(hwnd, IDC_PAUSE_RESUME, L"Pause");
				SetEvent(Config::internal.hPause);
				paused = false;
			}
			else {
				Config::internal.msgAudio = Message::Pause;
				ResetEvent(Config::internal.hPause);

				SetWindowTextW(hwnd, APP_TITLE_PAUSING);
				EnableWindow(GetDlgItem(hwnd, IDC_PAUSE_RESUME), FALSE);
			}
			return TRUE;

		case IDC_START_STOP:
			EnableClose(false);
			if (recording) {
				SetWindowTextW(hwnd, APP_TITLE_STOPPING);
				EnableWindow(GetDlgItem(hwnd, IDC_START_STOP), FALSE);
				video.Stop();
				audio.Stop();
			}
			else {
				SetWindowTextW(hwnd, APP_TITLE_STARTING);
				EnableWindow(GetDlgItem(hwnd, IDC_START_STOP), FALSE);
				SetConfigValues();
				handles[0] = video.Start();
				if (Config::user.audio.captureSys || Config::user.audio.captureMic) { handles[1] = audio.Start(); }
			}
			return TRUE;

		case IDC_PATH_BROWSE:
			BrowsePath();
			return TRUE;
		}
		break;
	

	case WM_CLOSE:
		video.Stop();
		audio.Stop();
		SetConfigValues();
		DestroyWindow(hwnd);
		return TRUE;

	case WM_DESTROY:
		PostQuitMessage(0);
		return TRUE;
	}
	return FALSE;
}

void SetConfigValues() {
	GetDlgItemTextW(Config::internal.hwnd, IDC_PATH, Config::user.app.path, 260);

	Config::user.video.bitrate = (unsigned char)SendDlgItemMessageW(Config::internal.hwnd, IDC_VIDEO_BITRATE, TBM_GETPOS, 0, 0);
	Config::user.video.framerate = GetDlgItemInt(Config::internal.hwnd, IDC_VIDEO_FRAMERATE, nullptr, FALSE);

	Config::user.audio.captureSys = IsDlgButtonChecked(Config::internal.hwnd, IDC_AUDIO_SYS);
	Config::user.audio.captureMic = IsDlgButtonChecked(Config::internal.hwnd, IDC_AUDIO_MIC);

	SYSTEMTIME st;
	GetLocalTime(&st);

	wchar_t buffer[21]{};

	_snwprintf_s(buffer, 21, 21, L"\\%04d-%02d-%02d_%02d-%02d-%02d",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	if (buffer) {
		memcpy(Config::internal.path, Config::user.app.path, 260);
		wcscat_s(Config::internal.path, 260, buffer);
	}
}

void BrowsePath() {
	IFileOpenDialog* pDlg = nullptr;

	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg));

	if (FAILED(hr)) { return; }
	pDlg->SetOptions(FOS_PICKFOLDERS);
	hr = pDlg->Show(Config::internal.hwnd);

	if (SUCCEEDED(hr)) {
		IShellItem* pItem = nullptr;
		hr = pDlg->GetResult(&pItem);

		if (SUCCEEDED(hr)) {
			PWSTR path = nullptr;
			pItem->GetDisplayName(SIGDN_FILESYSPATH, &path);

			if (path) { SetDlgItemTextW(Config::internal.hwnd, IDC_PATH, path); }

			CoTaskMemFree(path);
			pItem->Release();
		}
	}
	pDlg->Release();
}

void EnableClose(bool enable) {
	HMENU hMenu = GetSystemMenu(Config::internal.hwnd, FALSE);
	if (hMenu) { EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | (enable ? MF_ENABLED : MF_DISABLED | MF_GRAYED)); }
}

void SetBorderColor(COLORREF color) {
	DwmSetWindowAttribute(Config::internal.hwnd, DWMWA_BORDER_COLOR, &color, 4);
}