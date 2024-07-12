#include "auto-trade-options.hpp"

#include <utility>

#include "cct_invalid_argument_exception.hpp"

namespace cct {

AutoTradeOptions::AutoTradeOptions(const json &data) {
  for (const auto &[publicExchangeName, marketAutoTradeOptions] : data.items()) {
    PublicExchangeAutoTradeOptions publicExchangeAutoTradeOptions;
    for (const auto &marketJson : marketAutoTradeOptions.items()) {
      publicExchangeAutoTradeOptions.emplace(marketJson.key(), MarketAutoTradeOptions(marketJson.value()));
    }
    _options.emplace(publicExchangeName, std::move(publicExchangeAutoTradeOptions));
  }
}

ExchangeNames AutoTradeOptions::exchangeNames() const {
  ExchangeNames exchangeNames;
  for (const auto &[publicExchangeName, publicExchangeAutoTradeOptions] : _options) {
    const int posPublicExchangeName = exchangeNames.size();
    for (const auto &[market, marketAutoTradeOptions] : publicExchangeAutoTradeOptions) {
      const int posMarket = exchangeNames.size();
      for (std::string_view account : marketAutoTradeOptions.accounts()) {
        ExchangeName exchangeName(publicExchangeName.name(), account);
        const auto it = std::find(exchangeNames.begin() + posPublicExchangeName, exchangeNames.end(), exchangeName);
        if (it == exchangeNames.end()) {
          exchangeNames.push_back(std::move(exchangeName));
        } else if (it >= exchangeNames.begin() + posMarket) {
          throw invalid_argument("Duplicated account {} for exchange {}", account, publicExchangeName);
        }
      }
    }
  }
  return exchangeNames;
}

PublicExchangeNameVector AutoTradeOptions::publicExchanges() const {
  PublicExchangeNameVector exchanges;
  for (const auto &[publicExchangeName, _] : _options) {
    exchanges.emplace_back(publicExchangeName);
  }
  std::ranges::sort(exchanges);
  return exchanges;
}

AutoTradeOptions::AccountAutoTradeOptionsPtrVector AutoTradeOptions::accountAutoTradeOptionsPtr(
    std::string_view publicExchangeName) const {
  AccountAutoTradeOptionsPtrVector accountAutoTradeOptionsPtr;
  for (const auto &[exchangeStr, publicExchangeAutoTradeOptions] : _options) {
    ExchangeName exchangeName(exchangeStr);
    if (exchangeStr.name() == publicExchangeName) {
      accountAutoTradeOptionsPtr.emplace_back(&publicExchangeAutoTradeOptions);
    }
  }
  return accountAutoTradeOptionsPtr;
}

const PublicExchangeAutoTradeOptions &AutoTradeOptions::operator[](const ExchangeName &exchangeName) const {
  const auto it = _options.find(exchangeName);
  if (it == _options.end()) {
    throw exception("No auto trade options for exchange {}", exchangeName);
  }
  return it->second;
}

}  // namespace cct