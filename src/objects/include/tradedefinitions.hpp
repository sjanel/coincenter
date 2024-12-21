#pragma once

#include <cstdint>

#include "cct_json.hpp"

namespace cct {
enum class TradeTimeoutAction : int8_t {
  exchange_default,  // Use exchange config file default settings
  cancel,            // When timeout of trade is reached, cancel remaining order
  match              // When timeout of trade is reached, update remaining order at market price to force match
};

enum class TradeMode : int8_t {
  simulation,  // No real trade will be made. Useful for tests.
  real         // Real trade that will be executed in the exchange
};

/// Determines the default trade type if no override is present in the command.
/// A 'Single' trade is a trade from a start amount to a destination currency, on an exchange proposing the direct
/// conversion market.
/// A 'Multi' trade gives additional trading possibilities - in case the direct market from base to
/// target currency does not exist, engine evaluates the markets path reaching destination currency and apply the trades
/// in series.
// Example:
//  - Convert XRP to XLM on an exchange only proposing XRP-BTC and BTC-XLM markets.
enum class TradeTypePolicy : int8_t {
  kDefault,          // Use exchange config file default settings
  kForceMultiTrade,  // Force multi trade possibility
  kForceSingleTrade  // Force single trade only
};

enum class TradeSyncPolicy : int8_t {
  synchronous,  // Follow lifetime of the placed order, manage the price updates until it is either matched or
                // cancelled.
  asynchronous  // Placed order will not be followed-up - trade will exits once placed.
};

}  // namespace cct

template <>
struct glz::meta<cct::TradeMode> {
  using enum cct::TradeMode;

  static constexpr auto value = enumerate(simulation, real);
};

template <>
struct glz::meta<cct::TradeSyncPolicy> {
  using enum cct::TradeSyncPolicy;

  static constexpr auto value = enumerate(synchronous, asynchronous);
};

template <>
struct glz::meta<cct::TradeTimeoutAction> {
  using enum cct::TradeTimeoutAction;

  static constexpr auto value = enumerate(exchange_default, cancel, match);
};