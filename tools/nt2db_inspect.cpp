#include "nt2core/ntdb_reader.h"
#include "nt2core/m9p_scanner.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

std::uint32_t readU32LE(
    const std::vector<std::uint8_t>& bytes,
    std::size_t offset
) {
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
        | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

void printHex(
    const std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::size_t count
) {
    const std::size_t available =
        offset < bytes.size() ? bytes.size() - offset : 0;
    const std::size_t n = std::min(count, available);

    for (std::size_t i = 0; i < n; ++i) {
        if (i != 0) std::cout << ' ';
        std::cout << std::setw(2) << std::setfill('0')
                  << std::hex << static_cast<unsigned>(bytes[offset + i]);
    }
    std::cout << std::setfill(' ') << std::dec << '\n';
}

void printHead(const nt2::M9pHead& head) {
    std::cout << "    declared : " << head.declaredSize << " bytes\n";
    std::cout << "    flags    : 0x" << std::hex << head.flags
              << std::dec << '\n';
    std::cout << std::setprecision(10);
    std::cout << "    field+08 : " << head.field08F32 << " (float)\n";
    std::cout << "    referenceFrequency : "
              << head.referenceFrequency << " Hz\n";
    std::cout << "    sampleCount : " << head.sampleCount << '\n';
    std::cout << std::setprecision(6);
}

const nt2::M9pSection* findSection(
    const nt2::M9pRecord& record,
    const std::string& tag
) {
    for (const auto& section : record.sections) {
        if (section.tag == tag) return &section;
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: nt2db_inspect <file.ntdb>\n";
        return 2;
    }

    nt2::NtdbReader reader;
    std::string error;

    if (!reader.open(argv[1], &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    const auto summary = reader.inspect();

    std::cout << "NTDB file size : " << summary.fileSize << " bytes\n";
    std::cout << "M9DB magic     : "
              << (summary.validMagic ? "yes" : "no") << '\n';
    std::cout << "M9P records    : " << summary.m9pCount << '\n';

    if (summary.m9pCount != 0) {
        std::cout << "First M9P      : 0x"
                  << std::hex << summary.firstM9pOffset
                  << std::dec << '\n';
        std::cout << "Last M9P       : 0x"
                  << std::hex << summary.lastM9pOffset
                  << std::dec << '\n';
    }

    std::string warning;
    const auto records = nt2::M9pScanner::scan(reader.bytes(), &warning);

    std::size_t structurallyValid = 0;
    std::size_t lengthValid = 0;
    std::size_t versionTagPresent = 0;
    std::size_t versionValuePresent = 0;
    std::size_t headSize20 = 0;
    std::size_t flagsZero = 0;
    std::size_t rresLayoutMatch = 0;
    std::size_t rresSizeFieldMatch = 0;
    std::size_t rresCountMatch = 0;
    std::size_t rresCountAEqualHead = 0;
    std::size_t rresCountBEqualHead = 0;
    std::size_t rresCountsEqual = 0;
    std::size_t rresField08MatchesHarm04 = 0;
    std::size_t rresField08MatchesHarm08 = 0;
    std::size_t rresField0CMatchesHarm04 = 0;
    std::size_t rresField0CMatchesHarm08 = 0;
    std::size_t rresFooterSecondZero = 0;
    std::size_t harmSizeFieldMatch = 0;
    std::size_t harmHeaderCorrelationEither = 0;
    std::map<std::uint32_t, std::size_t> harmField04Histogram;
    std::map<std::uint32_t, std::size_t> harmField08Histogram;
    std::map<std::uint32_t, std::size_t> harmField0CHistogram;
    std::map<std::int64_t, std::size_t> rres04MinusHarm04Histogram;

    std::size_t rres04EqualsHarm04Plus256 = 0;
    std::size_t rres08EqualsHarm08 = 0;
    std::size_t relationDiffZero = 0;
    std::size_t relationDiff256 = 0;
    std::size_t relationDiffOther = 0;
    std::map<std::uint32_t, std::pair<std::size_t, std::size_t>> relationByHarm0C;
    std::size_t harm08EqRres08WhenCountsEqual = 0;
    std::size_t harm08EqRres08WhenCountsDiffer = 0;

    std::size_t rresCountAZero = 0;
    std::size_t rresCountAPowerOfTwo = 0;
    std::size_t rresCountALessThan4096 = 0;
    std::size_t rresCountAEqual4096 = 0;
    std::uint32_t rresCountAMin = UINT32_MAX;
    std::uint32_t rresCountAMax = 0;
    std::uint64_t rresCountADifferenceSum = 0;

    std::map<std::uint32_t, std::size_t> versionHistogram;
    std::map<std::uint32_t, std::size_t> versionSizeHistogram;
    std::map<std::uint32_t, std::size_t> field08Histogram;
    std::map<std::uint32_t, std::size_t> countAValueHistogram;
    std::map<std::uint32_t, std::size_t> countDifferenceHistogram;
    std::map<std::uint32_t, std::size_t> rresUnknown0Histogram;
    std::map<std::uint32_t, std::size_t> rresFooterFirstHistogram;

    std::size_t countRatioZero = 0;
    std::size_t countRatioUpTo25 = 0;
    std::size_t countRatioUpTo50 = 0;
    std::size_t countRatioUpTo75 = 0;
    std::size_t countRatioUpTo90 = 0;
    std::size_t countRatioUpTo99 = 0;
    std::size_t countRatioBelow100 = 0;
    std::size_t rresCountALessEqualB = 0;

    std::size_t diffDiv441Exact = 0;
    std::size_t diffNear512 = 0;
    std::size_t diffNear1024 = 0;
    std::size_t diffNear1280 = 0;
    std::size_t diffNear1536 = 0;
    std::size_t diffNear2048 = 0;
    std::size_t diffNear2304 = 0;
    std::size_t diffNear2560 = 0;
    std::size_t diffNear3328 = 0;

    std::size_t residualTailAfterAAllZero = 0;
    std::size_t residualTailAfterAHasNonZero = 0;
    std::size_t residualNonZeroCountEqualA = 0;

    std::vector<std::size_t> lengthMismatches;
    std::vector<std::size_t> unusualVersions;
    std::vector<std::size_t> rresCountMismatches;

    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto& record = records[i];

        if (record.sections.size() == 3
            && record.sections[0].tag == "head"
            && record.sections[1].tag == "harm"
            && record.sections[2].tag == "rres"
            && record.hasHead) {
            ++structurallyValid;
        }

        if (record.lengthMatches) {
            ++lengthValid;
        } else {
            lengthMismatches.push_back(i);
        }

        if (record.version.tagPresent) {
            ++versionTagPresent;
            ++versionSizeHistogram[record.version.declaredSize];
        }

        if (record.version.hasValue) {
            ++versionValuePresent;
            ++versionHistogram[record.version.value];
            if (record.version.value != 20250410u) {
                unusualVersions.push_back(i);
            }
        }

        if (record.hasHead) {
            if (record.head.declaredSize == 20) ++headSize20;
            if (record.head.flags == 0) ++flagsZero;

            std::uint32_t bits = 0;
            static_assert(sizeof(bits) == sizeof(float), "unexpected uint32 size");
            const float value = record.head.field08F32;
            std::memcpy(&bits, &value, sizeof(bits));
            ++field08Histogram[bits];
        }

        const auto* rres = findSection(record, "rres");
        const auto* harmForStats = findSection(record, "harm");

        if (harmForStats && harmForStats->payloadSize >= 16) {
            const std::size_t harmBase =
                static_cast<std::size_t>(harmForStats->payloadOffset);
            const std::size_t harmPayload =
                static_cast<std::size_t>(harmForStats->payloadSize);

            const std::uint32_t harmSize =
                readU32LE(reader.bytes(), harmBase);
            const std::uint32_t harm04 =
                readU32LE(reader.bytes(), harmBase + 4);
            const std::uint32_t harm08 =
                readU32LE(reader.bytes(), harmBase + 8);
            const std::uint32_t harm0C =
                readU32LE(reader.bytes(), harmBase + 12);

            if (static_cast<std::uint64_t>(harmSize) + 4u ==
                static_cast<std::uint64_t>(harmPayload)) {
                ++harmSizeFieldMatch;
            }

            ++harmField04Histogram[harm04];
            ++harmField08Histogram[harm08];
            ++harmField0CHistogram[harm0C];
        }

        if (rres && record.hasHead) {
            const std::size_t payload =
                static_cast<std::size_t>(rres->payloadSize);
            const std::size_t n =
                static_cast<std::size_t>(record.head.sampleCount);

            const bool layout =
                payload == 16u + n * 2u + 8u;
            if (layout) ++rresLayoutMatch;

            if (payload >= 16) {
                const auto base =
                    static_cast<std::size_t>(rres->payloadOffset);
                const std::uint32_t storedSize =
                    readU32LE(reader.bytes(), base);
                const std::uint32_t storedUnknown0 =
                    readU32LE(reader.bytes(), base + 4);
                const std::uint32_t storedCountA =
                    readU32LE(reader.bytes(), base + 8);
                const std::uint32_t storedCountB =
                    readU32LE(reader.bytes(), base + 12);

                if (harmForStats && harmForStats->payloadSize >= 12) {
                    const std::size_t harmBase =
                        static_cast<std::size_t>(harmForStats->payloadOffset);
                    const std::uint32_t harm04 =
                        readU32LE(reader.bytes(), harmBase + 4);
                    const std::uint32_t harm08 =
                        readU32LE(reader.bytes(), harmBase + 8);

                    const std::int64_t difference =
                        static_cast<std::int64_t>(storedUnknown0)
                        - static_cast<std::int64_t>(harm04);

                    ++rres04MinusHarm04Histogram[difference];

                    if (difference == 0) {
                        ++relationDiffZero;
                    } else if (difference == 256) {
                        ++relationDiff256;
                    } else {
                        ++relationDiffOther;
                    }

                    auto& relationPair = relationByHarm0C[
                        readU32LE(reader.bytes(), harmBase + 12)
                    ];
                    if (difference == 0) {
                        ++relationPair.first;
                    } else if (difference == 256) {
                        ++relationPair.second;
                    }

                    if (difference == 256) {
                        ++rres04EqualsHarm04Plus256;
                    }
                    if (storedCountA == harm08) {
                        ++rres08EqualsHarm08;
                        if (storedCountA == storedCountB) {
                            ++harm08EqRres08WhenCountsEqual;
                        } else {
                            ++harm08EqRres08WhenCountsDiffer;
                        }
                    }
                }

                ++rresUnknown0Histogram[storedUnknown0];

                if (storedSize + 12u == payload) {
                    ++rresSizeFieldMatch;
                }

                const auto* harm = findSection(record, "harm");
                if (harm && harm->payloadSize >= 12) {
                    const std::size_t harmBase =
                        static_cast<std::size_t>(harm->payloadOffset);
                    const std::uint32_t harm04 =
                        readU32LE(reader.bytes(), harmBase + 4);
                    const std::uint32_t harm08 =
                        readU32LE(reader.bytes(), harmBase + 8);

                    if (storedCountA == harm04) {
                        ++rresField08MatchesHarm04;
                    }
                    if (storedCountA == harm08) {
                        ++rresField08MatchesHarm08;
                    }
                    if (storedCountB == harm04) {
                        ++rresField0CMatchesHarm04;
                    }
                    if (storedCountB == harm08) {
                        ++rresField0CMatchesHarm08;
                    }
                    if (storedCountA == harm08 || storedCountB == harm08) {
                        ++harmHeaderCorrelationEither;
                    }
                }

                rresCountAMin = std::min(rresCountAMin, storedCountA);
                rresCountAMax = std::max(rresCountAMax, storedCountA);
                if (storedCountA == 0) {
                    ++rresCountAZero;
                }
                if (storedCountA != 0
                    && (storedCountA & (storedCountA - 1u)) == 0) {
                    ++rresCountAPowerOfTwo;
                }
                if (storedCountA < 4096u) {
                    ++rresCountALessThan4096;
                } else if (storedCountA == 4096u) {
                    ++rresCountAEqual4096;
                }
                if (storedCountB >= storedCountA) {
                    rresCountADifferenceSum +=
                        static_cast<std::uint64_t>(storedCountB - storedCountA);
                }

                if (storedCountB >= storedCountA) {
                    ++rresCountALessEqualB;

                    const std::uint32_t diff =
                        storedCountB - storedCountA;

                    if (diff != 0 && diff % 441u == 0) {
                        ++diffDiv441Exact;
                    }

                    auto nearValue = [](std::uint32_t value,
                                        std::uint32_t target,
                                        std::uint32_t tolerance) {
                        const std::uint32_t delta =
                            value >= target ? value - target : target - value;
                        return delta <= tolerance;
                    };

                    if (diff != 0) {
                        if (nearValue(diff, 512u, 2u)) ++diffNear512;
                        if (nearValue(diff, 1024u, 2u)) ++diffNear1024;
                        if (nearValue(diff, 1280u, 2u)) ++diffNear1280;
                        if (nearValue(diff, 1536u, 2u)) ++diffNear1536;
                        if (nearValue(diff, 2048u, 2u)) ++diffNear2048;
                        if (nearValue(diff, 2304u, 2u)) ++diffNear2304;
                        if (nearValue(diff, 2560u, 3u)) ++diffNear2560;
                        if (nearValue(diff, 3328u, 2u)) ++diffNear3328;
                    }
                }

                ++countAValueHistogram[storedCountA];
                if (storedCountB >= storedCountA) {
                    ++countDifferenceHistogram[storedCountB - storedCountA];
                }

                if (storedCountB != 0) {
                    const double ratio =
                        static_cast<double>(storedCountA)
                        / static_cast<double>(storedCountB);

                    if (ratio == 0.0) {
                        ++countRatioZero;
                    } else if (ratio <= 0.25) {
                        ++countRatioUpTo25;
                    } else if (ratio <= 0.50) {
                        ++countRatioUpTo50;
                    } else if (ratio <= 0.75) {
                        ++countRatioUpTo75;
                    } else if (ratio <= 0.90) {
                        ++countRatioUpTo90;
                    } else if (ratio <= 0.99) {
                        ++countRatioUpTo99;
                    } else if (ratio < 1.0) {
                        ++countRatioBelow100;
                    }
                }

                if (layout && n > 0) {
                    const std::size_t residualBase = base + 16u;
                    std::size_t nonZeroCount = 0;
                    std::size_t tailNonZeroCount = 0;

                    for (std::size_t sample = 0; sample < n; ++sample) {
                        const std::uint16_t raw =
                            static_cast<std::uint16_t>(
                                reader.bytes()[residualBase + sample * 2u]
                            )
                            | static_cast<std::uint16_t>(
                                static_cast<std::uint16_t>(
                                    reader.bytes()[residualBase + sample * 2u + 1u]
                                ) << 8
                            );

                        const std::int16_t value =
                            static_cast<std::int16_t>(raw);

                        if (value != 0) {
                            ++nonZeroCount;
                        }

                        if (sample >= storedCountA && value != 0) {
                            ++tailNonZeroCount;
                        }
                    }

                    if (tailNonZeroCount == 0) {
                        ++residualTailAfterAAllZero;
                    } else {
                        ++residualTailAfterAHasNonZero;
                    }

                    if (nonZeroCount == storedCountA) {
                        ++residualNonZeroCountEqualA;
                    }
                }

                if (storedCountA == n) {
                    ++rresCountAEqualHead;
                }
                if (storedCountB == n) {
                    ++rresCountBEqualHead;
                }
                if (storedCountA == storedCountB) {
                    ++rresCountsEqual;
                }
                if (storedCountA == n && storedCountB == n) {
                    ++rresCountMatch;
                } else {
                    rresCountMismatches.push_back(i);
                }

                if (payload >= 8) {
                    const std::uint32_t footerSecond =
                        readU32LE(
                            reader.bytes(),
                            base + payload - 4
                        );
                    const std::uint32_t footerFirst =
                        readU32LE(
                            reader.bytes(),
                            base + payload - 8
                        );
                    ++rresFooterFirstHistogram[footerFirst];

                    if (footerSecond == 0) {
                        ++rresFooterSecondZero;
                    }
                }
            }
        }
    }

    auto printTopHistogram = [](const std::string& title,
                                const auto& histogram) {
        std::vector<std::pair<std::uint32_t, std::size_t>> items(
            histogram.begin(), histogram.end());

        std::sort(
            items.begin(),
            items.end(),
            [](const auto& a, const auto& b) {
                if (a.second != b.second) {
                    return a.second > b.second;
                }
                return a.first < b.first;
            }
        );

        const std::size_t limit = std::min<std::size_t>(32, items.size());
        std::cout << "\n" << title << " (top 32):\n";
        for (std::size_t i = 0; i < limit; ++i) {
            std::cout << "  " << items[i].first
                      << " : " << items[i].second << '\n';
        }
    };


    std::cout << "Structure valid    : "
              << structurallyValid << "/" << records.size() << '\n';
    std::cout << "Length field valid : "
              << lengthValid << "/" << records.size() << '\n';
    std::cout << "version tag present: "
              << versionTagPresent << "/" << records.size() << '\n';
    std::cout << "version value      : "
              << versionValuePresent << "/" << records.size() << '\n';
    std::cout << "head declared=20   : "
              << headSize20 << "/" << records.size() << '\n';
    std::cout << "head flags=0        : "
              << flagsZero << "/" << records.size() << '\n';
    std::cout << "rres 16+2N+8       : "
              << rresLayoutMatch << "/" << records.size() << '\n';
    std::cout << "rres sizeField+12  : "
              << rresSizeFieldMatch << "/" << records.size() << '\n';
    std::cout << "rres counts=N,N     : "
              << rresCountMatch << "/" << records.size() << '\n';
    std::cout << "rres countA=N       : "
              << rresCountAEqualHead << "/" << records.size() << '\n';
    std::cout << "rres countB=N       : "
              << rresCountBEqualHead << "/" << records.size() << '\n';
    std::cout << "rres field+08==+0C   : "
              << rresCountsEqual << "/" << records.size() << '\n';
    std::cout << "rres footer u32[1]=0: "
              << rresFooterSecondZero << "/" << records.size() << '\n';

    std::cout << "\nHARM header size field:\n";
    std::cout << "  HARM +0x00 == payload-4 : "
              << harmSizeFieldMatch << "/" << records.size() << '\n';

    printTopHistogram("HARM +0x04 values", harmField04Histogram);
    printTopHistogram("HARM +0x08 values", harmField08Histogram);
    printTopHistogram("HARM +0x0C values", harmField0CHistogram);
    printTopHistogram("RRES +0x04 minus HARM +0x04", rres04MinusHarm04Histogram);

    std::cout << "RRES +0x04 == HARM +0x04 + 256 : "
              << rres04EqualsHarm04Plus256 << "/" << records.size() << '\n';
    std::cout << "RRES +0x04/HARM +0x04 relation classes:\n";
    std::cout << "  difference == 0   : "
              << relationDiffZero << "/" << records.size() << '\n';
    std::cout << "  difference == 256 : "
              << relationDiff256 << "/" << records.size() << '\n';
    std::cout << "  other             : "
              << relationDiffOther << "/" << records.size() << '\n';

    std::cout << "\nRelation by HARM +0x0C (same,+256):\n";
    for (const auto& item : relationByHarm0C) {
        std::cout << "  harm0C=" << item.first
                  << " : same=" << item.second.first
                  << " +256=" << item.second.second << '\n';
    }

    std::cout << "\nRRES +0x08 == HARM +0x08 breakdown:\n";
    std::cout << "  when RRES +0x08 == +0x0C : "
              << harm08EqRres08WhenCountsEqual << '\n';
    std::cout << "  when RRES +0x08 != +0x0C : "
              << harm08EqRres08WhenCountsDiffer << '\n';

    std::cout << "RRES +0x08 == HARM +0x08 (direct check): "
              << rres08EqualsHarm08 << "/" << records.size() << '\n';

    std::cout << "HARM/RRES shared-field correlation (either RRES +0x08 or +0x0C): "
              << harmHeaderCorrelationEither << "/" << records.size() << '\n';

    std::cout << "\nRRES <-> HARM metadata correlation:\n";
    std::cout << "  RRES +0x08 == HARM +0x04 : "
              << rresField08MatchesHarm04 << "/" << records.size() << '\n';
    std::cout << "  RRES +0x08 == HARM +0x08 : "
              << rresField08MatchesHarm08 << "/" << records.size() << '\n';
    std::cout << "  RRES +0x0C == HARM +0x04 : "
              << rresField0CMatchesHarm04 << "/" << records.size() << '\n';
    std::cout << "  RRES +0x0C == HARM +0x08 : "
              << rresField0CMatchesHarm08 << "/" << records.size() << '\n';

    if (!records.empty()) {
        const double averageDifference =
            static_cast<double>(rresCountADifferenceSum)
            / static_cast<double>(records.size());

        std::cout << "\nRRES countA analysis:\n";
        std::cout << "  min countA              : " << rresCountAMin << '\n';
        std::cout << "  max countA              : " << rresCountAMax << '\n';
        std::cout << "  countA == 0             : "
                  << rresCountAZero << "/" << records.size() << '\n';
        std::cout << "  countA < 4096           : "
                  << rresCountALessThan4096 << "/" << records.size() << '\n';
        std::cout << "  countA == 4096          : "
                  << rresCountAEqual4096 << "/" << records.size() << '\n';
        std::cout << "  countA is power-of-two  : "
                  << rresCountAPowerOfTwo << "/" << records.size() << '\n';
        std::cout << std::setprecision(10);
        std::cout << "  average (countB-countA) : "
                  << averageDifference << '\n';
        std::cout << std::setprecision(6);
    }


    printTopHistogram("RRES countA values", countAValueHistogram);
    printTopHistogram("RRES countB-countA values", countDifferenceHistogram);
    printTopHistogram("RRES unknown0 values", rresUnknown0Histogram);
    printTopHistogram("RRES footer first u32 values", rresFooterFirstHistogram);

    std::cout << "\nRRES countA/countB ratio buckets:\n";
    std::cout << "  ratio == 0       : " << countRatioZero << '\n';
    std::cout << "  0 < ratio <= .25 : " << countRatioUpTo25 << '\n';
    std::cout << "  .25 < ratio <= .50: " << countRatioUpTo50 << '\n';
    std::cout << "  .50 < ratio <= .75: " << countRatioUpTo75 << '\n';
    std::cout << "  .75 < ratio <= .90: " << countRatioUpTo90 << '\n';
    std::cout << "  .90 < ratio <= .99: " << countRatioUpTo99 << '\n';
    std::cout << "  .99 < ratio < 1  : " << countRatioBelow100 << '\n';
    std::cout << "  ratio == 1       : " << rresCountsEqual << '\n';

    std::cout << "\nRRES count difference alignment:\n";
    std::cout << "  countA <= countB       : "
              << rresCountALessEqualB << "/" << records.size() << '\n';
    std::cout << "  diff divisible by 441  : "
              << diffDiv441Exact << "/" << records.size() << '\n';
    std::cout << "  diff ~= 512            : "
              << diffNear512 << "/" << records.size() << '\n';
    std::cout << "  diff ~= 1024           : "
              << diffNear1024 << "/" << records.size() << '\n';
    std::cout << "  diff ~= 1280           : "
              << diffNear1280 << "/" << records.size() << '\n';
    std::cout << "  diff ~= 1536           : "
              << diffNear1536 << "/" << records.size() << '\n';
    std::cout << "  diff ~= 2048           : "
              << diffNear2048 << "/" << records.size() << '\n';
    std::cout << "  diff ~= 2304           : "
              << diffNear2304 << "/" << records.size() << '\n';
    std::cout << "  diff ~= 2560           : "
              << diffNear2560 << "/" << records.size() << '\n';
    std::cout << "  diff ~= 3328           : "
              << diffNear3328 << "/" << records.size() << '\n';

    std::cout << "\nRRES residual/countA boundary analysis:\n";
    std::cout << "  residual after countA all zero : "
              << residualTailAfterAAllZero << "/" << records.size() << '\n';
    std::cout << "  residual after countA has data : "
              << residualTailAfterAHasNonZero << "/" << records.size() << '\n';
    std::cout << "  total nonzero residuals == countA : "
              << residualNonZeroCountEqualA << "/" << records.size() << '\n';

    std::cout << "\nVersion histogram:\n";
    for (const auto& item : versionHistogram) {
        std::cout << "  " << item.first
                  << " (0x" << std::hex << item.first << std::dec
                  << ") : " << item.second << '\n';
    }

    std::cout << "\nVersion payload-size histogram:\n";
    for (const auto& item : versionSizeHistogram) {
        std::cout << "  " << item.first << " : " << item.second << '\n';
    }

    std::cout << "\nfield+08 distinct values: "
              << field08Histogram.size() << '\n';
    std::size_t fieldPrint = 0;
    for (const auto& item : field08Histogram) {
        float value = 0.0f;
        std::memcpy(&value, &item.first, sizeof(value));
        std::cout << "  " << std::setprecision(10)
                  << value << " : " << item.second
                  << std::setprecision(6) << '\n';
        if (++fieldPrint >= 16) break;
    }

    if (!lengthMismatches.empty()) {
        std::cout << "\nLength mismatches (first 32):\n";
        const std::size_t n = std::min<std::size_t>(
            32, lengthMismatches.size());

        for (std::size_t j = 0; j < n; ++j) {
            const std::size_t i = lengthMismatches[j];
            const auto& record = records[i];

            std::cout << "  record[" << i << "]"
                      << " offset=0x" << std::hex << record.offset
                      << std::dec
                      << " actual=" << (record.endOffset - record.offset)
                      << " declared=" << record.declaredBodySize;

            if (record.version.tagPresent) {
                std::cout << " verSize=" << record.version.declaredSize;
            }

            if (record.version.hasValue) {
                std::cout << " ver=" << record.version.value;
            }

            std::cout << '\n';

            std::cout << "    prefix20: ";
            printHex(
                reader.bytes(),
                static_cast<std::size_t>(record.offset),
                20
            );
        }
    }

    if (!rresCountMismatches.empty()) {
        std::cout << "\nRRES count mismatches (first 32):\n";
        const std::size_t n = std::min<std::size_t>(
            32, rresCountMismatches.size());

        for (std::size_t j = 0; j < n; ++j) {
            const std::size_t i = rresCountMismatches[j];
            const auto& record = records[i];
            const auto* rres = findSection(record, "rres");
            const auto* harm = findSection(record, "harm");

            std::cout << "  record[" << i << "]"
                      << " offset=0x" << std::hex << record.offset
                      << std::dec;

            if (record.hasHead) {
                std::cout << " headN=" << record.head.sampleCount;
            }

            if (harm) {
                std::cout << " harmFirst32=";
                printHex(
                    reader.bytes(),
                    static_cast<std::size_t>(harm->payloadOffset),
                    64
                );
            }

            if (rres && rres->payloadSize >= 16) {
                const std::size_t base =
                    static_cast<std::size_t>(rres->payloadOffset);
                const std::uint32_t countA =
                    readU32LE(reader.bytes(), base + 8);
                const std::uint32_t countB =
                    readU32LE(reader.bytes(), base + 12);

                const std::uint32_t diff =
                    countB >= countA ? countB - countA : 0u;

                std::cout << " field08=" << countA
                          << " field0C=" << countB
                          << " diff=" << diff
                          << " diffMs=" << std::setprecision(8)
                          << (static_cast<double>(diff) / 44.1)
                          << std::setprecision(6)
                          << " rresPayload=" << rres->payloadSize;

                if (rres->payloadSize == 16u
                    + static_cast<std::size_t>(record.head.sampleCount) * 2u
                    + 8u
                    && countB >= countA) {
                    const std::size_t residualBase = base + 16u;
                    std::size_t tailNonZero = 0;
                    std::int32_t tailMaxAbs = 0;
                    std::size_t totalNonZero = 0;
                    std::size_t firstNonZero = static_cast<std::size_t>(countB);
                    std::size_t lastNonZero = 0;

                    for (std::size_t sample = 0;
                         sample < static_cast<std::size_t>(countB);
                         ++sample) {
                        const std::uint16_t raw =
                            static_cast<std::uint16_t>(
                                reader.bytes()[residualBase + sample * 2u]
                            )
                            | static_cast<std::uint16_t>(
                                static_cast<std::uint16_t>(
                                    reader.bytes()[residualBase + sample * 2u + 1u]
                                ) << 8
                            );
                        const std::int16_t value =
                            static_cast<std::int16_t>(raw);

                        if (value != 0) {
                            ++totalNonZero;
                            firstNonZero = std::min(firstNonZero, sample);
                            lastNonZero = std::max(lastNonZero, sample);
                        }

                        if (sample >= countA && value != 0) {
                            ++tailNonZero;
                            const std::int32_t absValue =
                                value == INT16_MIN
                                    ? 32768
                                    : (value < 0 ? -value : value);
                            tailMaxAbs = std::max(tailMaxAbs, absValue);
                        }
                    }

                    std::cout << " tailNonZero=" << tailNonZero
                              << " tailMaxAbs=" << tailMaxAbs
                              << " totalNonZero=" << totalNonZero;

                    if (totalNonZero != 0) {
                        std::cout << " firstNZ=" << firstNonZero
                                  << " lastNZ=" << lastNonZero
                                  << " lastNZ-countA=";
                        if (lastNonZero >= countA) {
                            std::cout << (lastNonZero - countA);
                        } else {
                            std::cout << "-" << (countA - lastNonZero);
                        }
                        std::cout << " trailingZero="
                                  << (static_cast<std::size_t>(countB) - 1u - lastNonZero);
                    }
                }

                if (harm) {
                    std::cout << " harmPayload=" << harm->payloadSize;
                }
            }

            std::cout << '\n';
        }
    }

    if (!unusualVersions.empty()) {
        std::cout << "\nNon-20250410 version records (first 32):\n";
        const std::size_t n = std::min<std::size_t>(
            32, unusualVersions.size());

        for (std::size_t j = 0; j < n; ++j) {
            const std::size_t i = unusualVersions[j];
            const auto& record = records[i];

            std::cout << "  record[" << i << "]"
                      << " offset=0x" << std::hex << record.offset
                      << std::dec
                      << " ver=" << record.version.value
                      << " (0x" << std::hex << record.version.value
                      << std::dec << ")\n";
        }
    }

    const std::size_t count = records.size() < 8 ? records.size() : 8;

    for (std::size_t i = 0; i < count; ++i) {
        const auto& record = records[i];

        std::cout << "\nrecord[" << i << "]\n";
        std::cout << "  offset : 0x" << std::hex
                  << record.offset << std::dec << '\n';
        std::cout << "  end    : 0x" << std::hex
                  << record.endOffset << std::dec << '\n';
        if (record.isLastRecord) {
            std::cout << "  trailing: " << record.trailingBytesAfterRecord << " bytes after declared record\n";
        }
        std::cout << "  size   : "
                  << (record.endOffset - record.offset)
                  << " bytes\n";
        std::cout << "  bodySz : "
                  << record.declaredBodySize
                  << " (size-16="
                  << ((record.endOffset - record.offset) >= 16
                      ? (record.endOffset - record.offset) - 16
                      : 0)
                  << ")"
                  << (record.lengthMatches ? " [match]" : " [MISMATCH]")
                  << '\n';

        if (record.version.tagPresent) {
            std::cout << "  ver    : ";
            if (record.version.hasValue) {
                std::cout << record.version.value
                          << " (0x" << std::hex
                          << record.version.value << std::dec << ")";
            } else {
                std::cout << "payload size "
                          << record.version.declaredSize
                          << " bytes";
            }
            std::cout << '\n';
        }

        for (const auto& section : record.sections) {
            std::cout << "  " << section.tag
                      << " @ 0x" << std::hex << section.offset
                      << " payload=0x" << section.payloadOffset
                      << std::dec
                      << " size=" << section.payloadSize
                      << " bytes\n";

            if (section.tag == "head" && record.hasHead) {
                printHead(record.head);
            }

            if (section.tag == "rres") {
                const std::size_t payload =
                    static_cast<std::size_t>(section.payloadSize);

                std::cout << "    rres first16 : ";
                printHex(
                    reader.bytes(),
                    static_cast<std::size_t>(section.payloadOffset),
                    16
                );

                if (payload >= 16) {
                    std::cout << "    rres tail16  : ";
                    printHex(
                        reader.bytes(),
                        static_cast<std::size_t>(
                            section.payloadOffset + payload - 16
                        ),
                        16
                    );
                }

                if (record.hasHead) {
                    const std::size_t n = record.head.sampleCount;
                    const std::size_t candidate = 16u + n * 2u + 8u;

                    std::cout << "    payload                   : "
                              << payload << " bytes\n";
                    std::cout << "    16 + sampleCount*2 + 8   : "
                              << candidate << " bytes\n";
                    std::cout << "    difference                : "
                              << static_cast<long long>(payload)
                                 - static_cast<long long>(candidate)
                              << " bytes\n";

                    if (payload >= 16) {
                        const std::size_t base =
                            static_cast<std::size_t>(section.payloadOffset);
                        const std::uint32_t storedSize =
                            readU32LE(reader.bytes(), base);

                        std::cout << "    rres storedSize        : "
                                  << storedSize << '\n';
                        std::cout << "    storedSize + 12       : "
                                  << (storedSize + 12u) << '\n';

                        std::cout << "    residual byte start   : +0x10\n";
                        std::cout << "    residual byte end     : +0x"
                                  << std::hex
                                  << (16u + n * 2u)
                                  << std::dec << '\n';
                        std::cout << "    footer start          : +0x"
                                  << std::hex
                                  << (16u + n * 2u)
                                  << std::dec << '\n';
                    }
                }
            }
        }

        std::cout << "  prefix20: ";
        printHex(
            reader.bytes(),
            static_cast<std::size_t>(record.offset),
            20
        );
    }

    if (!warning.empty()) {
        std::cout << "\nwarning: " << warning << '\n';
    }

    return 0;
}
