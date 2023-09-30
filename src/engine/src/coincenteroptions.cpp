#include "coincenteroptions.hpp"

#include <cstdlib>
#include <iostream>

#include "cct_const.hpp"
#include "curlhandle.hpp"
#include "ssl_sha.hpp"

namespace cct {

std::string_view SelectDefaultDataDir() noexcept {
  const char* pDataDirEnvValue = std::getenv("CCT_DATA_DIR");
  if (pDataDirEnvValue != nullptr) {
    return pDataDirEnvValue;
  }
  return kDefaultDataDir;
}

bool CoincenterCmdLineOptions::isSmartTrade() const noexcept {
  return !buy.empty() || !sell.empty() || !sellAll.empty();
}

void CoincenterCmdLineOptions::PrintVersion(std::string_view programName) noexcept {
  std::cout << programName << " version " << CCT_VERSION << std::endl;
  std::cout << "compiled with " << CCT_COMPILER_VERSION << " on " << __DATE__ << " at " << __TIME__ << std::endl;
  std::cout << "              " << GetCurlVersionInfo() << std::endl;
  std::cout << "              " << ssl::GetOpenSSLVersion() << std::endl;
}

void CoincenterCmdLineOptions::mergeGlobalWith(const CoincenterCmdLineOptions& other) {
  static const auto defaultDataDir = SelectDefaultDataDir();
  if (other.dataDir != defaultDataDir) {
    dataDir = other.dataDir;
  }
  if (!other.logConsole.empty()) {
    logConsole = other.logConsole;
  }
  if (!other.logFile.empty()) {
    logFile = other.logFile;
  }
  if (other.noSecrets) {
    noSecrets = other.noSecrets;
  }
  if (other.repeatTime != kDefaultRepeatTime) {
    repeatTime = other.repeatTime;
  }
  if (other.monitoringAddress != kDefaultMonitoringIPAddress) {
    monitoringAddress = other.monitoringAddress;
  }
  if (!other.monitoringUsername.empty()) {
    monitoringUsername = other.monitoringUsername;
  }
  if (!other.monitoringPassword.empty()) {
    monitoringPassword = other.monitoringPassword;
  }

  if (other.repeats.isPresent()) {
    repeats = other.repeats;
  }

  if (other.monitoringPort != kDefaultMonitoringPort) {
    monitoringPort = other.monitoringPort;
  }
  if (other.useMonitoring) {
    useMonitoring = other.useMonitoring;
  }
}

}  // namespace cct
