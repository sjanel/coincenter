#include "loadconfiguration.hpp"

namespace cct {
LoadConfiguration::LoadConfiguration() noexcept
    : _dataDir(kDefaultDataDir), _exchangeConfigFileType(ExchangeConfigFileType::kProd) {}

LoadConfiguration::LoadConfiguration(std::string_view dataDir, ExchangeConfigFileType exchangeConfigFileType)
    : _dataDir(dataDir), _exchangeConfigFileType(exchangeConfigFileType) {}

std::string_view LoadConfiguration::exchangeConfigFile() const {
  return _exchangeConfigFileType == ExchangeConfigFileType::kProd ? kProdDefaultExchangeConfigFile
                                                                  : kTestDefaultExchangeConfigFile;
}
}  // namespace cct