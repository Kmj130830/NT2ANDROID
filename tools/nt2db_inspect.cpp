#include "nt2core/ntdb_reader.h"
#include "nt2core/m9p_scanner.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

std::uint32_t readU32LE(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0])
        | (static_cast<std::uint32_t>(p[1]) << 8)
        | (static_cast<std::uint32_t>(p[2]) << 16)
        | (static_cast<std::uint32_t>(p[3]) << 24);
}

float readF32LE(const std::uint8_t* p) {
    const std::uint32_t bits = readU32LE(p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

double readF64LE(const std::uint8_t* p) {
    std::uint64_t bits =
        static_cast<std::uint64_t>(p[0])
        | (static_cast<std::uint64_t>(p[1]) << 8)
        | (static_cast<std::uint64_t>(p[2]) << 16)
        | (static_cast<std::uint64_t>(p[3]) << 24)
        | (static_cast<std::uint64_t>(p[4]) << 32)
        | (static_cast<std::uint64_t>(p[5]) << 40)
        | (static_cast<std::uint64_t>(p[6]) << 48)
        | (static_cast<std::uint64_t>(p[7]) << 56);

    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void printHex(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::size_t count) {
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

void printHeadCandidates(
    const std::vector<std::uint8_t>& bytes,
    const nt2::M9pSection& head
) {
    if (head.payloadSize != 24 || head.payloadOffset + 24 > bytes.size()) {
        return;
    }

    const auto* p = bytes.data() + head.payloadOffset;

    std::cout << "    head.hex    : ";
    printHex(bytes, static_cast<std::size_t>(head.payloadOffset), 24);

    std::cout << std::setprecision(10);
    std::cout << "    head.u32    :";
    for (int i = 0; i < 6; ++i) {
        std::cout << ' ' << readU32LE(p + i * 4);
    }
    std::cout << '\n';

    std::cout << "    head.f32    :";
    for (int i = 0; i < 6; ++i) {
        std::cout << ' ' << readF32LE(p + i * 4);
    }
    std::cout << '\n';

    std::cout << "    head.f64    : "
              << readF64LE(p) << ' '
              << readF64LE(p + 8) << ' '
              << readF64LE(p + 16) << '\n';
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
    const std::size_t count = records.size() < 8 ? records.size() : 8;

    for (std::size_t i = 0; i < count; ++i) {
        const auto& record = records[i];

        std::cout << "\nrecord[" << i << "]\n";
        std::cout << "  offset : 0x" << std::hex << record.offset << std::dec << '\n';
        std::cout << "  end    : 0x" << std::hex << record.endOffset << std::dec << '\n';
        std::cout << "  size   : " << (record.endOffset - record.offset) << " bytes\n";

        for (const auto& section : record.sections) {
            std::cout << "  " << section.tag
                      << " @ 0x" << std::hex << section.offset
                      << " payload=0x" << section.payloadOffset
                      << std::dec
                      << " size=" << section.payloadSize << " bytes\n";

            if (section.tag == "head") {
                printHeadCandidates(reader.bytes(), section);
            }
        }
    }

    if (!warning.empty()) {
        std::cout << "\nwarning: " << warning << '\n';
    }

    return 0;
}
