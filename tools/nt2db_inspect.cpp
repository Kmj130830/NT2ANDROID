#include "nt2core/ntdb_reader.h"
#include "nt2core/m9p_scanner.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

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
    std::cout << "    flags    : 0x" << std::hex << head.flags << std::dec << '\n';
    std::cout << std::setprecision(10);
    std::cout << "    field+08 : " << head.field08F32 << " (float)\n";
    std::cout << "    referenceFrequency : "
              << head.referenceFrequency << " Hz\n";
    std::cout << "    sampleCount : " << head.sampleCount << '\n';
    std::cout << std::setprecision(6);
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
    std::cout << "M9DB magic     : " << (summary.validMagic ? "yes" : "no") << '\n';
    std::cout << "M9P records    : " << summary.m9pCount << '\n';

    if (summary.m9pCount != 0) {
        std::cout << "First M9P      : 0x"
                  << std::hex << summary.firstM9pOffset << std::dec << '\n';
        std::cout << "Last M9P       : 0x"
                  << std::hex << summary.lastM9pOffset << std::dec << '\n';
    }

    std::string warning;
    const auto records = nt2::M9pScanner::scan(reader.bytes(), &warning);

    std::size_t structurallyValid = 0;
    std::size_t lengthValid = 0;
    std::size_t version20250410 = 0;
    std::size_t headSize20 = 0;
    std::size_t flagsZero = 0;

    for (const auto& record : records) {
        if (record.sections.size() == 3
            && record.sections[0].tag == "head"
            && record.sections[1].tag == "harm"
            && record.sections[2].tag == "rres"
            && record.hasHead) {
            ++structurallyValid;
        }
        if (record.lengthMatches) ++lengthValid;
        if (record.hasVersion && record.version.value == 20250410u) {
            ++version20250410;
        }
        if (record.hasHead && record.head.declaredSize == 20) ++headSize20;
        if (record.hasHead && record.head.flags == 0) ++flagsZero;
    }

    std::cout << "Structure valid   : "
              << structurallyValid << "/" << records.size() << '\n';
    std::cout << "Length field valid: "
              << lengthValid << "/" << records.size() << '\n';
    std::cout << "ver=20250410      : "
              << version20250410 << "/" << records.size() << '\n';
    std::cout << "head declared=20  : "
              << headSize20 << "/" << records.size() << '\n';
    std::cout << "head flags=0       : "
              << flagsZero << "/" << records.size() << '\n';

    const std::size_t count = records.size() < 8 ? records.size() : 8;

    for (std::size_t i = 0; i < count; ++i) {
        const auto& record = records[i];

        std::cout << "\nrecord[" << i << "]\n";
        std::cout << "  offset : 0x" << std::hex
                  << record.offset << std::dec << '\n';
        std::cout << "  end    : 0x" << std::hex
                  << record.endOffset << std::dec << '\n';
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

        if (record.hasVersion) {
            std::cout << "  ver    : "
                      << record.version.value
                      << " (0x" << std::hex
                      << record.version.value
                      << std::dec << ")\n";
        }

        for (const auto& section : record.sections) {
            std::cout << "  " << section.tag
                      << " @ 0x" << std::hex << section.offset
                      << " payload=0x" << section.payloadOffset
                      << std::dec
                      << " size=" << section.payloadSize << " bytes\n";

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
