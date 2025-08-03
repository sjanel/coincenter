#include "unitsparser.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

#include "cct_exception.hpp"
#include "cct_string.hpp"
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

namespace {
constexpr std::pair<int64_t, std::string_view> kBytesUnits[] = {{static_cast<int64_t>(1024) * 1024 * 1024 * 1024, "Ti"},
                                                                {static_cast<int64_t>(1024) * 1024 * 1024, "Gi"},
                                                                {static_cast<int64_t>(1024) * 1024, "Mi"},
                                                                {static_cast<int64_t>(1024), "Ki"},
                                                                {static_cast<int64_t>(1), ""}};
}

std::span<char> BytesToBuffer(int64_t numberOfBytes, std::span<char> buf, int nbSignificantUnits) {
  char *begBuf = buf.data();
  char *endBuf = begBuf + buf.size();

  if (numberOfBytes < 0) {
    *begBuf = '-';
    ++begBuf;

    numberOfBytes = -numberOfBytes;
  }

  for (int unitPos = 0; numberOfBytes > 0 && nbSignificantUnits > 0; ++unitPos) {
    int64_t nbUnits = numberOfBytes / kBytesUnits[unitPos].first;

    if (nbUnits != 0) {
      numberOfBytes %= kBytesUnits[unitPos].first;

      auto [ptr, errc] = std::to_chars(begBuf, endBuf, nbUnits);
      if (errc != std::errc()) {
        throw exception("Unable to decode integral into string");
      }

      if (ptr + kBytesUnits[unitPos].second.size() > endBuf) {
        throw exception("Buffer too small for number of bytes string representation");
      }

      begBuf = std::ranges::copy(kBytesUnits[unitPos].second, ptr).out;

      --nbSignificantUnits;
    }
  }

  return {buf.data(), begBuf};
}

int64_t BytesToStrLen(int64_t numberOfBytes, int nbSignificantUnits) {
  int64_t len = 0;

  if (numberOfBytes < 0) {
    ++len;
    numberOfBytes = -numberOfBytes;
  }
  for (int unitPos = 0; numberOfBytes > 0 && nbSignificantUnits > 0; ++unitPos) {
    int64_t nbUnits = numberOfBytes / kBytesUnits[unitPos].first;

    if (nbUnits != 0) {
      numberOfBytes %= kBytesUnits[unitPos].first;

      auto intStr = IntegralToCharVector(nbUnits);

      len += intStr.size();
      len += kBytesUnits[unitPos].second.size();

      --nbSignificantUnits;
    }
  }

  return len;
}

string BytesToStr(int64_t numberOfBytes, int nbSignificantUnits) {
  string ret(BytesToStrLen(numberOfBytes, nbSignificantUnits), '\0');
  BytesToBuffer(numberOfBytes, ret, nbSignificantUnits);
  return ret;
}

}  // namespace cct
