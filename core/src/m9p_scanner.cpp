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

constexpr std::array<std::uint8_t, 4> kVer = {
    static_cast<std::uint8_t>('v'),
    static_cast<std::uint8_t>('e'),
    static_cast<std::uint8_t>('r'),
    static_cast<std::uint8_t>(' ')
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

std::uint32_t readU32LE(
    const std::vector<std::uint8_t>& bytes,
    std::size_t p
) {
    return static_cast<std::uint32_t>(bytes[p])
        | (static_cast<std::uint32_t>(bytes[p + 1]) << 8)
        | (static_cast<std::uint32_t>(bytes[p + 2]) << 16)
        | (static_cast<std::uint32_t>(bytes[p + 3]) << 24);
}

float readF32LE(
    const std::vector<std::uint8_t>& bytes,
    std::size_t p
) {
    const std::uint32_t bits = readU32LE(bytes, p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

double readF64LE(
    const std::vector<std::uint8_t>& bytes,
    std::size_t p
) {
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
        const std::size_t markerNext =
            (i + 1 < offsets.size()) ? offsets[i + 1] : bytes.size();
        const bool isLast = i + 1 == offsets.size();

        M9pRecord record;
        record.offset = start;
        record.isLastRecord = isLast;

        if (start + 8 <= bytes.size()) {
            record.declaredBodySize = readU32LE(bytes, start + 4);

            if (record.declaredBodySize <=
                bytes.size() - start - 16) {
                record.declaredEndOffset =
                    start + 16u + record.declaredBodySize;

                record.nextMarkerMatchesDeclaredEnd =
                    record.declaredEndOffset == markerNext;

                if (isLast) {
                    record.trailingBytesAfterRecord =
                        markerNext >= record.declaredEndOffset
                            ? markerNext - record.declaredEndOffset
                            : 0;
                }

                // Prefer the record's own declared size. This is important
                // for the last record because NTDB may contain trailing data
                // after the final M9P record.
                record.endOffset = record.declaredEndOffset;
                record.lengthMatches =
                    record.endOffset >= start
                    && record.endOffset <= bytes.size()
                    && (!isLast
                        ? record.nextMarkerMatchesDeclaredEnd
                        : true);
            } else {
                // Invalid declared end. Fall back to the next marker so the
                // inspection tool can still report the damaged record.
                record.endOffset = markerNext;
                record.lengthMatches = false;
            }
        } else {
            record.endOffset = markerNext;
        }

        if (record.endOffset <= start || record.endOffset > bytes.size()) {
            record.endOffset = markerNext;
            record.lengthMatches = false;
        }

        const std::size_t end = static_cast<std::size_t>(record.endOffset);

        const std::size_t verOffset = start + 8;
        if (verOffset + 8 <= end && matches4(bytes, verOffset, kVer)) {
            record.version.tagPresent = true;
            record.version.declaredSize = readU32LE(bytes, verOffset + 4);

            if (record.version.declaredSize <= end - (verOffset + 8)
                && record.version.declaredSize == 4) {
                record.version.value = readU32LE(bytes, verOffset + 8);
                record.version.hasValue = true;
            }
        }

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

        record.head.declaredSize = readU32LE(bytes, headPayload);
        record.head.flags = readU32LE(bytes, headPayload + 4);
        record.head.field08F32 = readF32LE(bytes, headPayload + 8);
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
            *warning = "Some M9P records did not match the observed "
                       "head/harm/rres structural layout.";
        } else {
            warning->clear();
        }
    }

    return records;
}

} // namespace nt2
