#include "market-auto-trade-options.hpp"

namespace cct {

MarketAutoTradeOptions::MarketAutoTradeOptions(const json &data)
    : _algorithmName(), _repeatTime(), _baseStartAmount(), _quoteStartAmount(), _stopCriteria() {}

}  // namespace cct