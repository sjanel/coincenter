#include "exchangeinfo.hpp"

#include "cct_log.hpp"

namespace cct {

ExchangeInfo::ExchangeInfo(std::string_view exchangeNameStr, std::string_view makerStr, std::string_view takerStr,
                           std::span<const CurrencyCode> excludedAllCurrencies,
                           std::span<const CurrencyCode> excludedCurrenciesWithdraw, int minPublicQueryDelayMs,
                           int minPrivateQueryDelayMs, bool validateDepositAddressesInFile)
    : _excludedCurrenciesAll(excludedAllCurrencies.begin(), excludedAllCurrencies.end()),
      _excludedCurrenciesWithdrawal(excludedCurrenciesWithdraw.begin(), excludedCurrenciesWithdraw.end()),
      _minPublicQueryDelay(std::chrono::milliseconds(minPublicQueryDelayMs)),
      _minPrivateQueryDelay(std::chrono::milliseconds(minPrivateQueryDelayMs)),
      _generalMakerRatio((MonetaryAmount("100") - MonetaryAmount(makerStr)) / 100),
      _generalTakerRatio((MonetaryAmount("100") - MonetaryAmount(takerStr)) / 100),
      _validateDepositAddressesInFile(validateDepositAddressesInFile) {
  if (log::get_level() <= log::level::debug) {
    string excludedAssets(1, '[');
    string excludedWithdrawAssets(1, '[');
    for (CurrencyCode c : _excludedCurrenciesAll) {
      if (excludedAssets.size() > 1) {
        excludedAssets.push_back(',');
      }
      excludedAssets.append(c.str());
    }
    for (CurrencyCode c : _excludedCurrenciesWithdrawal) {
      if (excludedWithdrawAssets.size() > 1) {
        excludedWithdrawAssets.push_back(',');
      }
      excludedWithdrawAssets.append(c.str());
    }
    excludedAssets.push_back(']');
    excludedWithdrawAssets.push_back(']');

    log::trace("{} config", exchangeNameStr);

    log::trace(" - General  excluded currencies: {}", excludedAssets);
    log::trace(" - Withdraw excluded currencies: {}", excludedWithdrawAssets);
    log::trace(" - Query public / private delays: {} & {} ms", minPublicQueryDelayMs, minPrivateQueryDelayMs);
    log::trace(" - Taker / Maker fees: {} / {}", makerStr, takerStr);
  }
}

}  // namespace cct