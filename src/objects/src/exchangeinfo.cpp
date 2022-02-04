#include "exchangeinfo.hpp"

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
  string ret(1, '[');
  ret.append("Cur: ");
  ret.append(DurationToString(apiUpdateFrequencies.freq[api::kCurrencies]));
  ret.append(", Mar: ");
  ret.append(DurationToString(apiUpdateFrequencies.freq[api::kMarkets]));
  ret.append(", WFe: ");
  ret.append(DurationToString(apiUpdateFrequencies.freq[api::kWithdrawalFees]));
  ret.append(", AOb: ");
  ret.append(DurationToString(apiUpdateFrequencies.freq[api::kAllOrderBooks]));
  ret.append(", TdV: ");
  ret.append(DurationToString(apiUpdateFrequencies.freq[api::kTradedVolume]));
  ret.append(", Pri: ");
  ret.append(DurationToString(apiUpdateFrequencies.freq[api::kLastPrice]));
  ret.append(", Wal: ");
  ret.append(DurationToString(apiUpdateFrequencies.freq[api::kDepositWallet]));
  ret.append(", NbD: ");
  ret.append(DurationToString(apiUpdateFrequencies.freq[api::kNbDecimalsUnitsBithumb]));
  ret.push_back(']');
  return ret;
}
}  // namespace

ExchangeInfo::ExchangeInfo(std::string_view exchangeNameStr, std::string_view makerStr, std::string_view takerStr,
                           std::span<const CurrencyCode> excludedAllCurrencies,
                           std::span<const CurrencyCode> excludedCurrenciesWithdraw,
                           const APIUpdateFrequencies &apiUpdateFrequencies, Duration publicAPIRate,
                           Duration privateAPIRate, bool validateDepositAddressesInFile, bool placeSimulateRealOrder)
    : _excludedCurrenciesAll(excludedAllCurrencies.begin(), excludedAllCurrencies.end()),
      _excludedCurrenciesWithdrawal(excludedCurrenciesWithdraw.begin(), excludedCurrenciesWithdraw.end()),
      _apiUpdateFrequencies(apiUpdateFrequencies),
      _publicAPIRate(publicAPIRate),
      _privateAPIRate(privateAPIRate),
      _generalMakerRatio((MonetaryAmount(100) - MonetaryAmount(makerStr)) / 100),
      _generalTakerRatio((MonetaryAmount(100) - MonetaryAmount(takerStr)) / 100),
      _validateDepositAddressesInFile(validateDepositAddressesInFile),
      _placeSimulateRealOrder(placeSimulateRealOrder) {
  if (log::get_level() <= log::level::trace) {
    log::trace("{} configuration", exchangeNameStr);

    log::trace(" - General excluded currencies  : {}", BuildCurrenciesString(_excludedCurrenciesAll));
    log::trace(" - Withdraw excluded currencies : {}", BuildCurrenciesString(_excludedCurrenciesWithdrawal));
    log::trace(" - General update frequencies   : {} for public, {} for private", DurationToString(publicAPIRate),
               DurationToString(privateAPIRate));
    log::trace(" - Update frequencies by method : {}", BuildUpdateFrequenciesString(_apiUpdateFrequencies));
    log::trace(" - Taker / Maker fees           : {} / {}", makerStr, takerStr);
    log::trace(" - Validate deposit addresses   : {}{}", _validateDepositAddressesInFile ? "yes in " : "no",
               _validateDepositAddressesInFile ? kDepositAddressesFileName : "");
    log::trace(" - Order placing in simulation  : {}", _placeSimulateRealOrder ? "real, unmatchable" : "none");
  }
}

}  // namespace cct