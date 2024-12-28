#include "auto-trade-processor.hpp"

#include <thread>

#include "auto-trade-options.hpp"
#include "cct_exception.hpp"
#include "timestring.hpp"

namespace cct {

AutoTradeProcessor::AutoTradeProcessor(const AutoTradeOptions &autoTradeOptions)
    : _exchangeStatusVector(autoTradeOptions.publicExchanges().size()) {
  int publicExchangePos = 0;
  for (const auto &[exchangeNameEnum, publicExchangeAutoTradeOptions] : autoTradeOptions) {
    ExchangeStatus &selectedExchangesStatus = _exchangeStatusVector[publicExchangePos];
    selectedExchangesStatus.pPublicExchangeAutoTradeOptions = &publicExchangeAutoTradeOptions;
    for (const auto &[market, marketAutoTradeOptions] : publicExchangeAutoTradeOptions) {
      MarketStatus &marketStatus = selectedExchangesStatus.marketStatusVector.emplace_back();

      marketStatus.market = market;
      marketStatus.pMarketAutoTradeOptions = &marketAutoTradeOptions;

      for (std::string_view account : marketAutoTradeOptions.accounts) {
        marketStatus.privateExchangeNames.emplace_back(exchangeNameEnum, account);
      }
    }
    ++publicExchangePos;
  }
}

namespace {
const auto &GetAutoTradeMarketConfig(Market market, const auto &publicExchangeAutoTradeOptions) {
  const auto it = publicExchangeAutoTradeOptions.find(market);
  if (it == publicExchangeAutoTradeOptions.end()) {
    throw exception("Should not happen - market not found in account auto trade options");
  }
  return it->second;
}

bool IsQueryTooEarly(TimePoint nowTs, const auto &marketStatus, const auto &publicExchangeAutoTradeOptions) {
  const auto &marketAutoTradeOptions = GetAutoTradeMarketConfig(marketStatus.market, publicExchangeAutoTradeOptions);
  return marketStatus.lastQueryTime + marketAutoTradeOptions.repeatTime.duration > nowTs;
}
}  // namespace

AutoTradeProcessor::SelectedMarketVector AutoTradeProcessor::computeSelectedMarkets() {
  SelectedMarketVector selectedMarketVector;

  auto ts = Clock::now();

  TimePoint earliestQueryTime = TimePoint::max();

  for (ExchangeStatus &exchangeStatus : _exchangeStatusVector) {
    const auto &publicExchangeAutoTradeOptions = *exchangeStatus.pPublicExchangeAutoTradeOptions;

    auto &marketStatusVector = exchangeStatus.marketStatusVector;

    if (marketStatusVector.empty()) {
      continue;
    }

    // Sort markets by ascending last query time, discarding those (placed at the end of the vector) which cannot be
    // queried right now
    std::ranges::sort(marketStatusVector,
                      [ts, &publicExchangeAutoTradeOptions](const MarketStatus &lhs, const MarketStatus &rhs) {
                        const bool lhsIsTooEarly = IsQueryTooEarly(ts, lhs, publicExchangeAutoTradeOptions);
                        const bool rhsIsTooEarly = IsQueryTooEarly(ts, rhs, publicExchangeAutoTradeOptions);

                        if (lhsIsTooEarly != rhsIsTooEarly) {
                          return !lhsIsTooEarly;
                        }

                        return lhs.lastQueryTime < rhs.lastQueryTime;
                      });

    MarketStatus &selectedMarketStatus = marketStatusVector.front();
    if (IsQueryTooEarly(ts, selectedMarketStatus, publicExchangeAutoTradeOptions)) {
      const auto repeatTime =
          GetAutoTradeMarketConfig(selectedMarketStatus.market, publicExchangeAutoTradeOptions).repeatTime.duration;
      earliestQueryTime = std::min(earliestQueryTime, selectedMarketStatus.lastQueryTime + repeatTime);
      continue;
    }

    selectedMarketStatus.lastQueryTime = ts;
    selectedMarketVector.emplace_back(selectedMarketStatus.privateExchangeNames, selectedMarketStatus.market);
  }

  if (selectedMarketVector.empty() && earliestQueryTime != TimePoint::max()) {
    log::debug("Sleeping until {}", TimeToString(earliestQueryTime));
    std::this_thread::sleep_until(earliestQueryTime + std::chrono::milliseconds(1));
    selectedMarketVector = computeSelectedMarkets();
    if (selectedMarketVector.empty()) {
      throw exception("Waiting sufficient time should return at least one market for the next turn");
    }
  }

  return selectedMarketVector;
}

}  // namespace cct