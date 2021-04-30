#pragma once

#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include "cct_log.hpp"

namespace cct {
class exception : public std::exception {
 public:
  static constexpr int kMsgMaxLen = 127;

  static_assert(std::is_nothrow_default_constructible<std::string>::value &&
                    std::is_nothrow_move_constructible<std::string>::value,
                "cct::exception cannot be nothrow with a std::string");

  explicit exception(const char* str) noexcept : _info(str), _str() {
    if (str) {
      try {
        log::critical(str);
      } catch (...) {
      }
    }
    *std::begin(_storage) = '\0';
  }

  /// We cannot store a reference of a given string in an exception because of dangling reference problem.
  /// We neither can store a std::string directly as it could throw by allocating memory, which is not acceptable in an
  /// exception's constructor.
  ///
  /// Thus we store the msg in a fixed size char storage.
  /// Msg will be truncated to 'kMsgMaxLen' chars.
  explicit exception(std::string_view str) noexcept : _info(nullptr), _str() {
    try {
      log::critical(str);
    } catch (...) {
    }
    copyStrToInlineStorage(str);
  }

  explicit exception(std::string&& str) noexcept : _info(nullptr), _str(std::move(str)) {
    try {
      log::critical(_str);
    } catch (...) {
    }
    *std::begin(_storage) = '\0';
  }

  exception(const exception& o) noexcept : _info(o._info), _str() {
    if (_info) {
      *std::begin(_storage) = '\0';
    } else if (o._storage.front() != '\0') {
      _storage = o._storage;
    } else {
      copyStrToInlineStorage(o._str);
    }
  }

  exception(exception&&) noexcept = default;

  exception& operator=(const exception& o) noexcept {
    if (std::addressof(o) != this) {
      if (_str.capacity() >= o._str.size()) {
        _info = o._info;
        _storage = o._storage;
        _str = o._str;  // Copy is nothrow here as _str capacity is enough to hold o._str
      } else {          // o._str.size() is necessarily > 0
        // Our _str should grow to hold o._str, which is not possible here as we are noexcept.
        _info = nullptr;
        copyStrToInlineStorage(o._str);
        _str.clear();
      }
    }
    return *this;
  }

  exception& operator=(exception&&) noexcept = default;

  ~exception() = default;

  const char* what() const noexcept override {
    return _info ? _info : (_storage.front() == '\0' ? _str.c_str() : std::begin(_storage));
  }

 private:
  inline void copyStrToInlineStorage(std::string_view str) noexcept {
    const std::size_t sizeToCopy = std::min(sizeof(_storage), str.length());
    std::memcpy(std::begin(_storage), str.begin(), sizeToCopy);
    _storage[std::min(sizeof(_storage) - 1, sizeToCopy)] = '\0';
  }

  const char* _info;
  std::array<char, kMsgMaxLen + 1> _storage;
  std::string _str;
};
}  // namespace cct