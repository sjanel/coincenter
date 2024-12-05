#include "exchange-config.hpp"

#include <string_view>

#include "cct_const.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "exchange-config-default.hpp"
#include "file.hpp"
#include "loadconfiguration.hpp"
#include "read-json.hpp"
#include "unreachable.hpp"
#include "write-json.hpp"

namespace cct::schema {

AllExchangeConfigs::AllExchangeConfigs(const LoadConfiguration &loadConfiguration) {
  switch (loadConfiguration.exchangeConfigFileType()) {
    case LoadConfiguration::ExchangeConfigFileType::kProd: {
      std::string_view filename = loadConfiguration.exchangeConfigFileName();
      File exchangeConfigFile(loadConfiguration.dataDir(), File::Type::kStatic, filename, File::IfError::kNoThrow);

      string contentStr = exchangeConfigFile.readAll();
      if (contentStr.empty()) {
        log::warn("No {} file found. Creating a default one which can be updated freely at your convenience",
                  kExchangeConfigFileName);

        auto allExchangesConfigOptional =
            ReadJsonOrThrow<schema::details::AllExchangeConfigsOptional>(ExchangeConfigDefault::kProd);
        WritePrettyJsonOrThrow(allExchangesConfigOptional);

        mergeWith(allExchangesConfigOptional);
      } else {
        auto allExchangesConfigOptional = ReadJsonOrThrow<schema::details::AllExchangeConfigsOptional>(contentStr);

        mergeWith(allExchangesConfigOptional);
      }
      break;
    }
    case LoadConfiguration::ExchangeConfigFileType::kTest: {
      auto allExchangesConfigOptional =
          ReadJsonOrThrow<schema::details::AllExchangeConfigsOptional>(ExchangeConfigDefault::kTest);

      mergeWith(allExchangesConfigOptional);
      break;
    }
    default:
      unreachable();
  }
}

void AllExchangeConfigs::mergeWith(const details::AllExchangeConfigsOptional &other) {
  for (int exchangePos = 0; exchangePos < kNbSupportedExchanges; ++exchangePos) {
    auto &exchangeConfig = _exchangeConfigs[exchangePos];
    auto exchangeNameEnum = static_cast<ExchangeNameEnum>(exchangePos);

    auto assetIt = other.asset.exchange.find(exchangeNameEnum);
    auto queryIt = other.query.exchange.find(exchangeNameEnum);
    auto tradeFeesIt = other.tradeFees.exchange.find(exchangeNameEnum);
    auto withdrawIt = other.withdraw.exchange.find(exchangeNameEnum);

    exchangeConfig.asset.mergeWith(other.asset.def);
    if (assetIt != other.asset.exchange.end()) {
      exchangeConfig.asset.mergeWith(assetIt->second);
    }

    exchangeConfig.query.mergeWith(other.query.def);
    if (queryIt != other.query.exchange.end()) {
      exchangeConfig.query.mergeWith(queryIt->second);
    }

    exchangeConfig.tradeFees.mergeWith(other.tradeFees.def);
    if (tradeFeesIt != other.tradeFees.exchange.end()) {
      exchangeConfig.tradeFees.mergeWith(tradeFeesIt->second);
    }

    exchangeConfig.withdraw.mergeWith(other.withdraw.def);
    if (withdrawIt != other.withdraw.exchange.end()) {
      exchangeConfig.withdraw.mergeWith(withdrawIt->second);
    }
  }
}

}  // namespace cct::schema