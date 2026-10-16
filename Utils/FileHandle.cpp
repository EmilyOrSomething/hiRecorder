#include "FileHandle.hpp"

void FileHandle::Initialize(wpath filename, bool action) {
	hFile = CreateFileW(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, action ? OPEN_EXISTING : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) { lastError = GetLastError(); return; }
	if (!GetFileSizeEx(hFile, &size)) { lastError = GetLastError(); }
	memcpy(name, filename, 260);
}

FileHandle::FileHandle(wpath filename, bool action) {
	Initialize(filename, action);
}

FileHandle::~FileHandle() {
	if (hFile) { CloseHandle(hFile); }
}

void FileHandle::Reset() {
	lastError = 0;
	size = { 0 };
	pointer = { 0 };
	CloseHandle(hFile);
	hFile = nullptr;
}

bool FileHandle::Write(const void* data, const ulong size) {
	ulong written = 0;
	const BOOL succeeded = WriteFile(hFile, data, size, &written, nullptr);
	pointer.QuadPart += written;

	if (!succeeded || written != size) {
		lastError = succeeded ? ERROR_WRITE_FAULT : GetLastError();
		return false;
	}

	return true;
}

bool FileHandle::Read(void* data, ulong size) {
	ulong read = 0;
	const BOOL succeeded = ReadFile(hFile, data, size, &read, nullptr);
	pointer.QuadPart += read;

	if (!succeeded || read != size) {
		lastError = succeeded ? ERROR_HANDLE_EOF : GetLastError();
		return false;
	}

	return true;
}

bool FileHandle::Seek(ullong address, ulong from) {
	LARGE_INTEGER li{};
	li.QuadPart = address;

	if (!SetFilePointerEx(hFile, li, &pointer, from)) {
		lastError = GetLastError();
		return false;
	}

	return true;
}

bool FileHandle::Delete() {
	Reset();

	if (!DeleteFileW(name)) {
		lastError = GetLastError();
		return false;
	}

	return true;
}