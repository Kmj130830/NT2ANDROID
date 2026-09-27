#include "nt2core/m9p_scanner.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace nt2 {
namespace {

constexpr std::array<std::uint8_t, 4> kM9p = {
    static_cast<std::uint8_t>('M'),
    static_cast<std::uint8_t>('9'),
    static_cast<std::uint8_t>('P'),
    static_cast<std::uint8_t>('@')
};

constexpr std::array<std::array<char, 4>, 4> kTags = {{
    {{'v', 'e', 'r', 0}},
    {{'h', 'e', 'a', 'd'}},
    {{'h', 'a', 'r', 'm'}},
    {{'r', 'r', 'e', 's'}}
}};

bool matchesM9p(const std::vector<std::uint8_t>& bytes, std::size_t p) {
    if (p + kM9p.size() > bytes.size()) return false;
    return std::equal(kM9p.begin(), kM9p.end(), bytes.begin() + p);
}

bool matchesTag(
    const std::vector<std::uint8_t>& bytes,
    std::size_t p,
    const std::array<char, 4>& tag
) {
    if (p + 4 > bytes.size()) return false;
    for (std::size_t i = 0; i < 4; ++i) {
        if (static_cast<char>(bytes[p + i]) != tag[i]) return false;
    }
    return true;
}

} // namespace

std::vector<M9pRecord> M9pScanner::scan(
    const std::vector<std::uint8_t>& bytes,
    std::string* warning
) {
    std::vector<std::size_t> offsets;

    for (std::size_t p = 0; p + kM9p.size() <= bytes.size(); ++p) {
        if (matchesM9p(bytes, p)) offsets.push_back(p);
    }

    std::vector<M9pRecord> records;
    records.reserve(offsets.size());

    for (std::size_t i = 0; i < offsets.size(); ++i) {
        const std::size_t start = offsets[i];
        const std::size_t end =
            (i + 1 < offsets.size()) ? offsets[i + 1] : bytes.size();

        M9pRecord record;
        record.offset = start;
        record.endOffset = end;

        for (std::size_t p = start + kM9p.size(); p + 4 <= end; ++p) {
            for (const auto& tag : kTags) {
                if (!matchesTag(bytes, p, tag)) continue;

                M9pSection section;
                section.tag.assign(
                    reinterpret_cast<const char*>(bytes.data() + p), 4);
                section.offset = p;
                section.payloadOffset = p + 4;

                // This is only the distance to the end of the containing M9P.
                // It is NOT the confirmed section length.
                section.payloadSize =
                    static_cast<std::uint64_t>(end - section.payloadOffset);

                record.sections.push_back(std::move(section));
                break;
            }
        }

        records.push_back(std::move(record));
    }

    if (warning && records.empty()) {
        *warning = "No M9P@ signatures were found.";
    }

    return records;
}

} // namespace nt2
