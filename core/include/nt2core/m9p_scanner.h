#pragma once

#include "nt2core/export.h"

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

struct M9pVersion {
    bool tagPresent = false;
    std::uint32_t declaredSize = 0;
    bool hasValue = false;
    std::uint32_t value = 0;
};

struct M9pHead {
    std::uint32_t declaredSize = 0;
    std::uint32_t flags = 0;
    float field08F32 = 0.0f;
    double referenceFrequency = 0.0;
    std::uint32_t sampleCount = 0;
};

struct M9pRecord {
    std::uint64_t offset = 0;
    std::uint64_t endOffset = 0;
    std::uint32_t declaredBodySize = 0;
    bool lengthMatches = false;
    M9pVersion version;
    std::vector<M9pSection> sections;
    bool hasHead = false;
    M9pHead head;
};

class NT2CORE_API M9pScanner {
public:
    static std::vector<M9pRecord> scan(
        const std::vector<std::uint8_t>& bytes,
        std::string* warning = nullptr
    );
};

} // namespace nt2
