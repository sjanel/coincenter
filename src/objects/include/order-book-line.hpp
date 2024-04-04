#pragma once

#include <cstddef>
#include <cstdint>

#include "amount-price.hpp"
#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "monetaryamount.hpp"

namespace cct {

/// Represents an entry in an order book, an amount at a given price.
class OrderBookLine {
 public:
  enum class Type : int8_t { kAsk, kBid };

  /// Constructs a new OrderBookLine.
  OrderBookLine(MonetaryAmount amount, MonetaryAmount price, Type type)
      : _amountPrice(type == Type::kAsk ? -amount : amount, price) {}

  MonetaryAmount amount() const { return _amountPrice.amount; }

  MonetaryAmount price() const { return _amountPrice.price; }

 private:
  AmountPrice _amountPrice;
};

class MarketOrderBookLines {
 public:
  MarketOrderBookLines() noexcept = default;

  auto begin() const noexcept { return _orderBookLines.begin(); }
  auto end() const noexcept { return _orderBookLines.end(); }

  auto size() const noexcept { return _orderBookLines.size(); }
  auto capacity() const noexcept { return _orderBookLines.capacity(); }

  void clear() noexcept { _orderBookLines.clear(); }

  void shrink_to_fit() { _orderBookLines.shrink_to_fit(); }

  void reserve(std::size_t capacity) {
    _orderBookLines.reserve(static_cast<decltype(_orderBookLines)::size_type>(capacity));
  }

  void push(MonetaryAmount amount, MonetaryAmount price, OrderBookLine::Type type) {
    if (amount != 0) {
      _orderBookLines.emplace_back(amount, price, type);
    }
  }

  void pushAsk(MonetaryAmount amount, MonetaryAmount price) { push(amount, price, OrderBookLine::Type::kAsk); }

  void pushBid(MonetaryAmount amount, MonetaryAmount price) { push(amount, price, OrderBookLine::Type::kBid); }

  using trivially_relocatable = is_trivially_relocatable<vector<OrderBookLine>>::type;

 private:
  vector<OrderBookLine> _orderBookLines;
};

}  // namespace cct