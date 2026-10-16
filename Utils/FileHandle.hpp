#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Data.hpp"

using namespace Data::Types;

class FileHandle {
public:
	FileHandle(wpath filename, bool action); // action: true to open file, false to create file.
	FileHandle() {};

	void Initialize(wpath filename, bool action); // action: true to open file, false to create file.
	void Reset();

	~FileHandle();

	bool Write(const void* data, const ulong size);
	inline bool Write(const void* data, const unsigned int size) { return Write(data, static_cast<ulong>(size)); }
	inline bool Write(const void* data, const ullong size) { return Write(data, static_cast<ulong>(size)); }
	inline bool Write(const void* data, const __int64 size) { return Write(data, static_cast<ulong>(size)); }

	bool Read(void* data, ulong size);
	inline bool Read(void* data, unsigned int size) { return Read(data, static_cast<ulong>(size)); }
	inline bool Read(void* data, ullong size) { return Read(data, static_cast<ulong>(size)); }
	inline bool Read(void* data, __int64 size) { return Read(data, static_cast<ulong>(size)); }

	bool Seek(ullong address, ulong from);
	bool Delete();

	ullong Size() const { return size.QuadPart; }
	ullong Pointer() const { return pointer.QuadPart; }

	DWORD lastError = 0;
	wpath name{};


private:
	HANDLE hFile = nullptr;
	LARGE_INTEGER size{};
	LARGE_INTEGER pointer{};
};