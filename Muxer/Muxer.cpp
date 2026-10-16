#include "Muxer.hpp"
#include "BitReader.hpp"
#include "../Utils/Array.hpp"
#include "../Config.hpp"

#include <string>

struct HevcSpsInfo {
	byte profileSpace = 0;
	byte tierFlag = 0;
	byte profileIdc = 0;
	uint compatibilityFlags = 0;
	ullong constraintFlags = 0;
	byte levelIdc = 0;
	byte maxSubLayers = 1;
	bool temporalIdNested = false;
	byte chromaFormat = 1;
	byte bitDepthLumaMinus8 = 0;
	byte bitDepthChromaMinus8 = 0;
};

void AppendVint(Array<byte>& buffer, ullong value, int forcedSize = 0) {
	int size = forcedSize;
	if (!size) {
		for (size = 1; size <= 8; ++size) {
			const ullong maximum = (1ULL << (size * 7)) - 2;
			if (value <= maximum) { break; }
		}
	}

	if (size < 1 || size > 8) { return; }
	ullong encoded = value | (1ULL << (size * 7));
	for (int i = size - 1; i >= 0; --i) {
		buffer.Write(static_cast<byte>(encoded >> (i * 8)));
	}
}

void AppendId(Array<byte>& buffer, uint id) {
	int size = id > 0x00FFFFFF ? 4 : id > 0x0000FFFF ? 3 : id > 0x000000FF ? 2 : 1;
	for (int i = size - 1; i >= 0; --i) {
		buffer.Write(id >> (i * 8));
	}
}

void AppendElement(Array<byte>& buffer, uint id, const Array<byte>& payload) {
	AppendId(buffer, id);
	AppendVint(buffer, payload.Size());
	buffer.Write(payload.Data(), payload.Size());
}

void AppendUIntElement(Array<byte>& buffer, uint id, ullong value) {
	int size = 1;
	Array<byte> payload(8);
	while (size < 8 && (value >> (size * 8))) ++size;
	for (int i = size - 1; i >= 0; --i) {
		payload.Write(static_cast<byte>(value >> (i * 8)));
	}
	AppendElement(buffer, id, payload);
}

void AppendFloatElement(Array<byte>& buffer, uint id, double value) {
	ullong bits = 0;
	memcpy(&bits, &value, sizeof(bits));
	Array<byte> payload(8);

	for (int i = 7; i >= 0; --i) {
		payload.Write(static_cast<byte>(bits >> (i * 8)));
	}
	AppendElement(buffer, id, payload);
}

void AppendStringElement(Array<byte>& buffer, uint id, const char* value) {
	const ullong size = strlen(value);
	Array<byte> payload(size);
	payload.Write((const byte*)value, size);

	AppendElement(buffer, id, payload);
}

bool ParseAnnexB(const Bytes& data, Bytes* converted,
	Bytes* vps = nullptr, Bytes* sps = nullptr, Bytes* pps = nullptr) {
	if (converted) {
		converted->Clear();
		converted->Reserve(data.Size());
	}

	ullong nalStart = ULLONG_MAX;
	bool found = false;
	auto processNal = [&](ullong start, ullong end) {
		while (end > start && data[end - 1] == 0) --end;
		if (end - start < 2) return false;

		const ullong size = end - start;
		const byte type = (data[start] >> 1) & 0x3F;
		Bytes* parameterSet = type == 32 ? vps : type == 33 ? sps : type == 34 ? pps : nullptr;
		if (parameterSet && parameterSet->Size() == 0) {
			parameterSet->Write(data.Data() + start, size);
		}
		if (converted) {
			if (size > UINT_MAX) return false;
			const uint nalSize = static_cast<uint>(size);
			converted->Write(static_cast<byte>(nalSize >> 24));
			converted->Write(static_cast<byte>(nalSize >> 16));
			converted->Write(static_cast<byte>(nalSize >> 8));
			converted->Write(static_cast<byte>(nalSize));
			converted->Write(data.Data() + start, size);
		}
		found = true;
		return true;
		};

	for (ullong i = 0; i + 3 <= data.Size();) {
		ullong prefix = 0;
		if (i + 4 <= data.Size() && data[i] == 0 && data[i + 1] == 0 &&
			data[i + 2] == 0 && data[i + 3] == 1) {
			prefix = 4;
		}
		else if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
			prefix = 3;
		}

		if (!prefix) {
			++i;
			continue;
		}

		if (nalStart != ULLONG_MAX && !processNal(nalStart, i)) return false;
		nalStart = i + prefix;
		i += prefix;
	}

	if (nalStart != ULLONG_MAX && !processNal(nalStart, data.Size())) return false;
	return found;
}

bool SkipBits(BitReader& reader, unsigned int count) {
	ullong ignored = 0;
	return reader.Read(count, ignored);
}

bool ParseSps(const Bytes& sps, HevcSpsInfo& info) {
	if (sps.Size() < 4 || ((sps[0] >> 1) & 0x3F) != 33) return false;

	Bytes rbsp(sps.Size());
	unsigned int zeroCount = 0;
	for (ullong i = 2; i < sps.Size(); ++i) {
		const byte byte = sps[i];
		if (zeroCount >= 2 && byte == 3) {
			zeroCount = 0;
			continue;
		}
		rbsp.Write(byte);
		zeroCount = byte == 0 ? zeroCount + 1 : 0;
	}

	BitReader reader(rbsp);
	ullong value = 0;
	if (!reader.Read(4, value) || !reader.Read(3, value)) return false;
	const byte maxSubLayersMinus1 = static_cast<byte>(value);
	if (maxSubLayersMinus1 > 6 || !reader.Read(1, value)) return false;
	info.temporalIdNested = value != 0;
	info.maxSubLayers = maxSubLayersMinus1 + 1;

	if (!reader.Read(2, value)) return false;
	info.profileSpace = static_cast<byte>(value);
	if (!reader.Read(1, value)) return false;
	info.tierFlag = static_cast<byte>(value);
	if (!reader.Read(5, value)) return false;
	info.profileIdc = static_cast<byte>(value);
	if (!reader.Read(32, value)) return false;
	info.compatibilityFlags = static_cast<uint>(value);
	if (!reader.Read(48, value)) return false;
	info.constraintFlags = value;
	if (!reader.Read(8, value)) return false;
	info.levelIdc = static_cast<byte>(value);

	bool profilePresent[7]{};
	bool levelPresent[7]{};
	for (byte i = 0; i < maxSubLayersMinus1; ++i) {
		if (!reader.Read(1, value)) return false;
		profilePresent[i] = value != 0;
		if (!reader.Read(1, value)) return false;
		levelPresent[i] = value != 0;
	}
	if (maxSubLayersMinus1 && !SkipBits(reader, (8 - maxSubLayersMinus1) * 2)) return false;
	for (byte i = 0; i < maxSubLayersMinus1; ++i) {
		if (profilePresent[i] && !SkipBits(reader, 88)) return false;
		if (levelPresent[i] && !SkipBits(reader, 8)) return false;
	}

	uint ue = 0;
	if (!reader.ReadUe(ue) || !reader.ReadUe(ue) || ue > 3) return false;
	info.chromaFormat = static_cast<byte>(ue);
	if (ue == 3 && !SkipBits(reader, 1)) return false;
	if (!reader.ReadUe(ue) || !reader.ReadUe(ue)) return false;
	if (!reader.Read(1, value)) return false;
	if (value && (!reader.ReadUe(ue) || !reader.ReadUe(ue) ||
		!reader.ReadUe(ue) || !reader.ReadUe(ue))) return false;
	if (!reader.ReadUe(ue) || ue > 7) return false;
	info.bitDepthLumaMinus8 = static_cast<byte>(ue);
	if (!reader.ReadUe(ue) || ue > 7) return false;
	info.bitDepthChromaMinus8 = static_cast<byte>(ue);
	return true;
}

bool BuildHvcC(const Bytes& vps, const Bytes& sps, const Bytes& pps, Bytes& output) {
	HevcSpsInfo info;
	if (!ParseSps(sps, info) || vps.Size() > 65535 || sps.Size() > 65535 || pps.Size() > 65535) {
		return false;
	}

	output.Clear();
	output.Write(static_cast<byte>(1));
	output.Write(static_cast<byte>((info.profileSpace << 6) |
		(info.tierFlag << 5) | info.profileIdc));
	for (int i = 3; i >= 0; --i) {
		output.Write(static_cast<byte>(info.compatibilityFlags >> (i * 8)));
	}
	for (int i = 5; i >= 0; --i) {
		output.Write(static_cast<byte>(info.constraintFlags >> (i * 8)));
	}
	output.Write(info.levelIdc);
	const byte fixedHeader[] = { 0xF0, 0x00, 0xFC, static_cast<byte>(0xFC | info.chromaFormat),
		static_cast<byte>(0xF8 | info.bitDepthLumaMinus8),
		static_cast<byte>(0xF8 | info.bitDepthChromaMinus8), 0x00, 0x00,
		static_cast<byte>((info.maxSubLayers << 3) |
			(info.temporalIdNested ? 0x04 : 0x00) | 0x03), 0x03 };
	output.Write(fixedHeader, sizeof(fixedHeader));

	const Bytes* parameterSets[] = { &vps, &sps, &pps };
	const byte nalTypes[] = { 32, 33, 34 };
	for (int i = 0; i < 3; ++i) {
		output.Write(static_cast<byte>(0x80 | nalTypes[i]));
		output.Write(static_cast<byte>(0));
		output.Write(static_cast<byte>(1));
		output.Write(static_cast<byte>(parameterSets[i]->Size() >> 8));
		output.Write(static_cast<byte>(parameterSets[i]->Size()));
		output.Write(parameterSets[i]->Data(), parameterSets[i]->Size());
	}
	return true;
}

bool BuildEbmlHeader(Array<byte>& buffer) {
	Array<byte> payload;
	AppendUIntElement(payload, 0x4286, 1);
	AppendUIntElement(payload, 0x42F7, 1);
	AppendUIntElement(payload, 0x42F2, 4);
	AppendUIntElement(payload, 0x42F3, 8);
	AppendStringElement(payload, 0x4282, "matroska");
	AppendUIntElement(payload, 0x4287, 4);
	AppendUIntElement(payload, 0x4285, 2);
	AppendElement(buffer, 0x1A45DFA3, payload);
	return true;
}

Bytes BuildTrackEntry(byte number, ullong uid, byte type,
	const char* codecId, const char* name) {
	Bytes payload;
	AppendUIntElement(payload, 0xD7, number);
	AppendUIntElement(payload, 0x73C5, uid);
	AppendUIntElement(payload, 0x83, type);
	AppendUIntElement(payload, 0x9C, 0);
	AppendStringElement(payload, 0x86, codecId);
	AppendStringElement(payload, 0x536E, name);
	return payload;
}

Bytes BuildTracks(uint width, uint height, uint framerate,
	const Bytes& hvcC, bool includeSys, bool includeMic, byte& sysTrack,
	byte& micTrack) {
	Bytes tracks;
	byte nextTrack = 1;
	const ullong uidSeed = GetTickCount64() * 1000 +
		(ullong)&tracks;

	Bytes video = BuildTrackEntry(nextTrack++, uidSeed + 1, 1, "V_MPEGH/ISO/HEVC", "Video");
	AppendElement(video, 0x63A2, hvcC);
	AppendUIntElement(video, 0x23E383, 1000000000ULL / framerate);
	Bytes videoSettings;
	AppendUIntElement(videoSettings, 0xB0, width);
	AppendUIntElement(videoSettings, 0xBA, height);
	Bytes colour;
	AppendUIntElement(colour, 0x55B9, 2);
	AppendElement(videoSettings, 0x55B0, colour);
	AppendElement(video, 0xE0, videoSettings);
	AppendElement(tracks, 0xAE, video);

	auto addAudio = [&](const char* name, byte& track) {
		track = nextTrack++;
		Bytes entry = BuildTrackEntry(track, uidSeed + track, 2, "A_PCM/INT/LIT", name);
		Bytes audio;
		AppendFloatElement(audio, 0xB5, 48000.0);
		AppendUIntElement(audio, 0x9F, 2);
		AppendUIntElement(audio, 0x6264, 16);
		AppendElement(entry, 0xE1, audio);
		AppendElement(tracks, 0xAE, entry);
		};

	sysTrack = 0;
	micTrack = 0;
	if (includeSys) addAudio("System audio", sysTrack);
	if (includeMic) addAudio("Microphone", micTrack);
	Bytes result;
	AppendElement(result, 0x1654AE6B, tracks);
	return result;
}

bool ReadAudioChunk(FileHandle& file, Bytes& payload,
	ullong& timestamp, bool& hasChunk) {
	hasChunk = false;
	while (file.Pointer() < file.Size()) {
		const ullong remaining = static_cast<ullong>(file.Size() - file.Pointer());
		if (remaining < sizeof(uint) + sizeof(ullong)) return false;

		uint size = 0;
		if (!file.Read(&size, sizeof(size)) || !file.Read(&timestamp, sizeof(timestamp))) return false;
		if (size == 0) continue;
		if (size % 4 != 0 || size > file.Size() - file.Pointer()) return false;

		payload.Resize(size);
		if (!file.Read(payload.Data(), size)) return false;
		hasChunk = true;
		return true;
	}
	return true;
}

Bytes BuildCues(const Array<Cue>& cues) {
	Bytes cuePoints;
	for (ullong i = 0; i < cues.Size(); ++i) {
		const Cue& cue = cues[i];
		Bytes cuePoint;
		AppendUIntElement(cuePoint, 0xB3, cue.timecode);
		Bytes trackPositions;
		AppendUIntElement(trackPositions, 0xF7, cue.track);
		AppendUIntElement(trackPositions, 0xF1, cue.clusterOffset);
		AppendElement(cuePoint, 0xB7, trackPositions);
		AppendElement(cuePoints, 0xBB, cuePoint);
	}
	Bytes result;
	if (cues.Size()) AppendElement(result, 0x1C53BB6B, cuePoints);
	return result;
}

bool Muxer::Save() {
	wchar_t path[260]{};
	if (wcscpy_s(path, 260, Config::internal.path) != 0) { return false; }

	FileHandle sysPcm;
	FileHandle micPcm;

	if (Config::user.audio.captureSys) {
		wchar_t f[260]{};
		memcpy(f, path, 260);
		wcscat_s(f, 260, L".syspcm");
		sysPcm.Initialize(f, true);
		if (sysPcm.lastError) { return false; }
	}
	if (Config::user.audio.captureMic) {
		wchar_t f[260]{};
		memcpy(f, path, 260);
		wcscat_s(f, 260, L".micpcm");
		micPcm.Initialize(f, true);
		if (micPcm.lastError) { return false; }
	}

	wchar_t f[260]{};
	memcpy(f, path, 260);
	wcscat_s(f, 260, L".hevc");

	FileHandle hevc(f, true);
	if (hevc.lastError) { return false; }

	memcpy(f, path, 260);
	wcscat_s(f, 260, L".mkv");

	FileHandle output(f, false);
	if (output.lastError) { return false; }

	const ullong videoDataSize = hevc.Size() - sizeof(ullong);
	ullong totalTime = 0;
	if (!hevc.Seek(videoDataSize, FILE_BEGIN) || !hevc.Read(&totalTime, sizeof(totalTime)) || !hevc.Seek(0, FILE_BEGIN)) { return false; }

	Bytes vps;
	Bytes sps;
	Bytes pps;
	while (hevc.Pointer() < videoDataSize &&
		(!vps.Size() || !sps.Size() || !pps.Size())) {
		if (videoDataSize - hevc.Pointer() < sizeof(Frame)) { return false; }
		Frame frame{};

		if (!hevc.Read(&frame, sizeof(frame)) || frame.size == 0 || frame.size == INT_MIN) { return false; }

		const uint size = frame.size < 0 ? -frame.size : frame.size;
		if (!size || size > videoDataSize - hevc.Pointer()) { return false; }
		Bytes frameData;
		frameData.Resize(size);
		if (!hevc.Read(frameData.Data(), size) || !ParseAnnexB(frameData, nullptr, &vps, &sps, &pps)) {
			return false;
		}
	}
	if (!vps.Size() || !sps.Size() || !pps.Size()) { return false; }

	Bytes hvcC;
	if (!BuildHvcC(vps, sps, pps, hvcC)) return false;

	ullong sysDuration = Config::user.audio.captureSys ? Config::internal.sysAudioMs : 0;
	ullong micDuration = Config::user.audio.captureMic ? Config::internal.micAudioMs : 0;

	const ullong duration = totalTime >= sysDuration ?
		(totalTime >= micDuration ? totalTime : micDuration) :
		(sysDuration >= micDuration ? sysDuration : micDuration);


	Bytes metadata;
	BuildEbmlHeader(metadata);
	const byte segmentId[] = { 0x18, 0x53, 0x80, 0x67 };
	const byte unknownSize[] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	const ullong metadataStart = static_cast<ullong>(output.Pointer());
	metadata.Write(segmentId, sizeof(segmentId));
	metadata.Write(unknownSize, sizeof(unknownSize));
	const ullong segmentDataOffset = metadataStart + metadata.Size();

	Bytes info;
	AppendUIntElement(info, 0x2AD7B1, 1000000);
	AppendFloatElement(info, 0x4489, static_cast<double>(duration));
	AppendStringElement(info, 0x4D80, "hiRecorder");
	AppendStringElement(info, 0x5741, "hiRecorder");
	AppendElement(metadata, 0x1549A966, info);

	byte sysTrack = 0;
	byte micTrack = 0;
	Bytes trackElement = BuildTracks(Config::internal.width, Config::internal.height,
		Config::user.video.framerate, hvcC, Config::user.audio.captureSys,
		Config::user.audio.captureMic, sysTrack, micTrack);
	metadata.Write(trackElement.Data(), trackElement.Size());

	if (!output.Write(metadata.Data(), metadata.Size())) return false;

	struct AudioState {
		FileHandle* file = nullptr;
		byte track = 0;
		ullong timestamp = 0;
		Bytes payload;
		bool hasChunk = false;
	} sysState{ Config::user.audio.captureSys ? &sysPcm : nullptr, sysTrack },
		micState{ Config::user.audio.captureMic ? &micPcm : nullptr, micTrack };

	if (sysState.file && !ReadAudioChunk(*sysState.file, sysState.payload,
		sysState.timestamp, sysState.hasChunk)) return false;
	if (micState.file && !ReadAudioChunk(*micState.file, micState.payload,
		micState.timestamp, micState.hasChunk)) return false;

	if (!hevc.Seek(0, FILE_BEGIN)) return false;
	Frame videoFrame{};
	Bytes annexB;
	Bytes videoData;
	bool hasVideo = false;
	auto readVideo = [&]() {
		hasVideo = false;
		if (static_cast<ullong>(hevc.Pointer()) == videoDataSize) return true;
		if (videoDataSize - hevc.Pointer() < sizeof(Frame) || !hevc.Read(&videoFrame, sizeof(videoFrame)) ||
			videoFrame.size == 0 || videoFrame.size == INT_MIN) return false;
		const uint size = static_cast<uint>(videoFrame.size < 0 ? -videoFrame.size : videoFrame.size);
		if (!size || size > videoDataSize - hevc.Pointer()) return false;
		annexB.Resize(size);
		if (!hevc.Read(annexB.Data(), size) || !ParseAnnexB(annexB, &videoData)) return false;
		hasVideo = true;
		return true;
		};

	if (!readVideo() || !hasVideo) return false;
	Array<Cue> cues;
	bool clusterActive = false;
	ullong clusterTime = 0;
	ullong clusterOffset = 0;
	ullong clusterFileOffset = 0;
	bool wroteVideo = false;

	while (hasVideo || sysState.hasChunk || micState.hasChunk) {
		AudioState* audioState = nullptr;
		if (sysState.hasChunk) audioState = &sysState;
		if (micState.hasChunk && (!audioState || micState.timestamp < audioState->timestamp)) audioState = &micState;
		const bool writeVideo = hasVideo && (!audioState || videoFrame.timestamp <= audioState->timestamp);
		const ullong eventTime = writeVideo ? videoFrame.timestamp : audioState->timestamp;
		const bool keyframe = writeVideo && videoFrame.size < 0;
		const llong relative = clusterActive ? static_cast<llong>(eventTime) -
			static_cast<llong>(clusterTime) : 0;
		const bool keyframeSplit = keyframe && eventTime - clusterTime >= 1000;
		const bool timeSplit = clusterActive && (eventTime - clusterTime >= 30000 ||
			relative < SHRT_MIN ||
			relative > SHRT_MAX);

		if (!clusterActive || keyframeSplit || timeSplit) {
			clusterTime = eventTime;
			clusterFileOffset = static_cast<ullong>(output.Pointer());
			Bytes clusterHeader;
			AppendId(clusterHeader, 0x1F43B675);
			const byte unknownClusterSize[] = { 0x01, 0xFF, 0xFF, 0xFF,
				0xFF, 0xFF, 0xFF, 0xFF };
			clusterHeader.Write(unknownClusterSize, sizeof(unknownClusterSize));
			AppendUIntElement(clusterHeader, 0xE7, clusterTime);
			if (!output.Write(clusterHeader.Data(), clusterHeader.Size())) return false;
			clusterOffset = clusterFileOffset - segmentDataOffset;
			clusterActive = true;
		}

		const llong relativeTime = static_cast<llong>(eventTime) -
			static_cast<llong>(clusterTime);
		if (relativeTime < SHRT_MIN ||
			relativeTime > SHRT_MAX) return false;

		if (writeVideo) {
			if (videoData.Size() > UINT_MAX ||
				!WriteSimpleBlock(output, 1, static_cast<short>(relativeTime), keyframe,
					videoData.Data(), static_cast<uint>(videoData.Size()))) return false;
			if (keyframe) cues.Write(Cue{ eventTime, clusterOffset, 1 });
			wroteVideo = true;
			if (!readVideo()) return false;
		}
		else {
			if (audioState->payload.Size() > UINT_MAX ||
				!WriteSimpleBlock(output, audioState->track, static_cast<short>(relativeTime), true,
					audioState->payload.Data(), static_cast<uint>(audioState->payload.Size()))) return false;
			if (!ReadAudioChunk(*audioState->file, audioState->payload,
				audioState->timestamp, audioState->hasChunk)) return false;
		}
	}

	if (!wroteVideo) return false;
	if (!output.Seek(0, FILE_END)) return false;

	Bytes cueElement = BuildCues(cues);
	if (output.Write(cueElement.Data(), cueElement.Size())) {
		sysPcm.Delete();
		micPcm.Delete();
		hevc.Delete();
		return true;
	}

	return false;
}

bool Muxer::WriteSimpleBlock(FileHandle& file, byte trackNum,
	short relativeTimeMs, bool isKeyframe,
	const byte* payload, uint payloadSize) {
	if (!trackNum || trackNum > 0x7F || !payload || !payloadSize) return false;
	const ullong blockSize = 1 + 2 + 1 + static_cast<ullong>(payloadSize);
	int sizeVintLength = 1;
	while (sizeVintLength <= 8 && blockSize > (1ULL << (sizeVintLength * 7)) - 2) ++sizeVintLength;
	if (sizeVintLength > 8) return false;

	byte header[16]{};
	ullong headerSize = 0;
	header[headerSize++] = 0xA3;
	ullong sizeWithMarker = blockSize | (1ULL << (sizeVintLength * 7));
	for (int i = sizeVintLength - 1; i >= 0; --i) {
		header[headerSize++] = static_cast<byte>(sizeWithMarker >> (i * 8));
	}
	header[headerSize++] = static_cast<byte>(0x80 | trackNum);
	const uint timeBits = static_cast<uint>(relativeTimeMs);
	header[headerSize++] = static_cast<byte>(timeBits >> 8);
	header[headerSize++] = static_cast<byte>(timeBits);
	header[headerSize++] = isKeyframe ? 0x80 : 0x00;
	return file.Write(header, static_cast<unsigned long>(headerSize)) && file.Write(payload, payloadSize);
}