#pragma once

#include "nt2core/export.h"

#include <cstddef>
#include <cstdint>

namespace nt2 {

class NT2CORE_API ByteReader {
public:
    ByteReader(const std::uint8_t* data, std::size_t size) noexcept;

    bool canRead(std::size_t count) const noexcept;
    std::size_t position() const noexcept;
    std::size_t remaining() const noexcept;
    const std::uint8_t* current() const noexcept;

    bool skip(std::size_t count) noexcept;

    bool readU8(std::uint8_t& value) noexcept;
    bool readU16LE(std::uint16_t& value) noexcept;
    bool readU32LE(std::uint32_t& value) noexcept;
    bool readU64LE(std::uint64_t& value) noexcept;
    bool readI16LE(std::int16_t& value) noexcept;
    bool readF64LE(double& value) noexcept;

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t position_;
};

} // namespace nt2
