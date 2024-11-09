#pragma once

#include <cstdint>
#include <string_view>

#include "cct_json-container.hpp"

namespace cct {

class Writer {
 public:
  enum class Mode : int8_t { FromStart, Append };

  virtual ~Writer() = default;

  // Write a string and return number of bytes written
  virtual int write([[maybe_unused]] std::string_view data, [[maybe_unused]] Mode mode = Mode::FromStart) const {
    return 0;
  }

  // Write json and return number of bytes written
  int writeJson(const json::container &data, Mode mode = Mode::FromStart) const;
};

}  // namespace cct