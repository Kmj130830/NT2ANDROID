#include "nt2core/byte_reader.h"

#include <cstring>

namespace nt2 {

ByteReader::ByteReader(const std::uint8_t* data, std::size_t size) noexcept
    : data_(data), size_(size), position_(0) {
}

bool ByteReader::canRead(std::size_t count) const noexcept {
    return position_ <= size_ && count <= size_ - position_;
}

std::size_t ByteReader::position() const noexcept {
    return position_;
}

std::size_t ByteReader::remaining() const noexcept {
    return size_ - position_;
}

const std::uint8_t* ByteReader::current() const noexcept {
    return data_ + position_;
}

bool ByteReader::skip(std::size_t count) noexcept {
    if (!canRead(count)) return false;
    position_ += count;
    return true;
}

bool ByteReader::readU8(std::uint8_t& value) noexcept {
    if (!canRead(1)) return false;
    value = data_[position_++];
    return true;
}

bool ByteReader::readU16LE(std::uint16_t& value) noexcept {
    if (!canRead(2)) return false;
    value = static_cast<std::uint16_t>(data_[position_])
        | (static_cast<std::uint16_t>(data_[position_ + 1]) << 8);
    position_ += 2;
    return true;
}

bool ByteReader::readU32LE(std::uint32_t& value) noexcept {
    if (!canRead(4)) return false;
    value = static_cast<std::uint32_t>(data_[position_])
        | (static_cast<std::uint32_t>(data_[position_ + 1]) << 8)
        | (static_cast<std::uint32_t>(data_[position_ + 2]) << 16)
        | (static_cast<std::uint32_t>(data_[position_ + 3]) << 24);
    position_ += 4;
    return true;
}

bool ByteReader::readU64LE(std::uint64_t& value) noexcept {
    if (!canRead(8)) return false;
    value = static_cast<std::uint64_t>(data_[position_])
        | (static_cast<std::uint64_t>(data_[position_ + 1]) << 8)
        | (static_cast<std::uint64_t>(data_[position_ + 2]) << 16)
        | (static_cast<std::uint64_t>(data_[position_ + 3]) << 24)
        | (static_cast<std::uint64_t>(data_[position_ + 4]) << 32)
        | (static_cast<std::uint64_t>(data_[position_ + 5]) << 40)
        | (static_cast<std::uint64_t>(data_[position_ + 6]) << 48)
        | (static_cast<std::uint64_t>(data_[position_ + 7]) << 56);
    position_ += 8;
    return true;
}

bool ByteReader::readI16LE(std::int16_t& value) noexcept {
    std::uint16_t bits = 0;
    if (!readU16LE(bits)) return false;
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

bool ByteReader::readF64LE(double& value) noexcept {
    std::uint64_t bits = 0;
    if (!readU64LE(bits)) return false;
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

} // namespace nt2
