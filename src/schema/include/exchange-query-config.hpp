#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <type_traits>

#include "apiquerytypeenum.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "duration-schema.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "optional-or-type.hpp"
#include "priceoptionsdef.hpp"
#include "timedef.hpp"

namespace cct::schema {

namespace details {

template <bool Optional>
struct ExchangeQueryHttpConfig {
  template <class T, std::enable_if_t<std::is_same_v<T, ExchangeQueryHttpConfig<true>> && !Optional, bool> = true>
  void mergeWith(const T &other) {
    if (other.timeout) {
      timeout = *other.timeout;
    }
  }

  optional_or_t<Duration, Optional> timeout;
};

template <bool Optional>
struct ExchangeQueryTradeConfig {
  template <class T, std::enable_if_t<std::is_same_v<T, ExchangeQueryTradeConfig<true>> && !Optional, bool> = true>
  void mergeWith(const T &other) {
    if (other.minPriceUpdateDuration) {
      minPriceUpdateDuration = *other.minPriceUpdateDuration;
    }
    if (other.timeout) {
      timeout = *other.timeout;
    }
    if (other.strategy) {
      strategy = *other.strategy;
    }
    if (other.timeoutMatch) {
      timeoutMatch = *other.timeoutMatch;
    }
  }

  optional_or_t<Duration, Optional> minPriceUpdateDuration{};
  optional_or_t<Duration, Optional> timeout{};
  optional_or_t<PriceStrategy, Optional> strategy{};
  optional_or_t<bool, Optional> timeoutMatch{};
};

template <bool Optional>
struct ExchangeQueryLogLevelsConfig {
  template <class T, std::enable_if_t<std::is_same_v<T, ExchangeQueryLogLevelsConfig<true>> && !Optional, bool> = true>
  void mergeWith(const T &other) {
    if (other.requestsCall) {
      requestsCall = *other.requestsCall;
    }
    if (other.requestsAnswer) {
      requestsAnswer = *other.requestsAnswer;
    }
  }

  optional_or_t<log::level::level_enum, Optional> requestsCall{};
  optional_or_t<log::level::level_enum, Optional> requestsAnswer{};
};

}  // namespace details

using ExchangeQueryHttpConfig = details::ExchangeQueryHttpConfig<false>;
using ExchangeQueryHttpConfigOptional = details::ExchangeQueryHttpConfig<true>;

using ExchangeQueryUpdateFrequencyConfig = std::map<QueryType, Duration>;

using ExchangeQueryTradeConfig = details::ExchangeQueryTradeConfig<false>;
using ExchangeQueryTradeConfigOptional = details::ExchangeQueryTradeConfig<true>;

namespace details {

template <bool Optional>
struct ExchangeQueryConfig {
  template <class T, std::enable_if_t<std::is_same_v<T, ExchangeQueryConfig<true>> && !Optional, bool> = true>
  void mergeWith(const T &other) {
    if (other.http) {
      http.mergeWith(*other.http);
    }
    if (other.logLevels) {
      logLevels.mergeWith(*other.logLevels);
    }
    if (other.trade) {
      trade.mergeWith(*other.trade);
    }
    if (other.updateFrequency) {
      for (const auto &[key, value] : *other.updateFrequency) {
        updateFrequency.insert_or_assign(key, value);
      }
    }
    if (other.acceptEncoding) {
      acceptEncoding = *other.acceptEncoding;
    }
    if (other.privateAPIRate) {
      privateAPIRate = *other.privateAPIRate;
    }
    if (other.publicAPIRate) {
      publicAPIRate = *other.publicAPIRate;
    }
    if (!other.dustAmountsThreshold.empty()) {
      dustAmountsThreshold.insert_or_assign(other.dustAmountsThreshold.begin(), other.dustAmountsThreshold.end());
    }
    if (other.dustSweeperMaxNbTrades) {
      dustSweeperMaxNbTrades = *other.dustSweeperMaxNbTrades;
    }
    if (other.marketDataSerialization) {
      marketDataSerialization = *other.marketDataSerialization;
    }
    if (other.multiTradeAllowedByDefault) {
      multiTradeAllowedByDefault = *other.multiTradeAllowedByDefault;
    }
    if (other.placeSimulateRealOrder) {
      placeSimulateRealOrder = *other.placeSimulateRealOrder;
    }
    if (other.validateApiKey) {
      validateApiKey = *other.validateApiKey;
    }
  }

  optional_or_t<ExchangeQueryHttpConfig<Optional>, Optional> http;
  optional_or_t<ExchangeQueryLogLevelsConfig<Optional>, Optional> logLevels;
  optional_or_t<ExchangeQueryTradeConfig<Optional>, Optional> trade;
  optional_or_t<ExchangeQueryUpdateFrequencyConfig, Optional> updateFrequency;
  optional_or_t<string, Optional> acceptEncoding;
  optional_or_t<Duration, Optional> privateAPIRate{};
  optional_or_t<Duration, Optional> publicAPIRate{};
  MonetaryAmountByCurrencySet dustAmountsThreshold;
  optional_or_t<int32_t, Optional> dustSweeperMaxNbTrades{};
  optional_or_t<bool, Optional> marketDataSerialization{};
  optional_or_t<bool, Optional> multiTradeAllowedByDefault{};
  optional_or_t<bool, Optional> placeSimulateRealOrder{};
  optional_or_t<bool, Optional> validateApiKey{};
};

using ExchangeQueryConfigOptional = ExchangeQueryConfig<true>;

}  // namespace details

using ExchangeQueryConfig = details::ExchangeQueryConfig<false>;

}  // namespace cct::schema

// To make enum serializable as strings
template <>
struct glz::meta<::cct::log::level::level_enum> {
  using enum ::cct::log::level::level_enum;

  static constexpr auto value = enumerate(trace, debug, info, warn, err, critical, off);
};
