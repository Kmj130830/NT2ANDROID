#include "nt2core/ntdb_reader.h"

#include "nt2core/m9p_scanner.h"

#include <fstream>
#include <iterator>

namespace nt2 {

bool NtdbReader::open(const std::string& path, std::string* error) {
    bytes_.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "Unable to open NTDB file: " + path;
        return false;
    }

    file.unsetf(std::ios::skipws);
    bytes_.assign(
        std::istream_iterator<std::uint8_t>(file),
        std::istream_iterator<std::uint8_t>()
    );

    if (bytes_.empty()) {
        if (error) *error = "NTDB file is empty.";
        return false;
    }

    return true;
}

NtdbSummary NtdbReader::inspect() const noexcept {
    NtdbSummary summary;
    summary.fileSize = bytes_.size();

    if (bytes_.size() >= 4) {
        summary.validMagic =
            bytes_[0] == static_cast<std::uint8_t>('M') &&
            bytes_[1] == static_cast<std::uint8_t>('9') &&
            bytes_[2] == static_cast<std::uint8_t>('D') &&
            bytes_[3] == static_cast<std::uint8_t>('B');
    }

    const auto records = M9pScanner::scan(bytes_);
    summary.m9pCount = records.size();

    if (!records.empty()) {
        summary.firstM9pOffset = records.front().offset;
        summary.lastM9pOffset = records.back().offset;
    }

    return summary;
}

} // namespace nt2
