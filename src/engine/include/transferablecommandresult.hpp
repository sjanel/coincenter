#pragma once

#include <span>
#include <utility>

#include "cct_smallvector.hpp"
#include "cct_type_traits.hpp"
#include "exchange-names.hpp"
#include "exchangename.hpp"
#include "monetaryamount.hpp"

namespace cct {
class TransferableCommandResult {
 public:
  TransferableCommandResult(ExchangeName targetedExchange, MonetaryAmount resultedAmount)
      : _targetedExchange(std::move(targetedExchange)), _resultedAmount(resultedAmount) {}

  const ExchangeName &targetedExchange() const { return _targetedExchange; }
  MonetaryAmount resultedAmount() const { return _resultedAmount; }

  using trivially_relocatable = is_trivially_relocatable<ExchangeName>::type;

  bool operator==(const TransferableCommandResult &) const noexcept = default;

 private:
  ExchangeName _targetedExchange;
  MonetaryAmount _resultedAmount;
};

using TransferableCommandResultVector = SmallVector<TransferableCommandResult, 1>;

class CoincenterCommand;

std::pair<MonetaryAmount, ExchangeNames> ComputeTradeAmountAndExchanges(
    const CoincenterCommand &cmd, std::span<const TransferableCommandResult> previousTransferableResults);

std::pair<MonetaryAmount, ExchangeName> ComputeWithdrawAmount(
    const CoincenterCommand &cmd, std::span<const TransferableCommandResult> previousTransferableResults);
}  // namespace cct