#include "codec.hpp"

#include <cassert>
#include <cstddef>

#include "cct_cctype.hpp"
#include "cct_invalid_argument_exception.hpp"

namespace cct {

string BinToHex(std::span<const unsigned char> binData) {
  static constexpr const char* const kHexits = "0123456789abcdef";
  auto sz = binData.size();
  string ret(2 * sz, '\0');
  while (sz != 0) {
    --sz;
    ret[2 * sz] = kHexits[binData[sz] >> 4];
    ret[(2 * sz) + 1] = kHexits[binData[sz] & 0x0F];
  }
  return ret;
}

string B64Encode(std::span<const char> binData) {
  const auto binLen = binData.size();
  // Use = signs so the end is properly padded.
  string ret((((binLen + 2) / 3) * 4), '=');
  std::size_t outPos = 0;
  int bitsCollected = 0;
  unsigned int accumulator = 0;

  static constexpr const char* const kB64Table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  for (char ch : binData) {
    accumulator = (accumulator << 8) | (ch & 0xFFU);
    bitsCollected += 8;
    while (bitsCollected >= 6) {
      bitsCollected -= 6;
      ret[outPos++] = kB64Table[(accumulator >> bitsCollected) & 0x3FU];
    }
  }
  if (bitsCollected > 0) {  // Any trailing bits that are missing.
    assert(bitsCollected < 6);
    accumulator <<= 6 - bitsCollected;
    ret[outPos++] = kB64Table[accumulator & 0x3FU];
  }
  assert(ret.empty() || outPos >= (ret.size() - 2));
  assert(outPos <= ret.size());
  return ret;
}

string B64Decode(std::span<const char> ascData) {
  string ret;
  int bitsCollected = 0;
  unsigned int accumulator = 0;

  static constexpr char kReverseTable[] = {
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63, 52, 53, 54, 55,
      56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64, 64, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
      13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31, 32,
      33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64};

  for (char ch : ascData) {
    if (isspace(ch) || ch == '=') {
      // Skip whitespace and padding
      continue;
    }
    if (ch < 0 || kReverseTable[static_cast<unsigned char>(ch)] > 63) {
      throw invalid_argument("This contains characters not legal in a base64 encoded string.");
    }
    accumulator = (accumulator << 6) | kReverseTable[static_cast<unsigned char>(ch)];
    bitsCollected += 6;
    if (bitsCollected >= 8) {
      bitsCollected -= 8;
      ret.push_back(static_cast<char>((accumulator >> bitsCollected) & 0xFFU));
    }
  }
  return ret;
}
}  // namespace cct
