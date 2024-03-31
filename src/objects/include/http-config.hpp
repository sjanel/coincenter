#pragma once

#include "timedef.hpp"

namespace cct {

class HttpConfig {
 public:
  explicit HttpConfig(Duration timeout) : _timeout(timeout) {}

  Duration timeout() const { return _timeout; }

 private:
  Duration _timeout;
};

}  // namespace cct