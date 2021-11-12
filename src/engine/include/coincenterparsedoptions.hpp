#pragma once

#include <cstdint>
#include <string_view>

#include "cct_const.hpp"
#include "cct_string.hpp"
#include "cct_time_helpers.hpp"
#include "commandlineoptionsparser.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "tradeoptions.hpp"

namespace cct {
struct CoincenterCmdLineOptions;

class CoincenterParsedOptions {
 public:
  using Duration = Clock::duration;

  /// Parse arguments and store the overriden values in this object.
  CoincenterParsedOptions(int argc, const char *argv[]);

  MonetaryAmount startTradeAmount;
  CurrencyCode toTradeCurrency;
  PrivateExchangeName tradePrivateExchangeName;
  api::TradeOptions tradeOptions;

  CurrencyCode marketsCurrency;
  PublicExchangeNames marketsExchanges;

  Market marketForOrderBook;
  bool tickerForAll = false;
  PublicExchangeNames tickerExchanges;
  PublicExchangeNames orderBookExchanges;
  int orderbookDepth = 0;
  CurrencyCode orderbookCur;

  Market marketForConversionPath;
  PublicExchangeNames conversionPathExchanges;

  bool balanceForAll = false;
  PrivateExchangeNames balancePrivateExchanges;
  CurrencyCode balanceCurrencyCode;

  bool noSecretsForAll = false;
  PublicExchangeNames noSecretsExchanges;

  MonetaryAmount amountToWithdraw;
  PrivateExchangeName withdrawFromExchangeName, withdrawToExchangeName;
  CurrencyCode withdrawFeeCur;
  PublicExchangeNames withdrawFeeExchanges;

  Market tradedVolumeMarket;
  PublicExchangeNames tradedVolumeExchanges;

  Market lastPriceMarket;
  PublicExchangeNames lastPriceExchanges;

  string dataDir = kDefaultDataDir;

  string monitoring_address;
  string monitoring_username;
  string monitoring_password;
  uint16_t monitoring_port;
  bool useMonitoring = false;

  bool noProcess = false;
  int repeats = 1;
  Duration repeat_time = Duration::zero();

  std::string_view programName() const { return _programName; }

 protected:
  /// Constructor to be called for programs extending the command line options of 'coincenter'.
  /// Indeed, it's not possible to call the constructor with argv as it will contain some unknown arguments from higher
  /// level program
  CoincenterParsedOptions() = default;

  void setFromOptions(const CoincenterCmdLineOptions &cmdLineOptions);

 private:
  string _programName;
};
}  // namespace cct