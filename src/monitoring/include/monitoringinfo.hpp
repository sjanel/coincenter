#pragma once

#include <string_view>

#include "cct_string.hpp"
#include "cct_type_traits.hpp"

namespace cct {

class MonitoringInfo {
 public:
  /// Creates an empty monitoring info without any monitoring usage
  MonitoringInfo() = default;

  MonitoringInfo(bool useMonitoring, std::string_view jobName, std::string_view address, uint16_t port,
                 std::string_view username = std::string_view(), std::string_view password = std::string_view());

  std::string_view address() const { return _address; }
  std::string_view jobName() const { return _jobName; }

  std::string_view username() const { return _username; }
  std::string_view password() const { return _password; }

  uint16_t port() const { return _port; }

  bool useMonitoring() const { return _port != 0; }

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 private:
  string _jobName;
  string _address;
  string _username;
  string _password;
  uint16_t _port = 0;
};

}  // namespace cct
