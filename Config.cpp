#include "Config.hpp"
#include "Utils/FileHandle.hpp"

UserConfig Config::user;
InternalConfig Config::internal;

bool Config::Load() {
	wpath f = L"config.bin";
	FileHandle file(f, true);
	if (!file.lastError) {
		unsigned char* data = new unsigned char[file.Size()];
		if (!data) { return false; }

		if (!file.Read(data, file.Size())) {
			delete[] data;
			return false;
		}

		memcpy(&user, data, file.Size());
		delete[] data;

		return true;
	}
	Save();
	return true;
}

bool Config::Save() {
	wpath f = L"config.bin";
	FileHandle file(f, false);
	if (file.lastError) { return false; }

	unsigned char* data = new unsigned char[sizeof(UserConfig)];
	if (!data) { return false; }

	memcpy(data, &user, sizeof(UserConfig));

	bool r = file.Write(data, sizeof(user));
	delete[] data;

	return r;
}