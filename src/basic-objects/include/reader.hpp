#pragma once

#include "cct_string.hpp"

namespace cct {

class Reader {
 public:
  Reader() noexcept = default;

  virtual ~Reader() = default;

  // Read all content and return a string of it.
  [[nodiscard]] virtual string readAll() const { return {}; }
};

}  // namespace cct