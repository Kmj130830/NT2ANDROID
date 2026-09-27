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
    std::size_t rresFooterSecondZero = 0;

    std::map<std::uint32_t, std::size_t> versionHistogram;
    std::map<std::uint32_t, std::size_t> versionSizeHistogram;
    std::map<std::uint32_t, std::size_t> field08Histogram;

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

                if (storedSize + 12u == payload) {
                    ++rresSizeFieldMatch;
                }

                const std::uint32_t storedCountA =
                    readU32LE(reader.bytes(), base + 8);
                const std::uint32_t storedCountB =
                    readU32LE(reader.bytes(), base + 12);

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
                    if (footerSecond == 0) {
                        ++rresFooterSecondZero;
                    }
                }
            }
        }
    }

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
    std::cout << "rres countA=countB  : "
              << rresCountsEqual << "/" << records.size() << '\n';
    std::cout << "rres footer u32[1]=0: "
              << rresFooterSecondZero << "/" << records.size() << '\n';

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

            std::cout << "  record[" << i << "]"
                      << " offset=0x" << std::hex << record.offset
                      << std::dec;

            if (record.hasHead) {
                std::cout << " headN=" << record.head.sampleCount;
            }

            if (rres && rres->payloadSize >= 16) {
                const std::size_t base =
                    static_cast<std::size_t>(rres->payloadOffset);
                const std::uint32_t countA =
                    readU32LE(reader.bytes(), base + 8);
                const std::uint32_t countB =
                    readU32LE(reader.bytes(), base + 12);

                std::cout << " countA=" << countA
                          << " countB=" << countB
                          << " rresPayload=" << rres->payloadSize;
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
