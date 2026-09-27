#pragma once

#include "nt2core/export.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nt2 {

struct NtdbSummary {
    bool validMagic = false;
    std::uint64_t fileSize = 0;
    std::uint64_t m9pCount = 0;
    std::uint64_t firstM9pOffset = 0;
    std::uint64_t lastM9pOffset = 0;
};

class NT2CORE_API NtdbReader {
public:
    bool open(const std::string& path, std::string* error = nullptr);

    const std::vector<std::uint8_t>& bytes() const noexcept {
        return bytes_;
    }

    NtdbSummary inspect() const noexcept;

private:
    std::vector<std::uint8_t> bytes_;
};

} // namespace nt2
