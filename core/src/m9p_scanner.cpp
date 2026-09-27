#include "nt2core/m9p_scanner.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <utility>

namespace nt2 {
namespace {

constexpr std::array<std::uint8_t, 4> kM9p = {
    static_cast<std::uint8_t>('M'),
    static_cast<std::uint8_t>('9'),
    static_cast<std::uint8_t>('P'),
    static_cast<std::uint8_t>('@')
};

constexpr std::array<std::uint8_t, 4> kHead = {
    static_cast<std::uint8_t>('h'),
    static_cast<std::uint8_t>('e'),
    static_cast<std::uint8_t>('a'),
    static_cast<std::uint8_t>('d')
};

constexpr std::array<std::uint8_t, 4> kHarm = {
    static_cast<std::uint8_t>('h'),
    static_cast<std::uint8_t>('a'),
    static_cast<std::uint8_t>('r'),
    static_cast<std::uint8_t>('m')
};

constexpr std::array<std::uint8_t, 4> kRres = {
    static_cast<std::uint8_t>('r'),
    static_cast<std::uint8_t>('r'),
    static_cast<std::uint8_t>('e'),
    static_cast<std::uint8_t>('s')
};

bool matches4(
    const std::vector<std::uint8_t>& bytes,
    std::size_t p,
    const std::array<std::uint8_t, 4>& tag
) {
    if (p > bytes.size() || bytes.size() - p < tag.size()) return false;
    return std::equal(tag.begin(), tag.end(), bytes.begin() + p);
}

std::size_t findTag(
    const std::vector<std::uint8_t>& bytes,
    std::size_t begin,
    std::size_t end,
    const std::array<std::uint8_t, 4>& tag
) {
    if (begin > end || end > bytes.size()) return bytes.size();

    for (std::size_t p = begin; p + tag.size() <= end; ++p) {
        if (matches4(bytes, p, tag)) return p;
    }

    return bytes.size();
}

std::uint32_t readU32LE(const std::vector<std::uint8_t>& bytes, std::size_t p) {
    return static_cast<std::uint32_t>(bytes[p])
        | (static_cast<std::uint32_t>(bytes[p + 1]) << 8)
        | (static_cast<std::uint32_t>(bytes[p + 2]) << 16)
        | (static_cast<std::uint32_t>(bytes[p + 3]) << 24);
}

float readF32LE(const std::vector<std::uint8_t>& bytes, std::size_t p) {
    const std::uint32_t bits = readU32LE(bytes, p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

double readF64LE(const std::vector<std::uint8_t>& bytes, std::size_t p) {
    std::uint64_t bits =
        static_cast<std::uint64_t>(bytes[p])
        | (static_cast<std::uint64_t>(bytes[p + 1]) << 8)
        | (static_cast<std::uint64_t>(bytes[p + 2]) << 16)
        | (static_cast<std::uint64_t>(bytes[p + 3]) << 24)
        | (static_cast<std::uint64_t>(bytes[p + 4]) << 32)
        | (static_cast<std::uint64_t>(bytes[p + 5]) << 40)
        | (static_cast<std::uint64_t>(bytes[p + 6]) << 48)
        | (static_cast<std::uint64_t>(bytes[p + 7]) << 56);

    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void addSection(
    M9pRecord& record,
    const char* tag,
    std::size_t tagOffset,
    std::size_t tagSize,
    std::size_t nextOffset
) {
    M9pSection section;
    section.tag = tag;
    section.offset = tagOffset;
    section.payloadOffset = tagOffset + tagSize;
    section.payloadSize =
        nextOffset >= section.payloadOffset
            ? static_cast<std::uint64_t>(nextOffset - section.payloadOffset)
            : 0;
    record.sections.push_back(std::move(section));
}

} // namespace

std::vector<M9pRecord> M9pScanner::scan(
    const std::vector<std::uint8_t>& bytes,
    std::string* warning
) {
    std::vector<std::size_t> offsets;

    for (std::size_t p = 0; p + kM9p.size() <= bytes.size(); ++p) {
        if (matches4(bytes, p, kM9p)) offsets.push_back(p);
    }

    std::vector<M9pRecord> records;
    records.reserve(offsets.size());

    std::size_t malformed = 0;

    for (std::size_t i = 0; i < offsets.size(); ++i) {
        const std::size_t start = offsets[i];
        const std::size_t end =
            (i + 1 < offsets.size()) ? offsets[i + 1] : bytes.size();

        M9pRecord record;
        record.offset = start;
        record.endOffset = end;

        // The observed on-disk invariant is:
        // M9P@ + 0x14 bytes -> head tag.
        const std::size_t headOffset = start + 0x14;
        if (headOffset + 4 > end || !matches4(bytes, headOffset, kHead)) {
            ++malformed;
            records.push_back(std::move(record));
            continue;
        }

        const std::size_t headPayload = headOffset + 4;
        if (headPayload + 24 > end) {
            ++malformed;
            records.push_back(std::move(record));
            continue;
        }

        // head payload:
        // u32 declaredSize (=20 in the supplied DB)
        // u32 flags/reserved (=0 in observed records)
        // f32 sampleRate
        // f64 referenceFrequency
        // u32 sampleCount
        record.head.declaredSize = readU32LE(bytes, headPayload);
        record.head.flags = readU32LE(bytes, headPayload + 4);
        record.head.sampleRate = readF32LE(bytes, headPayload + 8);
        record.head.referenceFrequency = readF64LE(bytes, headPayload + 12);
        record.head.sampleCount = readU32LE(bytes, headPayload + 20);
        record.hasHead = true;

        const std::size_t harmOffset = headPayload + 24;
        if (harmOffset + 4 > end || !matches4(bytes, harmOffset, kHarm)) {
            ++malformed;
            records.push_back(std::move(record));
            continue;
        }

        const std::size_t rresOffset =
            findTag(bytes, harmOffset + 4, end, kRres);

        if (rresOffset == bytes.size()) {
            ++malformed;
            records.push_back(std::move(record));
            continue;
        }

        addSection(record, "head", headOffset, 4, harmOffset);
        addSection(record, "harm", harmOffset, 4, rresOffset);
        addSection(record, "rres", rresOffset, 4, end);

        records.push_back(std::move(record));
    }

    if (warning) {
        if (records.empty()) {
            *warning = "No M9P@ signatures were found.";
        } else if (malformed != 0) {
            *warning = "Some M9P records did not match the currently observed "
                       "head/harm/rres structural layout.";
        } else {
            warning->clear();
        }
    }

    return records;
}

} // namespace nt2
