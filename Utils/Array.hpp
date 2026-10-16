#pragma once
#include "Data.hpp"

#include <cstdint>

using namespace Data::Types;

template <typename t>
class Array {
public:
	Array() = default;
	explicit Array(ullong capacity) { Reserve(capacity); }
	Array(const Array& other) {
		Reserve(other.elements);
		for (; pointer < other.pointer; ++pointer) data[pointer] = other.data[pointer];
	}
	Array(Array&& other) noexcept : pointer(other.pointer), elements(other.elements), data(other.data) {
		other.pointer = 0;
		other.elements = 0;
		other.data = nullptr;
	}
	Array& operator=(const Array& other) {
		if (this != &other) {
			Clear();
			Reserve(other.pointer);
			Write(other.data, other.pointer);
		}
		return *this;
	}
	Array& operator=(Array&& other) noexcept {
		if (this != &other) {
			Delete();
			pointer = other.pointer;
			elements = other.elements;
			data = other.data;
			other.pointer = 0;
			other.elements = 0;
			other.data = nullptr;
		}
		return *this;
	}
	~Array() { Delete(); }

	ullong Pointer() const { return pointer; }
	t* Data() { return data; }
	const t* Data() const { return data; }
	void Delete() {
		delete[] data;
		data = nullptr;
		pointer = 0;
		elements = 0;
	}
	void Clear() { pointer = 0; }
	ullong Size() const { return pointer; }
	ullong Bytes() const { return elements * sizeof(t); }
	void Reserve(ullong capacity) {
		if (capacity <= elements) return;
		t* replacement = new t[capacity]{};
		for (ullong i = 0; i < pointer; ++i) replacement[i] = data[i];
		delete[] data;
		data = replacement;
		elements = capacity;
	}
	void Resize(ullong size) {
		Reserve(size);
		if (size > pointer) {
			for (ullong i = pointer; i < size; ++i) data[i] = t{};
		}
		pointer = size;
	}
	t& operator[](ullong index) { return data[index]; }
	const t& operator[](ullong index) const { return data[index]; }

	bool Write(t source) {
		EnsureCapacity(pointer + 1);
		data[pointer++] = source;
		return true;
	}

	bool Write(const t* source, ullong size) {
		if (!size) return true;
		if (!source) return false;
		const ullong newSize = pointer + size;
		if (newSize < pointer) return false;
		const std::uintptr_t sourceAddress = reinterpret_cast<std::uintptr_t>(source);
		const std::uintptr_t dataAddress = reinterpret_cast<std::uintptr_t>(data);
		const bool internalSource = data && sourceAddress >= dataAddress &&
			sourceAddress < dataAddress + pointer * sizeof(t);
		const ullong sourceOffset = internalSource ?
			static_cast<ullong>((sourceAddress - dataAddress) / sizeof(t)) : 0;
		EnsureCapacity(newSize);
		if (internalSource) source = data + sourceOffset;
		for (ullong i = 0; i < size; ++i) data[pointer++] = source[i];
		return true;
	}

private:
	void EnsureCapacity(ullong required) {
		if (required <= elements) return;
		ullong capacity = elements ? elements : 1;
		while (capacity < required) {
			if (capacity > ~static_cast<ullong>(0) / 2) {
				capacity = required;
				break;
			}
			capacity *= 2;
		}
		Reserve(capacity);
	}

	ullong pointer = 0;
	ullong elements = 0;
	t* data = nullptr;

};