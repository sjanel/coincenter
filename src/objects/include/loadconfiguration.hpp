#pragma once

#include "cct_const.hpp"

namespace cct {
class LoadConfiguration {
 public:
  enum class ExchangeConfigFileType : int8_t { kProd, kTest };

  LoadConfiguration() noexcept : _dataDir(kDefaultDataDir), _exchangeConfigFileType(ExchangeConfigFileType::kProd) {}

  LoadConfiguration(std::string_view dataDir, ExchangeConfigFileType exchangeConfigFileType)
      : _dataDir(dataDir), _exchangeConfigFileType(exchangeConfigFileType) {}

  std::string_view dataDir() const { return _dataDir; }

  std::string_view exchangeConfigFile() const {
    return _exchangeConfigFileType == ExchangeConfigFileType::kProd ? kProdDefaultExchangeConfigFile
                                                                    : kTestDefaultExchangeConfigFile;
  }

  ExchangeConfigFileType exchangeConfigFileType() const { return _exchangeConfigFileType; }

 private:
  static constexpr std::string_view kProdDefaultExchangeConfigFile = "exchangeconfig.json";
  static constexpr std::string_view kTestDefaultExchangeConfigFile = "exchangeconfig_test.json";

  std::string_view _dataDir;
  ExchangeConfigFileType _exchangeConfigFileType;
};
}  // namespace cct