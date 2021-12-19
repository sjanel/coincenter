#include "exchangeinfo.hpp"

#include "cct_log.hpp"

namespace cct {

namespace {
string BuildCurrenciesString(std::span<const CurrencyCode> currencies) {
  string ret(1, '[');
  for (CurrencyCode c : currencies) {
    if (ret.size() > 1) {
      ret.push_back(',');
    }
    ret.append(c.str());
  }
  ret.push_back(']');
  return ret;
}
}  // namespace

ExchangeInfo::ExchangeInfo(std::string_view exchangeNameStr, std::string_view makerStr, std::string_view takerStr,
                           std::span<const CurrencyCode> excludedAllCurrencies,
                           std::span<const CurrencyCode> excludedCurrenciesWithdraw, int minPublicQueryDelayMs,
                           int minPrivateQueryDelayMs, bool validateDepositAddressesInFile, bool placeSimulateRealOrder)
    : _excludedCurrenciesAll(excludedAllCurrencies.begin(), excludedAllCurrencies.end()),
      _excludedCurrenciesWithdrawal(excludedCurrenciesWithdraw.begin(), excludedCurrenciesWithdraw.end()),
      _minPublicQueryDelay(std::chrono::milliseconds(minPublicQueryDelayMs)),
      _minPrivateQueryDelay(std::chrono::milliseconds(minPrivateQueryDelayMs)),
      _generalMakerRatio((MonetaryAmount(100) - MonetaryAmount(makerStr)) / 100),
      _generalTakerRatio((MonetaryAmount(100) - MonetaryAmount(takerStr)) / 100),
      _validateDepositAddressesInFile(validateDepositAddressesInFile),
      _placeSimulateRealOrder(placeSimulateRealOrder) {
  if (log::get_level() <= log::level::trace) {
    log::trace("{} config", exchangeNameStr);

    log::trace(" - General  excluded currencies       : {}", BuildCurrenciesString(_excludedCurrenciesAll));
    log::trace(" - Withdraw excluded currencies       : {}", BuildCurrenciesString(_excludedCurrenciesWithdrawal));
    log::trace(" - Query public / private delays      : {} & {} ms", minPublicQueryDelayMs, minPrivateQueryDelayMs);
    log::trace(" - Taker / Maker fees                 : {} / {}", makerStr, takerStr);
    log::trace(" - Validate deposit addresses in file : {}", _validateDepositAddressesInFile);
    log::trace(" - Place real order in simulation mode: {}", _placeSimulateRealOrder);
  }
}

}  // namespace cct