#pragma once

#include "apikey.hpp"
#include "cct_exception.hpp"
#include "exchangeprivateapi.hpp"

namespace cct {
namespace api {
class TradeOptions;

class ExchangePrivateDefault : public ExchangePrivate {
 public:
  ExchangePrivateDefault() : ExchangePrivate(APIKey("default", "", "xxx", "xxx")) {}

  BalancePortfolio queryAccountBalance(CurrencyCode) override {
    throw exception("Cannot use default private exchange");
  }

  Wallet queryDepositWallet(CurrencyCode) override { throw exception("Cannot use default private exchange"); }

  MonetaryAmount trade(MonetaryAmount &, CurrencyCode, const TradeOptions &) override {
    throw exception("Cannot use default private exchange");
  }

  WithdrawInfo withdraw(MonetaryAmount, ExchangePrivate &) override {
    throw exception("Cannot use default private exchange");
  }
};
}  // namespace api
}  // namespace cct
