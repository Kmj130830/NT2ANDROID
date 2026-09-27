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
    std::cout << "    sampleRate       : " << head.sampleRate << " Hz\n";
    std::cout << "    referenceFrequency : "
              << head.referenceFrequency << " Hz\n";
    std::cout << "    sampleCount      : " << head.sampleCount << '\n';
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
    std::size_t headSize20 = 0;
    std::size_t flagsZero = 0;
    std::size_t sampleRate44100 = 0;

    for (const auto& record : records) {
        if (record.sections.size() == 3
            && record.sections[0].tag == "head"
            && record.sections[1].tag == "harm"
            && record.sections[2].tag == "rres"
            && record.hasHead) {
            ++structurallyValid;
            if (record.head.declaredSize == 20) ++headSize20;
            if (record.head.flags == 0) ++flagsZero;
            if (record.head.sampleRate == 44100.0f) ++sampleRate44100;
        }
    }

    std::cout << "Structure valid : "
              << structurallyValid << "/" << records.size() << '\n';
    std::cout << "head declared=20: "
              << headSize20 << "/" << structurallyValid << '\n';
    std::cout << "head flags=0    : "
              << flagsZero << "/" << structurallyValid << '\n';
    std::cout << "sampleRate=44100 : "
              << sampleRate44100 << "/" << structurallyValid << '\n';

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

        std::cout << "  prefix(20) : ";
        printHex(
            reader.bytes(),
            static_cast<std::size_t>(record.offset),
            20
        );

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
                const std::size_t sampleBytes =
                    static_cast<std::size_t>(record.head.sampleCount) * 2u;
                const std::size_t expectedPayload = 24u + sampleBytes;

                std::cout << "    first24      : ";
                printHex(
                    reader.bytes(),
                    static_cast<std::size_t>(section.payloadOffset),
                    24
                );
                std::cout << "    payload-expected-from-head : "
                          << expectedPayload << " bytes\n";
                std::cout << "    payload-minus-expected     : "
                          << static_cast<long long>(section.payloadSize)
                             - static_cast<long long>(expectedPayload)
                          << " bytes\n";
            }
        }
    }

    if (!warning.empty()) {
        std::cout << "\nwarning: " << warning << '\n';
    }

    return 0;
}
