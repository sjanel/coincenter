#include "transferablecommandresult.hpp"

#include <algorithm>
#include <optional>
#include <span>
#include <utility>

#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "coincentercommand.hpp"
#include "exchange-names.hpp"
#include "exchangename.hpp"
#include "monetaryamount.hpp"

namespace cct {
namespace {
struct AmountExchangeNames {
  bool operator==(const AmountExchangeNames &) const noexcept = default;

  MonetaryAmount amount;
  ExchangeNames exchangeNames;
};

std::optional<AmountExchangeNames> AccumulateAmount(
    std::span<const TransferableCommandResult> previousTransferableResults) {
  std::optional<AmountExchangeNames> ret;
  for (const TransferableCommandResult &previousResult : previousTransferableResults) {
    const auto previousAmount = previousResult.resultedAmount();
    if (!ret) {
      ret = {previousAmount, ExchangeNames{}};
    } else if (ret->amount.currencyCode() == previousAmount.currencyCode()) {
      ret->amount += previousAmount;
    } else {
      ret.reset();
      break;
    }

    auto exchangeName = previousResult.targetedExchange();
    const auto insertIt = std::ranges::lower_bound(ret->exchangeNames, exchangeName);
    if (insertIt == ret->exchangeNames.end() || *insertIt != exchangeName) {
      ret->exchangeNames.insert(insertIt, std::move(exchangeName));
    }
  }
  return ret;
}
}  // namespace

std::pair<MonetaryAmount, ExchangeNames> ComputeTradeAmountAndExchanges(
    const CoincenterCommand &cmd, std::span<const TransferableCommandResult> previousTransferableResults) {
  // 2 input styles are possible:
  //  - standard full information with an amount to trade, a destination currency and an optional list of exchanges
  //  where to trade
  //  - a currency - the destination one, and start amount and exchange(s) should come from previous command result
  if (cmd.amount().isDefault() && !cmd.isPercentageAmount() && cmd.exchangeNames().empty()) {
    // take information from previous results
    auto optAmountExchangeNames = AccumulateAmount(previousTransferableResults);
    if (!optAmountExchangeNames) {
      log::error("Skipping trade as there are multiple currencies in previous resulted amounts");
      return {};
    }
    return {optAmountExchangeNames->amount, std::move(optAmountExchangeNames->exchangeNames)};
  }
  if (!cmd.amount().isDefault()) {
    return {cmd.amount(), cmd.exchangeNames()};
  }
  throw exception("Invalid flow for trade, should not happen (error should have been caught previously)");
}

std::pair<MonetaryAmount, ExchangeName> ComputeWithdrawAmount(
    const CoincenterCommand &cmd, std::span<const TransferableCommandResult> previousTransferableResults) {
  // 2 input styles are possible:
  //  - standard full information with an amount to withdraw, and a couple of source - destination exchanges
  //  - a single exchange (which is the target one) with the source and amount information coming from previous
  //  command result
  if (cmd.exchangeNames().size() == 1U && cmd.amount().isDefault()) {
    if (previousTransferableResults.size() != 1U) {
      log::error("Skipping withdraw apply all to {} as invalid previous transferable results size {}, expected 1",
                 cmd.exchangeNames().back(), cmd.exchangeNames().size());
      return {};
    }
    const TransferableCommandResult &previousResult = previousTransferableResults.front();

    return {previousResult.resultedAmount(), previousResult.targetedExchange()};
  }
  if (cmd.exchangeNames().size() == 2U && !cmd.amount().isDefault()) {
    return {cmd.amount(), cmd.exchangeNames().front()};
  }
  throw exception("Invalid flow for withdraw apply, should not happen (error should have been caught previously)");
}
}  // namespace cct