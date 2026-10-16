#pragma once
#include "../Utils/Array.hpp"

using namespace Data::Types;
using Bytes = Array<byte>;

class BitReader {
public:
	explicit BitReader(const Bytes& bytes) : data(bytes) {}

	bool Read(unsigned int count, ullong& value);
	bool ReadUe(uint& value);

private:
	const Bytes& data;
	ullong bitPosition = 0;

};
