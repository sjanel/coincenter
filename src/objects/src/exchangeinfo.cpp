#include "exchangeinfo.hpp"

#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

#include "apiquerytypeenum.hpp"
#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "currencycodevector.hpp"
#include "durationstring.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "timedef.hpp"

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

auto DustSweeperMaxNbTrades(int dustSweeperMaxNbTrades) {
  if (dustSweeperMaxNbTrades > static_cast<int>(std::numeric_limits<int16_t>::max())) {
    throw invalid_argument("{} is too large");
  }
  if (dustSweeperMaxNbTrades < 0) {
    throw invalid_argument("{} should be positive");
  }
  return static_cast<int16_t>(dustSweeperMaxNbTrades);
}
}  // namespace

ExchangeInfo::ExchangeInfo(std::string_view exchangeNameStr, std::string_view makerStr, std::string_view takerStr,
                           CurrencyCodeVector &&excludedAllCurrencies, CurrencyCodeVector &&excludedCurrenciesWithdraw,
                           CurrencyCodeVector &&preferredPaymentCurrencies,
                           MonetaryAmountByCurrencySet &&dustAmountsThreshold,
                           const APIUpdateFrequencies &apiUpdateFrequencies, Duration publicAPIRate,
                           Duration privateAPIRate, std::string_view acceptEncoding, int dustSweeperMaxNbTrades,
                           log::level::level_enum requestsCallLogLevel, log::level::level_enum requestsAnswerLogLevel,
                           bool multiTradeAllowedByDefault, bool validateDepositAddressesInFile,
                           bool placeSimulateRealOrder, bool validateApiKey)
    : _excludedCurrenciesAll(std::move(excludedAllCurrencies)),
      _excludedCurrenciesWithdrawal(std::move(excludedCurrenciesWithdraw)),
      _preferredPaymentCurrencies(std::move(preferredPaymentCurrencies)),
      _dustAmountsThreshold(std::move(dustAmountsThreshold)),
      _apiUpdateFrequencies(apiUpdateFrequencies),
      _publicAPIRate(publicAPIRate),
      _privateAPIRate(privateAPIRate),
      _acceptEncoding(acceptEncoding),
      _generalMakerRatio((MonetaryAmount(100) - MonetaryAmount(makerStr)) / 100),
      _generalTakerRatio((MonetaryAmount(100) - MonetaryAmount(takerStr)) / 100),
      _dustSweeperMaxNbTrades(DustSweeperMaxNbTrades(dustSweeperMaxNbTrades)),
      _requestsCallLogLevel(PosFromLevel(requestsCallLogLevel)),
      _requestsAnswerLogLevel(PosFromLevel(requestsAnswerLogLevel)),
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
    log::trace(" - Requests call log level      : {}", log::level::to_string_view(LevelFromPos(_requestsCallLogLevel)));
    log::trace(" - Requests answer log level    : {}",
               log::level::to_string_view(LevelFromPos(_requestsAnswerLogLevel)));
    log::trace(" - General update frequencies   : {} for public, {} for private", DurationToString(publicAPIRate),
               DurationToString(privateAPIRate));
    log::trace(" - Accept encoding              : {}", _acceptEncoding);
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
