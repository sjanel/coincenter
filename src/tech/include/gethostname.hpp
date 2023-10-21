#pragma once

#include "cct_config.hpp"
#include "cct_string.hpp"

namespace cct {
class HostNameGetter {
 public:
#ifdef CCT_MSVC
  HostNameGetter();
#else
  HostNameGetter() noexcept = default;
#endif

  HostNameGetter(const HostNameGetter &) = delete;
  HostNameGetter &operator=(const HostNameGetter &) = delete;
  HostNameGetter(HostNameGetter &&) = delete;
  HostNameGetter &operator=(HostNameGetter &&) = delete;

#ifdef CCT_MSVC
  ~HostNameGetter();
#else
  ~HostNameGetter() = default;
#endif

  /// Safe version of gethostname, working in POSIX and Windows with similar behavior.
  string getHostName() const;
};

}  // namespace cct