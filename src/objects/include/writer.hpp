#pragma once

#include "cct_json.hpp"

namespace cct {

class Writer {
 public:
  // Write json and return number of bytes written
  virtual int write([[maybe_unused]] const json &data) const { return 0; }

  virtual ~Writer() = default;
};

}  // namespace cct