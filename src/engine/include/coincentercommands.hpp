#pragma once

#include <string_view>

#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "coincentercommand.hpp"
#include "coincenteroptions.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"
#include "exchangesecretsinfo.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "monitoringinfo.hpp"
#include "ordersconstraints.hpp"
#include "timedef.hpp"
#include "tradeoptions.hpp"

namespace cct {

class CoincenterCommands {
 public:
  CoincenterCommands() noexcept(std::is_nothrow_default_constructible_v<string>) = default;

  CoincenterCmdLineOptions parseOptions(int argc, const char *argv[]) const;

  MonitoringInfo createMonitoringInfo(std::string_view programName,
                                      const CoincenterCmdLineOptions &cmdLineOptions) const;

  bool setFromOptions(const CoincenterCmdLineOptions &cmdLineOptions);

  MonetaryAmount startTradeAmount;
  MonetaryAmount endTradeAmount;
  CurrencyCode fromTradeCurrency;
  CurrencyCode toTradeCurrency;
  ExchangeNames tradePrivateExchangeNames;
  TradeOptions tradeOptions;

  ExchangeSecretsInfo exchangesSecretsInfo;

  Duration repeatTime{};

  int repeats = 1;

  bool isPercentageTrade = false;

  std::span<const CoincenterCommand> commands() const { return _commands; }

 private:
  using Commands = vector<CoincenterCommand>;

  Commands _commands;
};

}  // namespace cct
