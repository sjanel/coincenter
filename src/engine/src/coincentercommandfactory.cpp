#include "coincentercommandfactory.hpp"

#include <chrono>
#include <string_view>
#include <utility>

#include "cct_invalid_argument_exception.hpp"
#include "coincentercommand.hpp"
#include "coincentercommandtype.hpp"
#include "coincenteroptions.hpp"
#include "currencycode.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"
#include "stringoptionparser.hpp"
#include "timedef.hpp"

namespace cct {
CoincenterCommand CoincenterCommandFactory::CreateMarketCommand(StringOptionParser &optionParser) {
  auto market = optionParser.parseMarket(StringOptionParser::FieldIs::kOptional);
  if (market.isNeutral()) {
    market = Market(optionParser.parseCurrency(), CurrencyCode());
  }
  CoincenterCommand ret(CoincenterCommandType::kMarkets);
  ret.setCur1(market.base()).setCur2(market.quote()).setExchangeNames(optionParser.parseExchanges());
  return ret;
}

CoincenterCommand CoincenterCommandFactory::createOrderCommand(CoincenterCommandType type,
                                                               StringOptionParser &optionParser) {
  auto market = optionParser.parseMarket(StringOptionParser::FieldIs::kOptional);
  if (market.isNeutral()) {
    market = Market(optionParser.parseCurrency(StringOptionParser::FieldIs::kOptional), CurrencyCode());
  }
  CoincenterCommand ret(type);
  ret.setOrdersConstraints(
         OrdersConstraints(market.base(), market.quote(), std::chrono::duration_cast<Duration>(_cmdLineOptions.minAge),
                           std::chrono::duration_cast<Duration>(_cmdLineOptions.maxAge),
                           OrdersConstraints::OrderIdSet(StringOptionParser(_cmdLineOptions.ids).getCSVValues())))
      .setExchangeNames(optionParser.parseExchanges());
  return ret;
}

CoincenterCommand CoincenterCommandFactory::createTradeCommand(CoincenterCommandType type,
                                                               StringOptionParser &optionParser) {
  CoincenterCommand command(type);
  command.setTradeOptions(_cmdLineOptions.computeTradeOptions());

  if (!_cmdLineOptions.sellAll.empty()) {
    // sell all - not possible with previous command information (probably unwanted, and dangerous)
    command.setAmount(MonetaryAmount(100, optionParser.parseCurrency()))
        .setPercentageAmount(true)
        .setExchangeNames(optionParser.parseExchanges());
  } else if (!_cmdLineOptions.tradeAll.empty()) {
    // trade all - not possible with previous command information (probably unwanted, and dangerous)
    auto market = optionParser.parseMarket();
    command.setAmount(MonetaryAmount(100, market.base()))
        .setPercentageAmount(true)
        .setCur1(market.quote())
        .setExchangeNames(optionParser.parseExchanges());
  } else {
    auto [amount, amountType] = optionParser.parseNonZeroAmount(StringOptionParser::FieldIs::kOptional);
    if (!_cmdLineOptions.isSmartTrade()) {
      command.setCur1(optionParser.parseCurrency());
    }
    if (amountType == StringOptionParser::AmountType::kNotPresent) {
      // trade command with only the destination currency
      // we should use previous command information
      if (_pPreviousCommand == nullptr) {
        throw invalid_argument("No previous command to deduce information for trade");
      }
      if (_cmdLineOptions.isSmartTrade() && _cmdLineOptions.sell.empty()) {
        throw invalid_argument("No amount / exchanges is only possible for smart sell");
      }
    } else {
      if (amount <= 0) {
        throw invalid_argument("Start trade amount should be positive");
      }
      command.setAmount(amount)
          .setPercentageAmount(amountType == StringOptionParser::AmountType::kPercentage)
          .setExchangeNames(optionParser.parseExchanges());
    }
  }

  return command;
}

CoincenterCommand CoincenterCommandFactory::createWithdrawApplyCommand(StringOptionParser &optionParser) {
  auto [amount, amountType] = optionParser.parseNonZeroAmount(StringOptionParser::FieldIs::kOptional);
  auto exchanges = optionParser.parseExchanges('-');
  if (amountType == StringOptionParser::AmountType::kNotPresent) {
    if (_pPreviousCommand == nullptr) {
      throw invalid_argument("No previous command to deduce origin exchange for withdrawal");
    }
    if (exchanges.size() != 1U) {
      throw invalid_argument("One destination exchange should be provided for withdraw with previous command");
    }
    const auto &previousCommand = *_pPreviousCommand;
    if (!IsAnyTrade(previousCommand.type())) {
      throw invalid_argument("Previous command for withdrawal should be an any trade type");
    }
  } else if (exchanges.size() != 2U) {
    throw invalid_argument("Exactly 2 exchanges 'from-to' should be provided for withdraw");
  }
  CoincenterCommand command(CoincenterCommandType::kWithdrawApply);
  command.setPercentageAmount(amountType == StringOptionParser::AmountType::kPercentage)
      .setWithdrawOptions(_cmdLineOptions.computeWithdrawOptions())
      .setExchangeNames(std::move(exchanges));
  if (amountType != StringOptionParser::AmountType::kNotPresent) {
    command.setAmount(amount);
  }
  return command;
}

CoincenterCommand CoincenterCommandFactory::createWithdrawApplyAllCommand(StringOptionParser &optionParser) {
  auto cur = optionParser.parseCurrency();
  auto exchanges = optionParser.parseExchanges('-');
  if (exchanges.size() != 2U || cur.isNeutral()) {
    throw invalid_argument("Withdraw all expects a currency with a from-to pair of exchanges");
  }
  CoincenterCommand command(CoincenterCommandType::kWithdrawApply);
  command.setPercentageAmount(true)
      .setExchangeNames(std::move(exchanges))
      .setWithdrawOptions(_cmdLineOptions.computeWithdrawOptions())
      .setAmount(MonetaryAmount(100, cur));
  return command;
}
}  // namespace cct