#pragma once

#include <cstdint>

namespace cct {

constexpr void write2(char* buf, int32_t value) {
  buf[0] = static_cast<char>('0' + (value / 10));
  buf[1] = static_cast<char>('0' + (value % 10));
}

constexpr void write3(char* buf, int32_t value) {
  buf[0] = static_cast<char>('0' + (value / 100));
  buf[1] = static_cast<char>('0' + ((value / 10) % 10));
  buf[2] = static_cast<char>('0' + (value % 10));
}

constexpr void write4(char* buf, int32_t value) {
  buf[0] = static_cast<char>('0' + (value / 1000));
  buf[1] = static_cast<char>('0' + ((value / 100) % 10));
  buf[2] = static_cast<char>('0' + ((value / 10) % 10));
  buf[3] = static_cast<char>('0' + (value % 10));
}

constexpr int32_t parse2(const char* ptr) { return ((ptr[0] - '0') * 10) + (ptr[1] - '0'); }

constexpr int32_t parse3(const char* ptr) { return ((ptr[0] - '0') * 100) + ((ptr[1] - '0') * 10) + (ptr[2] - '0'); }

constexpr int32_t parse4(const char* ptr) {
  return ((ptr[0] - '0') * 1000) + ((ptr[1] - '0') * 100) + ((ptr[2] - '0') * 10) + (ptr[3] - '0');
}

constexpr int32_t parse6(const char* ptr) {
  return ((ptr[0] - '0') * 100000) + ((ptr[1] - '0') * 10000) + ((ptr[2] - '0') * 1000) + ((ptr[3] - '0') * 100) +
         ((ptr[4] - '0') * 10) + (ptr[5] - '0');
}

constexpr int32_t parse9(const char* ptr) {
  return ((ptr[0] - '0') * 100000000) + ((ptr[1] - '0') * 10000000) + ((ptr[2] - '0') * 1000000) +
         ((ptr[3] - '0') * 100000) + ((ptr[4] - '0') * 10000) + ((ptr[5] - '0') * 1000) + ((ptr[6] - '0') * 100) +
         ((ptr[7] - '0') * 10) + (ptr[8] - '0');
}

constexpr auto parse(const char* ptr, int nbChars) {
  int64_t result = 0;
  for (int i = 0; i < nbChars; ++i) {
    result = (result * 10) + (ptr[i] - '0');
  }
  return result;
}

}  // namespace cct