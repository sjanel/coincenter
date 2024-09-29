#pragma once

#include <cstdint>

#include "cct_json.hpp"

namespace cct {

class Writer {
 public:
  enum class Mode : int8_t { FromStart, Append };

  // Write json and return number of bytes written
  virtual int write([[maybe_unused]] const json &data, [[maybe_unused]] Mode mode) const { return 0; }

  virtual ~Writer() = default;
};

}  // namespace cct