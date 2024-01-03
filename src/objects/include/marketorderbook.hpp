#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "cct_smallvector.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "simpletable.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct {

class PriceOptions;

/// Represents an entry in an order book, an amount at a given price.
class OrderBookLine {
 public:
  /// Constructs a new OrderBookLine.
  /// @param isAsk true if it represents a 'ask' price, false if it is a 'bid' price.
  OrderBookLine(MonetaryAmount amount, MonetaryAmount price, bool isAsk)
      : _amount(isAsk ? -amount : amount), _price(price) {}

 private:
  friend class MarketOrderBook;

  MonetaryAmount _amount;
  MonetaryAmount _price;
};

/// Represents a full order book associated to a Market.
/// Important note: all convert methods do not take fees into account, they should be handled accordingly.
class MarketOrderBook {
 public:
  static constexpr int kDefaultDepth = 10;

  using OrderBookLineSpan = std::span<const OrderBookLine>;

  struct AmountAtPrice {
    MonetaryAmount amount;
    MonetaryAmount price;
  };

  using AmountPerPriceVec = SmallVector<AmountAtPrice, 8>;

  MarketOrderBook() noexcept = default;

  /// Constructs a new MarketOrderBook given a market and a list of amounts and prices.
  /// @param volAndPriNbDecimals optional to force number of decimals of amounts
  explicit MarketOrderBook(Market market, OrderBookLineSpan orderLines = OrderBookLineSpan(),
                           VolAndPriNbDecimals volAndPriNbDecimals = VolAndPriNbDecimals());

  /// Constructs a MarketOrderBook based on simple ticker information and price / amount precision
  MarketOrderBook(MonetaryAmount askPrice, MonetaryAmount askVolume, MonetaryAmount bidPrice, MonetaryAmount bidVolume,
                  VolAndPriNbDecimals volAndPriNbDecimals, int depth = kDefaultDepth);

  Market market() const { return _market; }

  bool empty() const { return _orders.empty(); }
  int size() const { return _orders.size(); }

  bool isArtificiallyExtended() const { return _isArtificiallyExtended; }

  /// Get the highest bid price that a buyer is willing to pay
  MonetaryAmount highestBidPrice() const { return priceAt(_highestBidPricePos); }

  /// Get the lowest ask price that a seller is willing to sell
  MonetaryAmount lowestAskPrice() const { return priceAt(_lowestAskPricePos); }

  /// Get the amount available at highest bid price
  MonetaryAmount amountAtBidPrice() const { return amountAt(_highestBidPricePos); }

  /// Get the amount available at lowest ask price
  MonetaryAmount amountAtAskPrice() const { return -amountAt(_lowestAskPricePos); }

  /// Compute average price as simple average of lowest ask price and highest bid price
  std::optional<MonetaryAmount> averagePrice() const;

  /// Computes the amount that could be bought immediately from the order book at given price.
  /// Note that exception could be thrown if currency of given amount is different from quote currency of the order
  /// book.
  MonetaryAmount computeCumulAmountBoughtImmediatelyAt(MonetaryAmount price) const;

  /// Computes the amount that could be sold immediately from the order book at given price.
  /// Note that exception could be thrown if currency of given amount is different from quote currency of the order
  /// book.
  MonetaryAmount computeCumulAmountSoldImmediatelyAt(MonetaryAmount price) const;

  /// Computes the max price for which amount would be bought immediately from the order book.
  /// This price may not exist (when not enough volume for instance), returns empty optional in this case
  std::optional<MonetaryAmount> computeMaxPriceAtWhichAmountWouldBeBoughtImmediately(MonetaryAmount ma) const;

  /// Computes the min price for which amount would be sold immediately from the order book.
  /// This price may not exist (when not enough volume for instance), returns empty optional in this case
  std::optional<MonetaryAmount> computeMinPriceAtWhichAmountWouldBeSoldImmediately(MonetaryAmount ma) const;

  /// Computes the list of {price / amount}s for which amount would be bought immediately from the order book.
  /// If operation is not possible, return an empty vector.
  AmountPerPriceVec computePricesAtWhichAmountWouldBeBoughtImmediately(MonetaryAmount ma) const;

  /// Computes the list of {price / amount}s for which amount would be sold immediately from the order book.
  /// If operation is not possible, return an empty vector.
  AmountPerPriceVec computePricesAtWhichAmountWouldBeSoldImmediately(MonetaryAmount ma) const;

  /// Given an amount in either base or quote currency, attempt to convert it at market price immediately and return
  /// the average price matched.
  std::optional<MonetaryAmount> computeAvgPriceForTakerAmount(MonetaryAmount amountInBaseOrQuote) const;

  /// Given an amount in either base or quote currency, attempt to convert it at market price immediately and return
  /// the worst price matched.
  std::optional<MonetaryAmount> computeWorstPriceForTakerAmount(MonetaryAmount amountInBaseOrQuote) const;

  /// Attempt to convert given amount expressed in either base or quote currency, in the other currency of this market
  /// order book. It may not be possible, in which case an empty optional will be returned.
  /// This simulates a trade at market price.
  std::optional<MonetaryAmount> convert(MonetaryAmount amountInBaseOrQuote) const {
    return amountInBaseOrQuote.currencyCode() == _market.base() ? convertBaseAmountToQuote(amountInBaseOrQuote)
                                                                : convertQuoteAmountToBase(amountInBaseOrQuote);
  }

  std::optional<MonetaryAmount> convert(MonetaryAmount amountInBaseOrQuote, const PriceOptions& priceOptions) const;

  int nbAskPrices() const { return static_cast<int>(_orders.size()) - _lowestAskPricePos; }
  int nbBidPrices() const { return _lowestAskPricePos; }

  /// Get a pair of {Price, Amount} of values positioned at given relative price from limit price.
  /// At position 0, the pair will contain average limit prices and average amounts from both highest bid and lowest ask
  /// prices.
  /// No bounds check is made.
  std::pair<MonetaryAmount, MonetaryAmount> operator[](int relativePosToLimitPrice) const;

  MonetaryAmount getHighestTheoreticalPrice() const;
  MonetaryAmount getLowestTheoreticalPrice() const;

  std::optional<MonetaryAmount> computeLimitPrice(CurrencyCode fromCurrencyCode,
                                                  const PriceOptions& priceOptions) const;

  std::optional<MonetaryAmount> computeAvgPrice(MonetaryAmount from, const PriceOptions& priceOptions) const;

  /// Print the market order book in a SimpleTable and returns it.
  /// @param conversionPriceRate prices will be multiplied to given amount to display an additional column of equivalent
  ///                            currency
  SimpleTable getTable(std::string_view exchangeName, std::optional<MonetaryAmount> conversionPriceRate) const;

 private:
  using AmountType = MonetaryAmount::AmountType;

  struct AmountPrice {
    using AmountType = MonetaryAmount::AmountType;

    bool operator==(const AmountPrice& o) const noexcept = default;

    AmountType amount = 0;
    AmountType price = 0;
  };

  // Use a SmallVector to avoid memory allocation for all order book requests (ticker)
  using AmountPriceVector = SmallVector<AmountPrice, 2 * 1>;

 public:
  using trivially_relocatable = is_trivially_relocatable<AmountPriceVector>::type;

  bool operator==(const MarketOrderBook&) const = default;

 private:
  /// Represents a total amount of waiting orders at a given price.
  /// Note that currency is not stored in situ for memory footprint reasons, but it's not an issue as we can get it from
  /// the 'Market'. Also, the amount is an integral multiplied by the number of decimals stored in the 'Market'.
  /// Example of data structure for a A/B market for instance (order is important):
  ///
  /// Sellers of A|  A price  | Buyers of A
  ///  (buying B) |   in B    | (selling B)
  ///     asks    |           |    bids
  ///---------------------------------------
  ///      -13        0.50
  ///      -11        0.49
  ///       -9        0.48
  ///       -4        0.47
  ///       -3        0.46
  ///       -2        0.45
  ///       -1        0.44                <- lowest ask price
  ///                 0.42          1     <- highest bid price
  ///                 0.41          2
  ///                 0.40          4
  ///                 0.39          5
  ///                 0.38          6
  ///                 0.37          9
  ///                 0.36          15
  ///                 0.35          20
  ///                 0.34          23

  MonetaryAmount amountAt(int pos) const {
    return MonetaryAmount(_orders[pos].amount, _market.base(), _volAndPriNbDecimals.volNbDecimals);
  }
  MonetaryAmount negAmountAt(int pos) const {
    return MonetaryAmount(-_orders[pos].amount, _market.base(), _volAndPriNbDecimals.volNbDecimals);
  }
  MonetaryAmount priceAt(int pos) const {
    return MonetaryAmount(_orders[pos].price, _market.quote(), _volAndPriNbDecimals.priNbDecimals);
  }

  std::optional<MonetaryAmount> computeAvgPriceAtWhichAmountWouldBeSoldImmediately(MonetaryAmount ma) const;

  std::optional<MonetaryAmount> computeAvgPriceAtWhichAmountWouldBeBoughtImmediately(MonetaryAmount ma) const;

  /// Attempt to convert given amount expressed in base currency to quote currency.
  /// It may not be possible, in which case an empty optional will be returned.
  /// This simulates a trade at market price.
  std::optional<MonetaryAmount> convertBaseAmountToQuote(MonetaryAmount amountInBaseCurrency) const;

  /// Attempt to convert given amount expressed in quote currency to base currency.
  /// It may not be possible, in which case an empty optional will be returned.
  /// This simulates a trade at market price.
  std::optional<MonetaryAmount> convertQuoteAmountToBase(MonetaryAmount amountInQuoteCurrency) const;

  Market _market;
  VolAndPriNbDecimals _volAndPriNbDecimals;
  AmountPriceVector _orders;
  int _highestBidPricePos = 0;
  int _lowestAskPricePos = 0;
  bool _isArtificiallyExtended = false;
};

}  // namespace cct
