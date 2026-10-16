#include "BitReader.hpp"

#include <limits>

bool BitReader::Read(unsigned int count, ullong& value) {
	if (count > 64 || bitPosition + count > data.Size() * 8) { return false; }
	value = 0;

	for (unsigned int i = 0; i < count; ++i) {
		value = (value << 1) | ((data[bitPosition / 8] >> (7 - bitPosition % 8)) & 1);
		++bitPosition;
	}
	return true;
}

bool BitReader::ReadUe(uint& value) {
	unsigned int leadingZeros = 0;
	ullong bit = 0;

	while (true) {
		if (!Read(1, bit)) { return false; }
		if (bit) { break; }
		if (++leadingZeros > 31) { return false; }
	}

	ullong suffix = 0;
	if (leadingZeros && !Read(leadingZeros, suffix)) { return false; }

	const ullong decoded = ((1ULL << leadingZeros) - 1) + suffix;
	if (decoded > std::numeric_limits<uint>::max()) { return false; }

	value = static_cast<uint>(decoded);
	return true;
}