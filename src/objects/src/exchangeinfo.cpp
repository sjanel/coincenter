#include "exchangeinfo.hpp"

#include <span>

#include "cct_const.hpp"
#include "cct_log.hpp"
#include "durationstring.hpp"

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

string BuildUpdateFrequenciesString(const ExchangeInfo::APIUpdateFrequencies &apiUpdateFrequencies) {
  string ret;
  ret.append("[Cur: ").append(DurationToString(apiUpdateFrequencies.freq[api::kCurrencies]));
  ret.append(", Mar: ").append(DurationToString(apiUpdateFrequencies.freq[api::kMarkets]));
  ret.append(", WFe: ").append(DurationToString(apiUpdateFrequencies.freq[api::kWithdrawalFees]));
  ret.append(", Tic: ").append(DurationToString(apiUpdateFrequencies.freq[api::kAllOrderBooks]));
  ret.append(", TdV: ").append(DurationToString(apiUpdateFrequencies.freq[api::kTradedVolume]));
  ret.append(", Pri: ").append(DurationToString(apiUpdateFrequencies.freq[api::kLastPrice]));
  ret.append(", Wal: ").append(DurationToString(apiUpdateFrequencies.freq[api::kDepositWallet]));
  ret.append(", CuB: ").append(DurationToString(apiUpdateFrequencies.freq[api::kCurrencyInfoBithumb]));
  ret.push_back(']');
  return ret;
}
}  // namespace

ExchangeInfo::ExchangeInfo(std::string_view exchangeNameStr, std::string_view makerStr, std::string_view takerStr,
                           CurrencyVector &&excludedAllCurrencies, CurrencyVector &&excludedCurrenciesWithdraw,
                           CurrencyVector &&preferredPaymentCurrencies,
                           const APIUpdateFrequencies &apiUpdateFrequencies, Duration publicAPIRate,
                           Duration privateAPIRate, bool multiTradeAllowedByDefault,
                           bool validateDepositAddressesInFile, bool placeSimulateRealOrder)
    : _excludedCurrenciesAll(std::move(excludedAllCurrencies)),
      _excludedCurrenciesWithdrawal(std::move(excludedCurrenciesWithdraw)),
      _preferredPaymentCurrencies(std::move(preferredPaymentCurrencies)),
      _apiUpdateFrequencies(apiUpdateFrequencies),
      _publicAPIRate(publicAPIRate),
      _privateAPIRate(privateAPIRate),
      _generalMakerRatio((MonetaryAmount(100) - MonetaryAmount(makerStr)) / 100),
      _generalTakerRatio((MonetaryAmount(100) - MonetaryAmount(takerStr)) / 100),
      _multiTradeAllowedByDefault(multiTradeAllowedByDefault),
      _validateDepositAddressesInFile(validateDepositAddressesInFile),
      _placeSimulateRealOrder(placeSimulateRealOrder) {
  if (log::get_level() <= log::level::trace) {
    log::trace("{} configuration", exchangeNameStr);

    log::trace(" - General excluded currencies  : {}", BuildCurrenciesString(_excludedCurrenciesAll));
    log::trace(" - Withdraw excluded currencies : {}", BuildCurrenciesString(_excludedCurrenciesWithdrawal));
    log::trace(" - Preferred payment currencies : {}", BuildCurrenciesString(_preferredPaymentCurrencies));
    log::trace(" - General update frequencies   : {} for public, {} for private", DurationToString(publicAPIRate),
               DurationToString(privateAPIRate));
    log::trace(" - Update frequencies by method : {}", BuildUpdateFrequenciesString(_apiUpdateFrequencies));
    log::trace(" - Taker / Maker fees           : {} / {}", makerStr, takerStr);
    log::trace(" - Multi Trade by default       : {}", _multiTradeAllowedByDefault ? "yes" : "no");
    log::trace(" - Validate deposit addresses   : {}{}", _validateDepositAddressesInFile ? "yes in " : "no",
               _validateDepositAddressesInFile ? kDepositAddressesFileName : "");
    log::trace(" - Order placing in simulation  : {}", _placeSimulateRealOrder ? "real, unmatchable" : "none");
  }
  if (_preferredPaymentCurrencies.empty()) {
    log::warn("{} list of preferred currencies is empty, buy and sell commands cannot perform trades", exchangeNameStr);
  }
}

}  // namespace cct