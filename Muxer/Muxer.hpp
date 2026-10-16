#pragma once
#include "../Utils/FileHandle.hpp"
#include "../Utils/Data.hpp"

using namespace Data::Types;

struct Frame {
    int size;
    uint timestamp;
};

struct Cue {
    ullong timecode = 0;
    ullong clusterOffset = 0;
    byte track = 0;
};

class Muxer {
public:
    bool Save();

private:

    bool WriteSimpleBlock(FileHandle& file, byte trackNum, short relativeTimeMs, bool isKeyframe, const byte* payload, uint payloadSize);
};