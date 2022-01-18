#include "monitoringinfo.hpp"

#include "cct_exception.hpp"
#include "cct_log.hpp"

namespace cct {
MonitoringInfo::MonitoringInfo(bool useMonitoring, std::string_view jobName, std::string_view address, int port,
                               std::string_view username, std::string_view password)
    : _jobName(jobName), _address(address), _username(username), _password(password), _port(useMonitoring ? port : 0) {
  if (port < 0 || port > static_cast<int>(std::numeric_limits<uint16_t>::max())) {
    throw exception("Invalid port value");
  }
  if (useMonitoring) {
    log::info("Monitoring config - Export to {}:{} user '{}', job name {}", address, port, username, jobName);
  } else {
    log::info("Monitoring disabled");
  }
}
}  // namespace cct