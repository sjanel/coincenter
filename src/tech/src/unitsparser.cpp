#include "unitsparser.hpp"

#include <charconv>
#include <cstdint>
#include <string_view>

#include "cct_exception.hpp"
#include "stringconv.hpp"

namespace cct {

int64_t ParseNumberOfBytes(std::string_view sizeStr) {
  int64_t totalNbBytes = 0;
  while (!sizeStr.empty()) {
    auto endDigitPos = sizeStr.find_first_not_of("0123456789");
    if (endDigitPos == std::string_view::npos) {
      endDigitPos = sizeStr.size();
    }
    int64_t nbBytes = StringToIntegral<int64_t>(std::string_view(sizeStr.begin(), sizeStr.begin() + endDigitPos));
    if (nbBytes < 0) {
      throw exception("Number of bytes cannot be negative");
    }
    sizeStr.remove_prefix(endDigitPos);

    int64_t multiplier = 1;
    if (!sizeStr.empty()) {
      bool iMultiplier = 1UL < sizeStr.size() && sizeStr[1UL] == 'i';
      int64_t multiplierBase = iMultiplier ? 1024L : 1000L;
      switch (sizeStr.front()) {
        case '.':
          throw exception("Decimal number not accepted for number of bytes parsing");
        case 'T':  // NOLINT(bugprone-branch-clone)
          multiplier *= multiplierBase;
          [[fallthrough]];
        case 'G':
          multiplier *= multiplierBase;
          [[fallthrough]];
        case 'M':
          multiplier *= multiplierBase;
          [[fallthrough]];
        case 'K':
          [[fallthrough]];
        case 'k':
          multiplier *= multiplierBase;
          break;
        default:
          throw exception("Invalid suffix '{}' for number of bytes parsing", sizeStr.front());
      }
      sizeStr.remove_prefix(1UL + static_cast<std::string_view::size_type>(iMultiplier));
    }
    totalNbBytes += nbBytes * multiplier;
  }

  return totalNbBytes;
}

std::span<char> BytesToStr(int64_t numberOfBytes, std::span<char> buf) {
  static constexpr std::pair<int64_t, std::string_view> kUnits[] = {{1024L * 1024L * 1024L * 1024L, "Ti"},
                                                                    {1024L * 1024L * 1024L, "Gi"},
                                                                    {1024L * 1024L, "Mi"},
                                                                    {1024L, "Ki"},
                                                                    {1L, ""}};

  std::span<char>::size_type bufPos = 0;

  char *endBuf = buf.data() + buf.size();

  for (int unitPos = 0; numberOfBytes > 0; ++unitPos) {
    int64_t nbUnits = numberOfBytes / kUnits[unitPos].first;

    if (nbUnits != 0) {
      numberOfBytes %= kUnits[unitPos].first;

      auto [ptr, errc] = std::to_chars(buf.data() + bufPos, buf.data() + buf.size(), nbUnits);
      if (errc != std::errc()) {
        throw exception("Unable to decode integral into string");
      }

      if (ptr + kUnits[unitPos].second.size() >= endBuf) {
        throw exception("Buffer too small for number of bytes string representation");
      }

      auto [it, newEnd] = std::ranges::copy(kUnits[unitPos].second, ptr);

      bufPos = newEnd - buf.data();
    }
  }

  return {buf.data(), bufPos};
}

}  // namespace cct