#include "loadconfiguration.hpp"

#include <string_view>

#include "cct_const.hpp"

namespace cct {
LoadConfiguration::LoadConfiguration() noexcept
    : _dataDir(kDefaultDataDir), _exchangeConfigFileType(ExchangeConfigFileType::kTest) {}

LoadConfiguration::LoadConfiguration(std::string_view dataDir, ExchangeConfigFileType exchangeConfigFileType)
    : _dataDir(dataDir), _exchangeConfigFileType(exchangeConfigFileType) {}

std::string_view LoadConfiguration::exchangeConfigFileName() const {
  return _exchangeConfigFileType == ExchangeConfigFileType::kProd ? kProdDefaultExchangeConfigFile
                                                                  : kTestDefaultExchangeConfigFile;
}
}  // namespace cct