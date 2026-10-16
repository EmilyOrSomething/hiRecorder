#pragma once
#include "../Config.hpp"
#include "../Resources/Resource.h"

class Error {
public:
	static void MsgBox(const wchar_t* message) {
		MessageBoxW(nullptr, message, APP_NAME, MB_OK | MB_ICONERROR);
	}

	static void MsgBoxRemote(const wchar_t* message) {
		SendMessageW(Config::internal.hwnd, WM_APP_MSG, MB_ICONERROR, (LPARAM)message);
	}

};
