#include "codec.hpp"

#include <cassert>
#include <cstddef>

#include "cct_cctype.hpp"
#include "cct_invalid_argument_exception.hpp"

namespace cct {

string BinToHex(std::span<const unsigned char> bindata) {
  static constexpr char kHexits[] = "0123456789abcdef";
  const int s = static_cast<int>(bindata.size());
  string ret(2 * s, '\0');
  ret.reserve(2 * s);
  for (int i = 0; i < s; ++i) {
    ret[i * 2] = kHexits[bindata[i] >> 4];
    ret[(i * 2) + 1] = kHexits[bindata[i] & 0x0F];
  }
  return ret;
}

string B64Encode(std::string_view bindata) {
  const ::std::size_t binlen = bindata.size();
  // Use = signs so the end is properly padded.
  string retval((((binlen + 2) / 3) * 4), '=');
  std::size_t outpos = 0;
  int bits_collected = 0;
  unsigned int accumulator = 0;

  static constexpr char kB64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  for (char c : bindata) {
    accumulator = (accumulator << 8) | (c & 0xffu);
    bits_collected += 8;
    while (bits_collected >= 6) {
      bits_collected -= 6;
      retval[outpos++] = kB64Table[(accumulator >> bits_collected) & 0x3fu];
    }
  }
  if (bits_collected > 0) {  // Any trailing bits that are missing.
    assert(bits_collected < 6);
    accumulator <<= 6 - bits_collected;
    retval[outpos++] = kB64Table[accumulator & 0x3fu];
  }
  assert(retval.empty() || outpos >= (retval.size() - 2));
  assert(outpos <= retval.size());
  return retval;
}

string B64Decode(std::string_view ascdata) {
  string retval;
  int bits_collected = 0;
  unsigned int accumulator = 0;

  static constexpr char kReverseTable[] = {
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63, 52, 53, 54, 55,
      56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64, 64, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
      13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31, 32,
      33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64};

  for (char c : ascdata) {
    if (isspace(c) || c == '=') {
      // Skip whitespace and padding. Be liberal in what you accept.
      continue;
    }
    if (c < 0 || kReverseTable[static_cast<unsigned char>(c)] > 63) {
      throw invalid_argument("This contains characters not legal in a base64 encoded string.");
    }
    accumulator = (accumulator << 6) | kReverseTable[static_cast<unsigned char>(c)];
    bits_collected += 6;
    if (bits_collected >= 8) {
      bits_collected -= 8;
      retval.push_back(static_cast<char>((accumulator >> bits_collected) & 0xffu));
    }
  }
  return retval;
}
}  // namespace cct
