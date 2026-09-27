#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nt2 {

struct M9pSection {
    std::string tag;
    std::uint64_t offset = 0;
    std::uint64_t payloadOffset = 0;
    std::uint64_t payloadSize = 0;
};

struct M9pRecord {
    std::uint64_t offset = 0;
    std::uint64_t endOffset = 0;
    std::vector<M9pSection> sections;
};

class M9pScanner {
public:
    static std::vector<M9pRecord> scan(
        const std::vector<std::uint8_t>& bytes,
        std::string* warning = nullptr
    );
};

} // namespace nt2
