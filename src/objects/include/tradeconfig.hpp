#pragma once

#include "priceoptionsdef.hpp"
#include "timedef.hpp"
#include "tradedefinitions.hpp"

namespace cct {
class TradeConfig {
 public:
  TradeConfig(Duration minPriceUpdateDuration, Duration timeout, PriceStrategy tradeStrategy,
              TradeTimeoutAction tradeTimeoutAction)
      : _minPriceUpdateDuration(minPriceUpdateDuration),
        _timeout(timeout),
        _tradeStrategy(tradeStrategy),
        _tradeTimeoutAction(tradeTimeoutAction) {}

  Duration minPriceUpdateDuration() const { return _minPriceUpdateDuration; }

  Duration timeout() const { return _timeout; }

  PriceStrategy tradeStrategy() const { return _tradeStrategy; }

  TradeTimeoutAction tradeTimeoutAction() const { return _tradeTimeoutAction; }

 private:
  Duration _minPriceUpdateDuration;
  Duration _timeout;
  PriceStrategy _tradeStrategy;
  TradeTimeoutAction _tradeTimeoutAction;
};
}  // namespace cct