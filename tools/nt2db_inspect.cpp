#include "nt2core/ntdb_reader.h"
#include "nt2core/m9p_scanner.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cmath>
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
    struct Harm0CStats {
        std::size_t count = 0;
        double frequencySum = 0.0;
        double productSum = 0.0;
        double productMin = 0.0;
        double productMax = 0.0;
        bool initialized = false;
    };
    std::map<std::uint32_t, Harm0CStats> harm0CFrequencyStats;
    std::map<std::uint32_t, std::pair<std::size_t, std::size_t>> relationByHarm0C;
    std::size_t harm0CMatchesFloor16000 = 0;
    std::size_t harmDataDivisibleByHarm0C = 0;
    std::size_t harmDataDivisibleByHarm0Cx4 = 0;
    std::size_t harmDataDivisibleByHarm0Cx8 = 0;
    std::size_t harmDataDivisibleByHarm0Cx12 = 0;
    std::map<std::uint32_t, std::size_t> harmPerHarmonicHistogram;
    std::map<std::uint32_t, std::size_t> harmRemainderByHarm0C;
    std::map<std::uint32_t, std::size_t> harmFirstWordHistogram;
    std::map<std::uint32_t, std::size_t> harmSecondWordHistogram;
    std::map<std::uint32_t, std::size_t> harmThirdWordHistogram;
    std::size_t harmChainFirstPlusThreeValid = 0;
    std::size_t harmChainAtLeastTwoBlocks = 0;
    std::size_t harmChainCompleteToFooter = 0;
    std::size_t harmFirstCountMatchesCeilSampleOver16 = 0;
    std::size_t harmSecondCountMatchesHarm0C = 0;
    std::size_t harmSecondCountMatchesHarm0CPlusOrMinus1 = 0;
    std::map<std::uint32_t, std::size_t> harmSecondCountHistogram;
    std::map<std::uint32_t, std::map<std::uint32_t, std::size_t>> harmSecondCountByHarm0C;
    std::map<std::uint32_t, std::map<std::uint32_t, std::size_t>> harmSecondCountByHarm04;
    std::size_t harmSecondCountVsHarm0CWithin5 = 0;
    std::size_t harmSecondCountVsHarm0CWithin10 = 0;
    std::size_t harmPostSecondAllFiniteFloats = 0;
    std::size_t harmPositiveLenMatchesFirstCountPlus1 = 0;
    std::size_t harmPositiveLenMatchesFirstCountPlus2 = 0;
    std::size_t harmPositiveLenMatchesFirstCountPlus4 = 0;
    std::size_t harmPositiveLenMatchesCeilSamplePlus1 = 0;
    std::size_t harmPositiveLenMatchesCeilSamplePlus4 = 0;
    std::size_t harmPositiveRegionCountMatchesFirstCount = 0;
    std::size_t harmPositiveRegionAllFinitePositive = 0;
    std::size_t harmPositiveTail3AllFinitePositive = 0;
    std::size_t harmPostSecondTrailingExactlyOneZero = 0;
    std::size_t harmPostSecondEmbeddedCountMatchesFirstCount = 0;
    std::size_t harmPostSecondNegativeRegionAllFiniteNegative = 0;
    std::size_t harmPostSecondExactRepeatedF0Block = 0;
    std::map<std::int64_t, std::size_t> harmPositiveLenMinusFirstCount;
    std::map<std::uint32_t, std::size_t> harmNegativeLenHistogram;
    std::size_t harmPostSecondCandidateHeaderMatches = 0;
    std::size_t harm10CeilSampleOver16 = 0;
    std::size_t harm10FloorSampleOver16 = 0;
    std::size_t harm10CeilRres08Over16 = 0;
    std::size_t harm10CeilHarm08Over16 = 0;
    std::size_t harm10FloorHarm08Over16 = 0;
    std::size_t harm10EqualsZero = 0;
    std::size_t harm10Positive = 0;
    std::map<std::uint32_t, std::pair<std::size_t, std::size_t>> harm10VsHarm08Quartiles;
    std::map<std::uint32_t, std::size_t> harm10Harm08DeltaHistogram;
    std::size_t harm10EqualsSamplePlusOffset = 0;
    std::size_t harm0CFormulaMismatches = 0;
    double harmCutoffLowerBound = 0.0;
    double harmCutoffUpperBound = 0.0;
    bool harmCutoffBoundsInitialized = false;
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

            if (harm0C != 0 && harmPayload >= 16) {
                if (harmPayload >= 28) {
                    const std::uint32_t harm10 =
                        readU32LE(reader.bytes(), harmBase + 16);
                    const std::uint32_t harm14 =
                        readU32LE(reader.bytes(), harmBase + 20);
                    const std::uint32_t harm18 =
                        readU32LE(reader.bytes(), harmBase + 24);

                    if (record.hasHead) {
                        const std::uint32_t sampleCount =
                            record.head.sampleCount;
                        const std::uint32_t ceil16 =
                            (sampleCount + 15u) / 16u;
                        const std::uint32_t floor16 =
                            sampleCount / 16u;
                        if (harm10 == ceil16) ++harm10CeilSampleOver16;
                        if (harm10 == floor16) ++harm10FloorSampleOver16;
                    }

                    if (rres && rres->payloadSize >= 12) {
                        const std::uint32_t rres08 =
                            readU32LE(
                                reader.bytes(),
                                static_cast<std::size_t>(rres->payloadOffset) + 8u
                            );
                        const std::uint32_t ceilRres16 =
                            (rres08 + 15u) / 16u;
                        if (harm10 == ceilRres16) {
                            ++harm10CeilRres08Over16;
                        }
                    }

                    const std::uint32_t ceilHarm08 =
                        (harm08 + 15u) / 16u;
                    const std::uint32_t floorHarm08 =
                        harm08 / 16u;
                    if (harm10 == ceilHarm08) {
                        ++harm10CeilHarm08Over16;
                    }
                    if (harm10 == floorHarm08) {
                        ++harm10FloorHarm08Over16;
                    }

                    if (harm10 == 0) {
                        ++harm10EqualsZero;
                    } else {
                        ++harm10Positive;
                    }

                    const std::uint32_t harm08Delta =
                        harm08 >= harm10 * 16u
                            ? harm08 - harm10 * 16u
                            : harm10 * 16u - harm08;
                    ++harm10Harm08DeltaHistogram[harm08Delta];

                    const std::uint32_t quartile =
                        harm08 / 4096u;
                    auto& quartilePair = harm10VsHarm08Quartiles[quartile];
                    if (harm10 == ceilHarm08) {
                        ++quartilePair.first;
                    } else {
                        ++quartilePair.second;
                    }

                    ++harmFirstWordHistogram[harm10];
                    ++harmSecondWordHistogram[harm14];
                    ++harmThirdWordHistogram[harm18];
                }

                const std::size_t harmDataSize = harmPayload - 16u;

                if (harmDataSize % harm0C == 0) {
                    ++harmDataDivisibleByHarm0C;
                    ++harmPerHarmonicHistogram[
                        static_cast<std::uint32_t>(harmDataSize / harm0C)
                    ];
                } else {
                    ++harmRemainderByHarm0C[
                        static_cast<std::uint32_t>(harmDataSize % harm0C)
                    ];
                }

                if (harmDataSize % (static_cast<std::size_t>(harm0C) * 4u) == 0) {
                    ++harmDataDivisibleByHarm0Cx4;
                }
                if (harmDataSize % (static_cast<std::size_t>(harm0C) * 8u) == 0) {
                    ++harmDataDivisibleByHarm0Cx8;
                }
                if (harmDataSize % (static_cast<std::size_t>(harm0C) * 12u) == 0) {
                    ++harmDataDivisibleByHarm0Cx12;
                }
            }

            if (record.hasHead && record.head.referenceFrequency > 0.0) {
                auto& stats = harm0CFrequencyStats[harm0C];
                const double frequency = record.head.referenceFrequency;
                const double product =
                    static_cast<double>(harm0C) * frequency;
                const double formulaValue =
                    std::floor(16000.0 / frequency);
                const std::uint32_t expectedHarmonics =
                    formulaValue >= 0.0
                        ? static_cast<std::uint32_t>(formulaValue)
                        : 0u;

                ++stats.count;
                stats.frequencySum += frequency;
                stats.productSum += product;
                if (!stats.initialized) {
                    stats.productMin = product;
                    stats.productMax = product;
                    stats.initialized = true;
                } else {
                    stats.productMin = std::min(stats.productMin, product);
                    stats.productMax = std::max(stats.productMax, product);
                }

                if (harm0C == expectedHarmonics) {
                    ++harm0CMatchesFloor16000;

                    const double lower = product;
                    const double upper =
                        static_cast<double>(harm0C + 1u) * frequency;

                    if (!harmCutoffBoundsInitialized) {
                        harmCutoffLowerBound = lower;
                        harmCutoffUpperBound = upper;
                        harmCutoffBoundsInitialized = true;
                    } else {
                        harmCutoffLowerBound =
                            std::max(harmCutoffLowerBound, lower);
                        harmCutoffUpperBound =
                            std::min(harmCutoffUpperBound, upper);
                    }
                } else {
                    ++harm0CFormulaMismatches;
                }
            }

            if (harmPayload >= 20) {
                std::size_t cursor = 16u;
                std::size_t blocks = 0;
                bool firstPlusThreeValid = false;
                bool complete = false;

                while (cursor + 4u <= harmPayload && blocks < 64u) {
                    const std::uint32_t count =
                        readU32LE(reader.bytes(), harmBase + cursor);
                    const std::size_t dataBytes =
                        static_cast<std::size_t>(count) * 4u;

                    if (dataBytes > harmPayload - cursor - 4u) {
                        break;
                    }

                    const std::size_t afterData = cursor + 4u + dataBytes;
                    if (afterData + 12u > harmPayload) {
                        break;
                    }

                    cursor = afterData + 12u;
                    ++blocks;

                    if (blocks == 1u) {
                        firstPlusThreeValid = true;
                    }

                    if (cursor == harmPayload) {
                        complete = true;
                        break;
                    }
                }

                if (firstPlusThreeValid) {
                    ++harmChainFirstPlusThreeValid;
                }
                if (blocks >= 2u) {
                    ++harmChainAtLeastTwoBlocks;
                }
                if (complete) {
                    ++harmChainCompleteToFooter;
                }

                if (blocks >= 1u && record.hasHead) {
                    const std::uint32_t firstCount =
                        readU32LE(reader.bytes(), harmBase + 16u);
                    const std::uint32_t expected =
                        (record.head.sampleCount + 15u) / 16u;
                    if (firstCount == expected) {
                        ++harmFirstCountMatchesCeilSampleOver16;
                    }
                }

                if (blocks >= 2u) {
                    const std::size_t firstCursor = 16u;
                    const std::uint32_t firstCount =
                        readU32LE(reader.bytes(), harmBase + firstCursor);
                    const std::size_t secondHeader =
                        firstCursor + 4u
                        + static_cast<std::size_t>(firstCount) * 4u
                        + 12u;

                    if (secondHeader + 4u <= harmPayload) {
                        const std::uint32_t secondCount =
                            readU32LE(reader.bytes(), harmBase + secondHeader);
                        ++harmSecondCountHistogram[secondCount];
                        ++harmSecondCountByHarm0C[harm0C][secondCount];
                        ++harmSecondCountByHarm04[harm04][secondCount];

                        const std::uint32_t secondDelta =
                            secondCount >= harm0C
                                ? secondCount - harm0C
                                : harm0C - secondCount;
                        if (secondDelta <= 5u) {
                            ++harmSecondCountVsHarm0CWithin5;
                        }
                        if (secondDelta <= 10u) {
                            ++harmSecondCountVsHarm0CWithin10;
                        }

                        if (secondCount == harm0C) {
                            ++harmSecondCountMatchesHarm0C;
                        }

                        const std::uint32_t delta =
                            secondCount >= harm0C
                                ? secondCount - harm0C
                                : harm0C - secondCount;
                        if (delta <= 1u) {
                            ++harmSecondCountMatchesHarm0CPlusOrMinus1;
                        }

                        const std::size_t secondBlockEnd =
                            secondHeader
                            + 4u
                            + static_cast<std::size_t>(secondCount) * 4u
                            + 12u;

                        if (secondBlockEnd <= harmPayload
                            && record.hasHead) {
                            const std::size_t remaining =
                                harmPayload - secondBlockEnd;
                            if (remaining % 4u == 0u && remaining > 0u) {
                                const std::size_t wordCount = remaining / 4u;
                                ++harmPostSecondAllFiniteFloats;

                                std::size_t trailingZeros = 0u;
                                while (trailingZeros < wordCount) {
                                    const std::size_t pos =
                                        wordCount - 1u - trailingZeros;
                                    const std::uint32_t word =
                                        readU32LE(reader.bytes(), harmBase + secondBlockEnd + pos * 4u);
                                    if (word == 0u) {
                                        ++trailingZeros;
                                    } else {
                                        break;
                                    }
                                }
                                if (trailingZeros == 1u) {
                                    ++harmPostSecondTrailingExactlyOneZero;
                                }

                                const std::size_t dataEnd = wordCount - trailingZeros;
                                std::size_t firstPositive = dataEnd;
                                for (std::size_t pos = 0u; pos < dataEnd; ++pos) {
                                    const std::uint32_t word =
                                        readU32LE(reader.bytes(), harmBase + secondBlockEnd + pos * 4u);
                                    float value = 0.0f;
                                    std::memcpy(&value, &word, sizeof(value));
                                    if (value > 0.0f) {
                                        firstPositive = pos;
                                        break;
                                    }
                                }

                                const std::size_t negativeLen =
                                    firstPositive == dataEnd ? dataEnd : firstPositive;
                                const std::size_t positiveLen =
                                    dataEnd >= negativeLen ? dataEnd - negativeLen : 0u;

                                ++harmPositiveLenMinusFirstCount[
                                    static_cast<std::int64_t>(positiveLen)
                                    - static_cast<std::int64_t>(firstCount)
                                ];
                                ++harmNegativeLenHistogram[
                                    static_cast<std::uint32_t>(negativeLen)
                                ];

                                if (positiveLen == static_cast<std::size_t>(firstCount) + 4u) {
                                    ++harmPositiveLenMatchesFirstCountPlus4;
                                }

                                const std::size_t ceilSample =
                                    (static_cast<std::size_t>(record.head.sampleCount) + 15u) / 16u;
                                if (positiveLen == ceilSample + 4u) {
                                    ++harmPositiveLenMatchesCeilSamplePlus4;
                                }

                                bool negativeAllFiniteNegative = true;
                                for (std::size_t pos = 0u; pos < negativeLen; ++pos) {
                                    const std::uint32_t word =
                                        readU32LE(reader.bytes(), harmBase + secondBlockEnd + pos * 4u);
                                    float value = 0.0f;
                                    std::memcpy(&value, &word, sizeof(value));
                                    if (!std::isfinite(value) || value >= 0.0f) {
                                        negativeAllFiniteNegative = false;
                                        break;
                                    }
                                }
                                if (negativeAllFiniteNegative) {
                                    ++harmPostSecondNegativeRegionAllFiniteNegative;
                                }

                                if (firstPositive < dataEnd) {
                                    const std::uint32_t embeddedCount =
                                        readU32LE(reader.bytes(), harmBase + secondBlockEnd + firstPositive * 4u);

                                    if (embeddedCount == firstCount) {
                                        ++harmPostSecondEmbeddedCountMatchesFirstCount;
                                    }

                                    const std::size_t floatStart = firstPositive + 1u;
                                    const std::size_t floatCount =
                                        dataEnd > floatStart ? dataEnd - floatStart : 0u;

                                    if (positiveLen == static_cast<std::size_t>(embeddedCount) + 4u) {
                                        ++harmPositiveRegionCountMatchesFirstCount;
                                    }

                                    bool allFinitePositive = true;
                                    for (std::size_t j = 0u; j < floatCount; ++j) {
                                        const std::size_t pos = floatStart + j;
                                        const std::uint32_t word =
                                            readU32LE(reader.bytes(), harmBase + secondBlockEnd + pos * 4u);
                                        float value = 0.0f;
                                        std::memcpy(&value, &word, sizeof(value));
                                        if (!std::isfinite(value) || value <= 0.0f) {
                                            allFinitePositive = false;
                                            break;
                                        }
                                    }

                                    if (floatCount == static_cast<std::size_t>(firstCount) + 3u
                                        && allFinitePositive) {
                                        ++harmPositiveRegionAllFinitePositive;
                                    }

                                    bool tail3FinitePositive = floatCount >= 3u;
                                    if (tail3FinitePositive) {
                                        for (std::size_t j = 0u; j < 3u; ++j) {
                                            const std::size_t pos = dataEnd - 3u + j;
                                            const std::uint32_t word =
                                                readU32LE(reader.bytes(), harmBase + secondBlockEnd + pos * 4u);
                                            float value = 0.0f;
                                            std::memcpy(&value, &word, sizeof(value));
                                            if (!std::isfinite(value) || value <= 0.0f) {
                                                tail3FinitePositive = false;
                                                break;
                                            }
                                        }
                                    }
                                    if (tail3FinitePositive) {
                                        ++harmPositiveTail3AllFinitePositive;
                                    }

                                    if (trailingZeros == 1u
                                        && embeddedCount == firstCount
                                        && floatCount == static_cast<std::size_t>(firstCount) + 3u
                                        && allFinitePositive
                                        && tail3FinitePositive) {
                                        ++harmPostSecondExactRepeatedF0Block;
                                    }
                                }
                            }
                        }
                    }
                }
            }
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

    std::cout << "\nHARM +0x0C vs reference-frequency product:\n";
    for (const auto& item : harm0CFrequencyStats) {
        const auto& stats = item.second;
        if (!stats.initialized || stats.count == 0) continue;
        std::cout << "  harm0C=" << item.first
                  << " count=" << stats.count
                  << " avgFreq=" << (stats.frequencySum / static_cast<double>(stats.count))
                  << " avgProduct=" << (stats.productSum / static_cast<double>(stats.count))
                  << " productMin=" << stats.productMin
                  << " productMax=" << stats.productMax
                  << '\n';
    }

    std::cout << "\nHARM +0x0C == floor(16000 / referenceFrequency): "
              << harm0CMatchesFloor16000 << "/" << records.size() << '\n';
    std::cout << "HARM cutoff bounds implied by all matching records: ";
    if (harmCutoffBoundsInitialized) {
        std::cout << harmCutoffLowerBound
                  << " <= cutoff < " << harmCutoffUpperBound << '\n';
    } else {
        std::cout << "not available\n";
    }
    std::cout << "HARM formula mismatches: "
              << harm0CFormulaMismatches << "/" << records.size() << '\n';

    std::cout << "\nHARM payload decomposition by +0x0C:\n";
    std::cout << "  payload-16 divisible by harm0C    : "
              << harmDataDivisibleByHarm0C << "/" << records.size() << '\n';
    std::cout << "  divisible by harm0C*4             : "
              << harmDataDivisibleByHarm0Cx4 << "/" << records.size() << '\n';
    std::cout << "  divisible by harm0C*8             : "
              << harmDataDivisibleByHarm0Cx8 << "/" << records.size() << '\n';
    std::cout << "  divisible by harm0C*12            : "
              << harmDataDivisibleByHarm0Cx12 << "/" << records.size() << '\n';
    printTopHistogram("HARM (payload-16)/harm0C when exact", harmPerHarmonicHistogram);
    printTopHistogram("HARM (payload-16) remainder by harm0C", harmRemainderByHarm0C);
    printTopHistogram("HARM +0x10 first u32", harmFirstWordHistogram);
    printTopHistogram("HARM +0x14 second u32", harmSecondWordHistogram);
    printTopHistogram("HARM +0x18 third u32", harmThirdWordHistogram);

    std::cout << "\nHARM +0x10 candidate count checks:\n";
    std::cout << "  +0x10 == ceil(sampleCount/16) : "
              << harm10CeilSampleOver16 << "/" << records.size() << '\n';
    std::cout << "  +0x10 == floor(sampleCount/16) : "
              << harm10FloorSampleOver16 << "/" << records.size() << '\n';
    std::cout << "  +0x10 == ceil(RRES+0x08/16)    : "
              << harm10CeilRres08Over16 << "/" << records.size() << '\n';
    std::cout << "  +0x10 == ceil(HARM+0x08/16)       : "
              << harm10CeilHarm08Over16 << "/" << records.size() << '\n';
    std::cout << "  +0x10 == floor(HARM+0x08/16)      : "
              << harm10FloorHarm08Over16 << "/" << records.size() << '\n';
    std::cout << "  +0x10 == 0                        : "
              << harm10EqualsZero << "/" << records.size() << '\n';
    std::cout << "  +0x10 > 0                         : "
              << harm10Positive << "/" << records.size() << '\n';
    printTopHistogram("HARM +0x08 - (+0x10*16) absolute delta", harm10Harm08DeltaHistogram);
    std::cout << "\n+0x10 vs HARM +0x08 by 4096-wide buckets:\n";
    for (const auto& item : harm10VsHarm08Quartiles) {
        std::cout << "  harm08Bucket=" << item.first
                  << " : ceilMatch=" << item.second.first
                  << " nonMatch=" << item.second.second << '\n';
    }

    std::cout << "  post-second regions inspected: "
              << harmPostSecondAllFiniteFloats << "/" << records.size() << "\n";
    std::cout << "  positiveLen == firstCount+1 : "
              << harmPositiveLenMatchesFirstCountPlus1 << "/" << records.size() << '\n';
    std::cout << "  positiveLen == firstCount+2 : "
              << harmPositiveLenMatchesFirstCountPlus2 << "/" << records.size() << '\n';
    std::cout << "  positiveLen == firstCount+4 : "
              << harmPositiveLenMatchesFirstCountPlus4 << "/" << records.size() << '\n';
    std::cout << "  positiveLen == ceil(sampleCount/16)+1 : "
              << harmPositiveLenMatchesCeilSamplePlus1 << "/" << records.size() << '\n';
    std::cout << "  positiveLen == ceil(sampleCount/16)+4 : "
              << harmPositiveLenMatchesCeilSamplePlus4 << "/" << records.size() << '\n';
    std::cout << "  post-second trailing zero count == 1 : "
              << harmPostSecondTrailingExactlyOneZero << "/" << records.size() << '\n';
    std::cout << "  embedded count == firstCount : "
              << harmPostSecondEmbeddedCountMatchesFirstCount << "/" << records.size() << '\n';
    std::cout << "  positiveLen == embeddedCount+4 : "
              << harmPositiveRegionCountMatchesFirstCount << "/" << records.size() << '\n';
    std::cout << "  negative region all finite < 0 : "
              << harmPostSecondNegativeRegionAllFiniteNegative << "/" << records.size() << '\n';
    std::cout << "  positive float region count==firstCount+3 and all >0 : "
              << harmPositiveRegionAllFinitePositive << "/" << records.size() << '\n';
    std::cout << "  final 3 positive floats all finite >0 : "
              << harmPositiveTail3AllFinitePositive << "/" << records.size() << '\n';
    std::cout << "  exact [neg floats][count][float[count]][3 floats][0] : "
              << harmPostSecondExactRepeatedF0Block << "/" << records.size() << '\n';
    printTopHistogram(
        "HARM positiveLen-firstCount",
        harmPositiveLenMinusFirstCount
    );
    printTopHistogram(
        "HARM negative-region length values",
        harmNegativeLenHistogram
    );
    std::cout << "  post-second candidate small-int+3-float occurrences: "
              << harmPostSecondCandidateHeaderMatches << "\n";

    std::cout << "\nHARM first-block / second-block diagnostics:\n";
    std::cout << "  first count == ceil(sampleCount/16) : "
              << harmFirstCountMatchesCeilSampleOver16
              << "/" << records.size() << '\n';
    std::cout << "  second count == HARM +0x0C         : "
              << harmSecondCountMatchesHarm0C
              << "/" << records.size() << '\n';
    std::cout << "  second count within +/-1 of HARM0C : "
              << harmSecondCountMatchesHarm0CPlusOrMinus1
              << "/" << records.size() << '\n';

    printTopHistogram("HARM second block count values", harmSecondCountHistogram);
    std::cout << "  second count within +/-5 of HARM +0x0C : "
              << harmSecondCountVsHarm0CWithin5
              << "/" << records.size() << '\n';
    std::cout << "  second count within +/-10 of HARM +0x0C: "
              << harmSecondCountVsHarm0CWithin10
              << "/" << records.size() << '\n';

    std::cout << "  second-count distributions by common harmonic counts:\n";
    for (const std::uint32_t harmonicCount : {30u, 40u, 48u, 61u, 81u}) {
        const auto it = harmSecondCountByHarm0C.find(harmonicCount);
        if (it == harmSecondCountByHarm0C.end()) continue;
        std::cout << "    harm0C=" << harmonicCount << ":";
        std::size_t printed = 0;
        for (const auto& entry : it->second) {
            std::cout << " " << entry.first << "=" << entry.second;
            if (++printed >= 16) break;
        }
        std::cout << '\n';
    }

    std::cout << "\nHARM post-second-block float-region diagnostics:\n";
    std::cout << "  Representative second-block followers are inspected below.\n";
    std::cout << "  Candidate headers require a small integer (<=4096) followed by 3 finite floats.\n";

    std::cout << "\nHARM repeated [count][float[count]][3 floats] chain check:\n";
    std::cout << "  first block structurally valid : "
              << harmChainFirstPlusThreeValid << "/" << records.size() << '\n';
    std::cout << "  at least 2 blocks             : "
              << harmChainAtLeastTwoBlocks << "/" << records.size() << '\n';
    std::cout << "  chain reaches payload end     : "
              << harmChainCompleteToFooter << "/" << records.size() << '\n';

    const std::size_t diagnosticRecords[] = {0, 1, 2, 3, 4, 645, 656, 668, 675, 755, 756};
    std::cout << "\nSelected HARM +0x10..+0x2F words:\n";
    for (const std::size_t index : diagnosticRecords) {
        if (index >= records.size()) continue;
        const auto& record = records[index];
        const auto* harm = findSection(record, "harm");
        if (!harm || harm->payloadSize < 16) continue;

        const std::size_t base =
            static_cast<std::size_t>(harm->payloadOffset);

        std::cout << "  record[" << index << "] payload="
                  << harm->payloadSize
                  << " bytes :";

        const std::size_t bytesToPrint =
            std::min<std::size_t>(32, harm->payloadSize - 16u);
        for (std::size_t offset = 0; offset < bytesToPrint; offset += 4) {
            if (offset + 4 > bytesToPrint) break;
            const std::uint32_t word =
                readU32LE(reader.bytes(), base + 16u + offset);
            std::cout << " " << word;
        }
        std::cout << '\n';

        if (harm->payloadSize >= 28) {
            std::size_t secondBlockEnd = 0u;
            {
                const std::uint32_t firstCount =
                    readU32LE(reader.bytes(), base + 16u);
                const std::size_t firstEnd =
                    16u + 4u + static_cast<std::size_t>(firstCount) * 4u + 12u;
                if (firstEnd + 4u <= harm->payloadSize) {
                    const std::uint32_t secondCount =
                        readU32LE(reader.bytes(), base + firstEnd);
                    const std::size_t secondEnd =
                        firstEnd + 4u
                        + static_cast<std::size_t>(secondCount) * 4u
                        + 12u;
                    if (secondEnd <= harm->payloadSize) {
                        secondBlockEnd = secondEnd;
                    }
                }
            }

            if (secondBlockEnd != 0u) {
                const std::size_t remaining =
                    harm->payloadSize - secondBlockEnd;
                std::size_t finiteFloatWords = 0u;
                std::size_t candidateHeaders = 0u;

                for (std::size_t off = secondBlockEnd;
                     off + 16u <= harm->payloadSize;
                     off += 4u) {
                    const std::uint32_t word =
                        readU32LE(reader.bytes(), base + off);

                    float f0 = 0.0f;
                    std::memcpy(&f0, &word, sizeof(f0));

                    if (std::isfinite(f0)
                        && std::abs(f0) < 1.0e6f) {
                        ++finiteFloatWords;
                    }

                    if (word <= 4096u && off + 16u <= harm->payloadSize) {
                        bool threeFinite = true;
                        for (std::size_t j = 1u; j <= 3u; ++j) {
                            const std::uint32_t next =
                                readU32LE(reader.bytes(), base + off + j * 4u);
                            float fv = 0.0f;
                            std::memcpy(&fv, &next, sizeof(fv));
                            if (!std::isfinite(fv)
                                || std::abs(fv) >= 100.0f) {
                                threeFinite = false;
                                break;
                            }
                        }
                        if (threeFinite) {
                            ++candidateHeaders;
                        }
                    }
                }

                std::cout << "    after second block: +" << std::hex
                          << secondBlockEnd << std::dec
                          << " bytesRemaining=" << remaining
                          << " floatLikeWords=" << finiteFloatWords
                          << " candidateSmallInt+3floats="
                          << candidateHeaders << '\n';

                if (index == 0u || index == 1u || index == 2u
                    || index == 3u || index == 4u
                    || index == 755u || index == 756u) {
                    if (remaining >= 4u) {
                        std::cout << "    post-second first/last floats:";
                        const std::size_t show =
                            std::min<std::size_t>(8u, remaining / 4u);

                        for (std::size_t j = 0u; j < show; ++j) {
                            const std::uint32_t word =
                                readU32LE(
                                    reader.bytes(),
                                    base + secondBlockEnd + j * 4u
                                );
                            float value = 0.0f;
                            std::memcpy(&value, &word, sizeof(value));
                            std::cout << " " << std::setprecision(7) << value;
                        }

                        std::cout << " ...";

                        const std::size_t wordCount = remaining / 4u;
                        const std::size_t tailStart =
                            wordCount > show ? wordCount - show : 0u;

                        for (std::size_t j = tailStart; j < wordCount; ++j) {
                            const std::uint32_t word =
                                readU32LE(
                                    reader.bytes(),
                                    base + secondBlockEnd + j * 4u
                                );
                            float value = 0.0f;
                            std::memcpy(&value, &word, sizeof(value));
                            std::cout << " " << std::setprecision(7) << value;
                        }

                        std::cout << '\n';

                        std::cout << "    post-second sampled floats:";
                        const std::size_t samplePositions[] = {
                            0u, 1u, 2u, 3u, 4u, 8u, 16u, 32u,
                            64u, 128u, 256u, 512u, 1024u, 2048u,
                            wordCount > 8u ? wordCount - 8u : 0u,
                            wordCount > 4u ? wordCount - 4u : 0u,
                            wordCount > 1u ? wordCount - 1u : 0u
                        };
                        std::size_t previous = static_cast<std::size_t>(-1);
                        for (const std::size_t pos : samplePositions) {
                            if (pos >= wordCount || pos == previous) continue;
                            previous = pos;
                            const std::uint32_t word =
                                readU32LE(
                                    reader.bytes(),
                                    base + secondBlockEnd + pos * 4u
                                );
                            float value = 0.0f;
                            std::memcpy(&value, &word, sizeof(value));
                            std::cout << " [" << pos << "="
                                      << std::setprecision(8) << value << "]";
                        }
                        std::cout << '\n';

                        std::size_t firstPositive = wordCount;
                        std::size_t lastNegative = 0u;
                        std::size_t zeroCountAtEnd = 0u;
                        while (zeroCountAtEnd < wordCount) {
                            const std::size_t pos =
                                wordCount - 1u - zeroCountAtEnd;
                            const std::uint32_t word =
                                readU32LE(
                                    reader.bytes(),
                                    base + secondBlockEnd + pos * 4u
                                );
                            float value = 0.0f;
                            std::memcpy(&value, &word, sizeof(value));
                            if (value == 0.0f) {
                                ++zeroCountAtEnd;
                            } else {
                                break;
                            }
                        }

                        for (std::size_t pos = 0u; pos < wordCount; ++pos) {
                            const std::uint32_t word =
                                readU32LE(
                                    reader.bytes(),
                                    base + secondBlockEnd + pos * 4u
                                );
                            float value = 0.0f;
                            std::memcpy(&value, &word, sizeof(value));
                            if (value > 0.0f && firstPositive == wordCount) {
                                firstPositive = pos;
                            }
                            if (value < 0.0f) {
                                lastNegative = pos;
                            }
                        }

                        const std::size_t negativeLen =
                            firstPositive == wordCount ? wordCount : firstPositive;
                        const std::size_t positiveLen =
                            wordCount >= negativeLen + zeroCountAtEnd
                                ? wordCount - negativeLen - zeroCountAtEnd
                                : 0u;

                        if (record.hasHead) {
                            const std::uint32_t firstCount =
                                readU32LE(reader.bytes(), base + 16u);
                            if (positiveLen == static_cast<std::size_t>(firstCount) + 1u) {
                                ++harmPositiveLenMatchesFirstCountPlus1;
                            }
                            if (positiveLen == static_cast<std::size_t>(firstCount) + 2u) {
                                ++harmPositiveLenMatchesFirstCountPlus2;
                            }

                            const std::size_t ceilSample =
                                (static_cast<std::size_t>(record.head.sampleCount) + 15u) / 16u;
                            if (positiveLen == ceilSample + 1u) {
                                ++harmPositiveLenMatchesCeilSamplePlus1;
                            }

                            ++harmPositiveLenMinusFirstCount[
                                static_cast<std::int64_t>(positiveLen)
                                - static_cast<std::int64_t>(firstCount)
                            ];
                            ++harmNegativeLenHistogram[
                                static_cast<std::uint32_t>(negativeLen)
                            ];
                        }

                        std::cout << "    post-second sign/zero boundary:"
                                  << " firstPositive="
                                  << (firstPositive == wordCount
                                      ? -1ll
                                      : static_cast<long long>(firstPositive))
                                  << " lastNegative=" << lastNegative
                                  << " trailingZeroFloats=" << zeroCountAtEnd
                                  << " negativeLen=" << negativeLen
                                  << " positiveLen=" << positiveLen
                                  << '\n';

                        if (index == 0u || index == 1u || index == 2u
                            || index == 3u || index == 4u
                            || index == 755u || index == 756u) {
                            const std::uint32_t harm04 =
                                readU32LE(reader.bytes(), base + 4u);
                            const std::uint32_t harm08 =
                                readU32LE(reader.bytes(), base + 8u);
                            const std::uint32_t harm0C =
                                readU32LE(reader.bytes(), base + 12u);
                            const std::uint32_t firstCount =
                                readU32LE(reader.bytes(), base + 16u);
                            const std::size_t firstEnd =
                                16u + 4u + static_cast<std::size_t>(firstCount) * 4u + 12u;
                            std::uint32_t secondCount = 0u;
                            std::size_t secondEnd = secondBlockEnd;
                            if (firstEnd + 4u <= harm->payloadSize) {
                                secondCount = readU32LE(reader.bytes(), base + firstEnd);
                            }

                            std::cout << "    boundary metadata:"
                                      << " harm04=" << harm04
                                      << " harm08=" << harm08
                                      << " harm0C=" << harm0C
                                      << " firstCount=" << firstCount
                                      << " secondCount=" << secondCount
                                      << " negativeLen=" << negativeLen
                                      << " positiveLen=" << positiveLen
                                      << " negModHarm0C="
                                      << (harm0C ? negativeLen % harm0C : 0u)
                                      << " negModSecond="
                                      << (secondCount ? negativeLen % secondCount : 0u)
                                      << '\n';

                            if (firstPositive != wordCount) {
                                std::cout << "    transition floats:";
                                const std::size_t begin =
                                    firstPositive > 6u ? firstPositive - 6u : 0u;
                                const std::size_t end =
                                    std::min(wordCount, firstPositive + 8u);
                                for (std::size_t pos = begin; pos < end; ++pos) {
                                    const std::uint32_t word =
                                        readU32LE(
                                            reader.bytes(),
                                            base + secondBlockEnd + pos * 4u
                                        );
                                    float value = 0.0f;
                                    std::memcpy(&value, &word, sizeof(value));
                                    std::cout << " [" << pos << "="
                                              << std::setprecision(8) << value
                                              << "]";
                                }
                                std::cout << '\n';
                            }
                        }
                    }
                }

                std::cout << "    post-second candidates (first 12):";
                std::size_t printed = 0u;

                for (std::size_t off = secondBlockEnd;
                     off + 16u <= harm->payloadSize && printed < 12u;
                     off += 4u) {
                    const std::uint32_t word =
                        readU32LE(reader.bytes(), base + off);

                    if (word > 4096u) continue;

                    bool threeFinite = true;
                    float vals[3] = {};

                    for (std::size_t j = 0u; j < 3u; ++j) {
                        const std::uint32_t next =
                            readU32LE(
                                reader.bytes(),
                                base + off + 4u + j * 4u
                            );
                        std::memcpy(&vals[j], &next, sizeof(vals[j]));

                        if (!std::isfinite(vals[j])
                            || std::abs(vals[j]) >= 100.0f) {
                            threeFinite = false;
                            break;
                        }
                    }

                    if (!threeFinite) continue;

                    std::cout << " [+" << std::hex << off << std::dec
                              << " word=" << word
                              << " next=" << std::setprecision(5)
                              << vals[0] << "," << vals[1] << "," << vals[2]
                              << "]";
                    ++printed;
                }

                std::cout << '\n';

                harmPostSecondAllFiniteFloats +=
                    finiteFloatWords == (remaining / 4u);
                harmPostSecondCandidateHeaderMatches += candidateHeaders;
            }

            std::cout << "    count/float-tail chain (first 12 blocks):";
            std::size_t chainCursor = 16u;
            std::size_t chainBlocks = 0;

            while (chainCursor + 4u <= harm->payloadSize
                   && chainBlocks < 12u) {
                const std::uint32_t count =
                    readU32LE(reader.bytes(), base + chainCursor);
                const std::size_t dataBytes =
                    static_cast<std::size_t>(count) * 4u;

                if (dataBytes > harm->payloadSize - chainCursor - 4u
                    || chainCursor + 4u + dataBytes + 12u > harm->payloadSize) {
                    std::cout << " [stop@" << std::hex << chainCursor
                              << std::dec << " count=" << count << "]";
                    if (chainCursor + 32u <= harm->payloadSize) {
                        std::cout << " nextWords=";
                        for (std::size_t j = 0; j < 8u; ++j) {
                            const std::uint32_t word =
                                readU32LE(
                                    reader.bytes(),
                                    base + chainCursor + j * 4u
                                );
                            float value = 0.0f;
                            std::memcpy(&value, &word, sizeof(value));
                            if (j != 0u) std::cout << ",";
                            std::cout << word
                                      << "/"
                                      << std::setprecision(6)
                                      << value
                                      << std::setprecision(6);
                        }
                    }
                    break;
                }

                const std::size_t dataStart = chainCursor + 4u;
                const std::size_t tailStart = dataStart + dataBytes;

                std::cout << " [#" << chainBlocks
                          << " @+0x" << std::hex << chainCursor << std::dec
                          << " count=" << count
                          << " tail=";

                for (std::size_t t = 0; t < 3; ++t) {
                    float value = 0.0f;
                    const std::uint32_t word =
                        readU32LE(reader.bytes(), base + tailStart + t * 4u);
                    std::memcpy(&value, &word, sizeof(value));
                    if (t != 0) std::cout << ",";
                    std::cout << std::setprecision(6) << value;
                }

                std::cout << "]";

                chainCursor = tailStart + 12u;
                ++chainBlocks;

                if (chainCursor == harm->payloadSize) {
                    break;
                }
            }

            std::cout << '\n';

            for (std::size_t offset = 16; offset <= 24; offset += 4) {
                float value = 0.0f;
                const std::uint32_t word =
                    readU32LE(reader.bytes(), base + offset);
                std::memcpy(&value, &word, sizeof(value));
                std::cout << "    +0x" << std::hex << offset << std::dec
                          << " u32=" << word
                          << " float=" << std::setprecision(9) << value
                          << std::setprecision(6) << '\n';
            }

            const std::uint32_t harm10 =
                readU32LE(reader.bytes(), base + 16u);
            if (harm10 > 0) {
                const std::size_t candidateEnd =
                    20u + static_cast<std::size_t>(harm10) * 4u;
                if (candidateEnd + 16u <= harm->payloadSize) {
                    std::cout << "    candidate first-float-array end=+0x"
                              << std::hex << candidateEnd << std::dec << '\n';
                    std::cout << "    around candidate end: ";
                    printHex(
                        reader.bytes(),
                        base + candidateEnd - 16u,
                        32
                    );

                    for (std::size_t offset = candidateEnd;
                         offset < candidateEnd + 16u;
                         offset += 4) {
                        if (offset + 4u > harm->payloadSize) break;
                        float value = 0.0f;
                        const std::uint32_t word =
                            readU32LE(reader.bytes(), base + offset);
                        std::memcpy(&value, &word, sizeof(value));
                        std::cout << "    end+0x"
                                  << std::hex << (offset - candidateEnd)
                                  << std::dec
                                  << " u32=" << word
                                  << " float=" << std::setprecision(9)
                                  << value << std::setprecision(6) << '\n';
                    }
                }
            }
        }
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
