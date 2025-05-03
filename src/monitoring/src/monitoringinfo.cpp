#include "monitoringinfo.hpp"

#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

#include "cct_invalid_argument_exception.hpp"
#include "cct_log.hpp"

namespace cct {

MonitoringInfo::MonitoringInfo(bool useMonitoring, std::string_view jobName, std::string_view address, int port,
                               std::string_view username, std::string_view password)
    : _jobName(jobName),
      _address(address),
      _username(username),
      _password(password),
      _port(useMonitoring ? static_cast<uint16_t>(port) : 0U) {
  if (port < 0 || std::cmp_less(std::numeric_limits<uint16_t>::max(), port)) {
    throw invalid_argument("Invalid port value {}", port);
  }
  if (useMonitoring) {
    log::info("Monitoring config - Export to {}:{} user '{}', job name {}", address, port, username, jobName);
  } else {
    log::debug("Monitoring disabled");
  }
}

}  // namespace cct
