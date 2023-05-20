#pragma once

#include <string_view>

#include "cct_const.hpp"

namespace cct {
class LoadConfiguration {
 public:
  static constexpr std::string_view kProdDefaultExchangeConfigFile = "exchangeconfig.json";

  enum class ExchangeConfigFileType : int8_t { kProd, kTest };

  LoadConfiguration() noexcept;

  LoadConfiguration(std::string_view dataDir, ExchangeConfigFileType exchangeConfigFileType);

  std::string_view dataDir() const { return _dataDir; }

  std::string_view exchangeConfigFileName() const;

  ExchangeConfigFileType exchangeConfigFileType() const { return _exchangeConfigFileType; }

 private:
  static constexpr std::string_view kTestDefaultExchangeConfigFile = "exchangeconfig_test.json";

  std::string_view _dataDir;
  ExchangeConfigFileType _exchangeConfigFileType;
};
}  // namespace cct