#include "exchangeinfo.hpp"

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "durationstring.hpp"
#include "stringhelpers.hpp"

namespace cct {

namespace {
template <class ContainerType>
string BuildConcatenatedString(const ContainerType &printableValues) {
  string ret(1, '[');
  for (auto value : printableValues) {
    if (ret.size() > 1) {
      ret.push_back(',');
    }
    value.appendStrTo(ret);
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
                           CurrencyCodeVector &&excludedAllCurrencies, CurrencyCodeVector &&excludedCurrenciesWithdraw,
                           CurrencyCodeVector &&preferredPaymentCurrencies,
                           MonetaryAmountByCurrencySet &&dustAmountsThreshold,
                           const APIUpdateFrequencies &apiUpdateFrequencies, Duration publicAPIRate,
                           Duration privateAPIRate, int dustSweeperMaxNbTrades, bool multiTradeAllowedByDefault,
                           bool validateDepositAddressesInFile, bool placeSimulateRealOrder, bool validateApiKey)
    : _excludedCurrenciesAll(std::move(excludedAllCurrencies)),
      _excludedCurrenciesWithdrawal(std::move(excludedCurrenciesWithdraw)),
      _preferredPaymentCurrencies(std::move(preferredPaymentCurrencies)),
      _dustAmountsThreshold(std::move(dustAmountsThreshold)),
      _apiUpdateFrequencies(apiUpdateFrequencies),
      _publicAPIRate(publicAPIRate),
      _privateAPIRate(privateAPIRate),
      _generalMakerRatio((MonetaryAmount(100) - MonetaryAmount(makerStr)) / 100),
      _generalTakerRatio((MonetaryAmount(100) - MonetaryAmount(takerStr)) / 100),
      _dustSweeperMaxNbTrades(dustSweeperMaxNbTrades),
      _multiTradeAllowedByDefault(multiTradeAllowedByDefault),
      _validateDepositAddressesInFile(validateDepositAddressesInFile),
      _placeSimulateRealOrder(placeSimulateRealOrder),
      _validateApiKey(validateApiKey) {
  if (dustSweeperMaxNbTrades > std::numeric_limits<int16_t>::max() || dustSweeperMaxNbTrades < 0) {
    throw exception("Invalid number of dust sweeper max trades '{}', should be in [0, {}]", dustSweeperMaxNbTrades,
                    std::numeric_limits<int16_t>::max());
  }
  if (log::get_level() <= log::level::trace) {
    log::trace("{} configuration", exchangeNameStr);

    log::trace(" - General excluded currencies  : {}", BuildConcatenatedString(_excludedCurrenciesAll));
    log::trace(" - Withdraw excluded currencies : {}", BuildConcatenatedString(_excludedCurrenciesWithdrawal));
    log::trace(" - Preferred payment currencies : {}", BuildConcatenatedString(_preferredPaymentCurrencies));
    log::trace(" - Dust amounts threshold       : {}", BuildConcatenatedString(_dustAmountsThreshold));
    log::trace(" - Dust sweeper nb max trades   : {}", _dustSweeperMaxNbTrades);
    log::trace(" - General update frequencies   : {} for public, {} for private", DurationToString(publicAPIRate),
               DurationToString(privateAPIRate));
    log::trace(" - Update frequencies by method : {}", BuildUpdateFrequenciesString(_apiUpdateFrequencies));
    log::trace(" - Taker / Maker fees           : {} / {}", makerStr, takerStr);
    log::trace(" - Multi Trade by default       : {}", _multiTradeAllowedByDefault ? "yes" : "no");
    log::trace(" - Validate deposit addresses   : {}{}", _validateDepositAddressesInFile ? "yes in " : "no",
               _validateDepositAddressesInFile ? kDepositAddressesFileName : "");
    log::trace(" - Order placing in simulation  : {}", _placeSimulateRealOrder ? "real, unmatchable" : "none");
    log::trace(" - Validate API Key             : {}", _validateApiKey ? "yes" : "no");
  }
  if (_preferredPaymentCurrencies.empty()) {
    log::warn("{} list of preferred currencies is empty, buy and sell commands cannot perform trades", exchangeNameStr);
  }
  if (_dustAmountsThreshold.empty()) {
    log::warn("{} set of dust amounts threshold is empty, dust sweeper is not possible", exchangeNameStr);
  }
}

}  // namespace cct
