#include "auto-trade-options.hpp"

#include <utility>

#include "auto-trade-config.hpp"
#include "cct_invalid_argument_exception.hpp"

namespace cct {

AutoTradeOptions::AutoTradeOptions(schema::AutoTradeConfig &&autoTradeConfig)
    : _autoTradeConfig(std::move(autoTradeConfig)) {}

ExchangeNames AutoTradeOptions::exchangeNames() const {
  ExchangeNames exchangeNames;
  for (const auto &[exchangeNameEnum, publicExchangeAutoTradeOptions] : _autoTradeConfig) {
    const int posPublicExchangeName = exchangeNames.size();
    for (const auto &[market, autoTradeMarketConfig] : publicExchangeAutoTradeOptions) {
      const int posMarket = exchangeNames.size();
      for (std::string_view account : autoTradeMarketConfig.accounts) {
        ExchangeName exchangeName(exchangeNameEnum, account);
        const auto it = std::find(exchangeNames.begin() + posPublicExchangeName, exchangeNames.end(), exchangeName);
        if (it == exchangeNames.end()) {
          exchangeNames.push_back(std::move(exchangeName));
        } else if (it >= exchangeNames.begin() + posMarket) {
          throw invalid_argument("Duplicated account {} for exchange {}", account, exchangeName.name());
        }
      }
    }
  }
  return exchangeNames;
}

ExchangeNameEnumVector AutoTradeOptions::publicExchanges() const {
  ExchangeNameEnumVector exchanges;
  for (const auto &[publicExchangeName, _] : _autoTradeConfig) {
    exchanges.emplace_back(publicExchangeName);
  }
  std::ranges::sort(exchanges);
  return exchanges;
}

AutoTradeOptions::AccountAutoTradeOptionsPtrVector AutoTradeOptions::accountAutoTradeOptionsPtr(
    std::string_view publicExchangeName) const {
  AccountAutoTradeOptionsPtrVector accountAutoTradeOptionsPtr;
  for (const auto &[exchangeNameEnum, publicExchangeAutoTradeOptions] : _autoTradeConfig) {
    if (kSupportedExchanges[static_cast<int>(exchangeNameEnum)] == publicExchangeName) {
      accountAutoTradeOptionsPtr.emplace_back(&publicExchangeAutoTradeOptions);
    }
  }
  return accountAutoTradeOptionsPtr;
}

const schema::AutoTradeExchangeConfig &AutoTradeOptions::operator[](ExchangeNameEnum exchangeNameEnum) const {
  const auto it = std::ranges::find_if(_autoTradeConfig, [exchangeNameEnum](const auto &exchangeConfig) {
    return exchangeConfig.first == exchangeNameEnum;
  });
  if (it == _autoTradeConfig.end()) {
    throw exception("No auto trade options for exchange {}", kSupportedExchanges[static_cast<int>(exchangeNameEnum)]);
  }
  return it->second;
}

}  // namespace cct