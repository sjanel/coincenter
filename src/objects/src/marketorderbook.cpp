#include "marketorderbook.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <string_view>
#include <utility>

#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "priceoptions.hpp"
#include "priceoptionsdef.hpp"
#include "simpletable.hpp"
#include "timedef.hpp"
#include "unreachable.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct {

MarketOrderBook::MarketOrderBook(TimePoint timeStamp, Market market, OrderBookLineSpan orderLines,
                                 VolAndPriNbDecimals volAndPriNbDecimals)
    : _time(timeStamp), _market(market), _volAndPriNbDecimals(volAndPriNbDecimals) {
  const int nbPrices = static_cast<int>(orderLines.size());
  _orders.reserve(nbPrices);
  if (_volAndPriNbDecimals == VolAndPriNbDecimals()) {
    for (const OrderBookLine& orderBookLine : orderLines) {
      _volAndPriNbDecimals.volNbDecimals =
          std::min(_volAndPriNbDecimals.volNbDecimals, orderBookLine._amount.currentMaxNbDecimals());
      _volAndPriNbDecimals.priNbDecimals =
          std::min(_volAndPriNbDecimals.priNbDecimals, orderBookLine._price.currentMaxNbDecimals());
    }
  }
  if (nbPrices == 0) {
    _lowestAskPricePos = 0;
    _highestBidPricePos = 0;
  } else {
    for (const OrderBookLine& orderBookLine : orderLines) {
      assert(orderBookLine._amount.currencyCode() == market.base() &&
             orderBookLine._price.currencyCode() == market.quote());
      // amounts cannot be nullopt here
      if (orderBookLine._amount == 0) {
        // Just ignore empty lines
        continue;
      }

      const auto amountIntegral = orderBookLine._amount.amount(_volAndPriNbDecimals.volNbDecimals);
      const auto priceIntegral = orderBookLine._price.amount(_volAndPriNbDecimals.priNbDecimals);

      assert(amountIntegral.has_value());
      assert(priceIntegral.has_value());

      _orders.emplace_back(*amountIntegral, *priceIntegral);
    }

    std::ranges::sort(_orders, [](AmountPrice lhs, AmountPrice rhs) { return lhs.price < rhs.price; });
    auto adjacentFindIt =
        std::ranges::adjacent_find(_orders, [](AmountPrice lhs, AmountPrice rhs) { return lhs.price == rhs.price; });
    if (adjacentFindIt != _orders.end()) {
      throw exception("Forbidden duplicate price {} in the order book for market {}", adjacentFindIt->price, market);
    }

    auto highestBidPriceIt =
        std::ranges::partition_point(_orders, [](AmountPrice amountPrice) { return amountPrice.amount > 0; });
    _highestBidPricePos = static_cast<int>(highestBidPriceIt - _orders.begin() - 1);
    _lowestAskPricePos = static_cast<int>(
        std::find_if(highestBidPriceIt, _orders.end(), [](AmountPrice amountPrice) { return amountPrice.amount < 0; }) -
        _orders.begin());
  }
}

MarketOrderBook::MarketOrderBook(TimePoint timeStamp, MonetaryAmount askPrice, MonetaryAmount askVolume,
                                 MonetaryAmount bidPrice, MonetaryAmount bidVolume,
                                 VolAndPriNbDecimals volAndPriNbDecimals, int depth)
    : _time(timeStamp),
      _market(askVolume.currencyCode(), askPrice.currencyCode()),
      _isArtificiallyExtended(depth > 1),
      _volAndPriNbDecimals(volAndPriNbDecimals) {
  if (depth <= 0) {
    throw exception("Invalid depth, should be strictly positive");
  }

  static constexpr std::string_view kErrNegVolumeMsg = " for MarketOrderbook creation, should be strictly positive";

  if (askVolume <= 0) {
    throw exception("Invalid ask volume {}{}", askVolume, kErrNegVolumeMsg);
  }
  if (bidVolume <= 0) {
    throw exception("Invalid bid volume {}{}", bidVolume, kErrNegVolumeMsg);
  }

  static constexpr MonetaryAmount::RoundType roundType = MonetaryAmount::RoundType::kNearest;

  askPrice.round(_volAndPriNbDecimals.priNbDecimals, roundType);
  bidPrice.round(_volAndPriNbDecimals.priNbDecimals, roundType);
  askVolume.round(_volAndPriNbDecimals.volNbDecimals, roundType);
  bidVolume.round(_volAndPriNbDecimals.volNbDecimals, roundType);

  if (askVolume == 0) {
    throw exception("Number of decimals {} is too small for given start volume {}", _volAndPriNbDecimals.volNbDecimals,
                    askVolume);
  }
  if (bidVolume == 0) {
    throw exception("Number of decimals {} is too small for given start volume {}", _volAndPriNbDecimals.volNbDecimals,
                    bidVolume);
  }

  std::optional<AmountType> optStepPrice = (askPrice - bidPrice).amount(_volAndPriNbDecimals.priNbDecimals);
  if (!optStepPrice) {
    throw exception("Invalid number of decimals {} for ask price {} and bid price {}",
                    _volAndPriNbDecimals.priNbDecimals, askPrice, bidPrice);
  }
  if (*optStepPrice <= 0) {
    throw exception(
        "Invalid ask price {} and bid price {} for MarketOrderbook creation. Ask price should be larger than Bid "
        "price",
        askPrice, bidPrice);
  }
  const AmountPrice::AmountType stepPrice = *optStepPrice;

  std::optional<AmountType> optBidVol = bidVolume.amount(_volAndPriNbDecimals.volNbDecimals);
  std::optional<AmountType> optAskVol = askVolume.amount(_volAndPriNbDecimals.volNbDecimals);
  if (!optBidVol) {
    throw exception("Cannot retrieve amount from bid volume {} with {} decimals", bidVolume,
                    _volAndPriNbDecimals.volNbDecimals);
  }
  if (!optAskVol) {
    throw exception("Cannot retrieve amount from ask volume {} with {} decimals", askVolume,
                    _volAndPriNbDecimals.volNbDecimals);
  }

  std::optional<AmountType> optBidPri = bidPrice.amount(_volAndPriNbDecimals.priNbDecimals);
  std::optional<AmountType> optAskPri = askPrice.amount(_volAndPriNbDecimals.priNbDecimals);
  if (!optBidPri) {
    throw exception("Cannot retrieve amount from bid price {} with {} decimals", bidPrice,
                    _volAndPriNbDecimals.priNbDecimals);
  }
  if (!optAskPri) {
    throw exception("Cannot retrieve amount from ask price {} with {} decimals", askPrice,
                    _volAndPriNbDecimals.priNbDecimals);
  }

  const AmountPrice refBidAmountPrice(*optBidVol, *optBidPri);
  const AmountPrice refAskAmountPrice(-(*optAskVol), *optAskPri);
  const AmountPrice::AmountType simulatedStepVol = std::midpoint(refBidAmountPrice.amount, -refAskAmountPrice.amount);

  constexpr AmountPrice::AmountType kMaxVol = std::numeric_limits<AmountPrice::AmountType>::max() / 2;

  _orders.resize(depth * 2);

  // Add bid lines first
  for (int currentDepth = 0; currentDepth < depth; ++currentDepth) {
    AmountPrice amountPrice = currentDepth == 0 ? refBidAmountPrice : _orders[depth - currentDepth];
    amountPrice.price -= stepPrice * currentDepth;
    if (currentDepth != 0 && amountPrice.amount < kMaxVol) {
      amountPrice.amount += simulatedStepVol / 2;
    }
    _orders[depth - currentDepth - 1] = std::move(amountPrice);
  }
  _highestBidPricePos = depth - 1;
  _lowestAskPricePos = _highestBidPricePos + 1;

  // Finally add ask lines
  for (int currentDepth = 0; currentDepth < depth; ++currentDepth) {
    AmountPrice amountPrice = currentDepth == 0 ? refAskAmountPrice : _orders[depth + currentDepth - 1];
    amountPrice.price += stepPrice * currentDepth;
    if (currentDepth != 0 && -amountPrice.amount < kMaxVol) {
      amountPrice.amount -= simulatedStepVol / 2;
    }
    _orders[depth + currentDepth] = std::move(amountPrice);
  }
}

std::optional<MonetaryAmount> MarketOrderBook::averagePrice() const {
  switch (_orders.size()) {
    case 0U:
      return std::nullopt;
    case 1U:
      return MonetaryAmount(_orders.front().price, _market.quote(), _volAndPriNbDecimals.priNbDecimals);
    default:
      // std::midpoint computes safely the average of two values (without overflow)
      return MonetaryAmount(std::midpoint(_orders[_lowestAskPricePos].price, _orders[_highestBidPricePos].price),
                            _market.quote(), _volAndPriNbDecimals.priNbDecimals);
  }
}

MonetaryAmount MarketOrderBook::computeCumulAmountBoughtImmediatelyAt(MonetaryAmount price) const {
  AmountType integralAmountRep = 0;
  const int nbOrders = _orders.size();
  for (int pos = _lowestAskPricePos; pos < nbOrders && priceAt(pos) <= price; ++pos) {
    integralAmountRep -= _orders[pos].amount;  // Minus as amounts are negative here
  }
  return {integralAmountRep, _market.base(), _volAndPriNbDecimals.volNbDecimals};
}

MonetaryAmount MarketOrderBook::computeCumulAmountSoldImmediatelyAt(MonetaryAmount price) const {
  AmountType integralAmountRep = 0;
  for (int pos = _highestBidPricePos; pos >= 0 && priceAt(pos) >= price; --pos) {
    integralAmountRep += _orders[pos].amount;
  }
  return {integralAmountRep, _market.base(), _volAndPriNbDecimals.volNbDecimals};
}

std::optional<MonetaryAmount> MarketOrderBook::computeMaxPriceAtWhichAmountWouldBeBoughtImmediately(
    MonetaryAmount ma) const {
  if (ma.currencyCode() != _market.base()) {
    throw exception("Given amount should be in the base currency of this market");
  }
  AmountType integralAmountRep = 0;
  const std::optional<AmountType> integralTotalAmountOpt = ma.amount(_volAndPriNbDecimals.volNbDecimals);
  if (!integralTotalAmountOpt) {
    return std::nullopt;
  }
  const AmountType integralTotalAmount = *integralTotalAmountOpt;
  const int nbOrders = _orders.size();
  for (int pos = _highestBidPricePos + 1; pos < nbOrders && integralAmountRep <= integralTotalAmount; ++pos) {
    integralAmountRep -= _orders[pos].amount;  // -= because amount is < 0 here
    if (integralAmountRep >= integralTotalAmount) {
      return priceAt(pos);
    }
  }
  return std::nullopt;
}

MarketOrderBook::AmountPerPriceVec MarketOrderBook::computePricesAtWhichAmountWouldBeBoughtImmediately(
    MonetaryAmount ma) const {
  if (ma.currencyCode() != _market.base()) {
    throw exception("Given amount should be in the base currency of this market");
  }
  AmountType integralAmountRep = 0;
  const std::optional<AmountType> integralTotalAmountOpt = ma.amount(_volAndPriNbDecimals.volNbDecimals);
  AmountPerPriceVec ret;
  if (!integralTotalAmountOpt) {
    log::warn("Not enough amount to buy {} on market {}", ma, _market);
    return ret;
  }
  const AmountType integralTotalAmount = *integralTotalAmountOpt;

  const int nbOrders = _orders.size();

  for (int pos = _highestBidPricePos + 1; pos < nbOrders; ++pos) {
    MonetaryAmount price(_orders[pos].price, _market.quote(), _volAndPriNbDecimals.priNbDecimals);
    if (_orders[pos].amount != 0) {
      if (integralTotalAmount - integralAmountRep <= -_orders[pos].amount) {
        ret.emplace_back(
            MonetaryAmount(integralTotalAmount - integralAmountRep, _market.base(), _volAndPriNbDecimals.volNbDecimals),
            price);
        return ret;
      }
      integralAmountRep -= _orders[pos].amount;  // -= because amount is < 0 here
      ret.emplace_back(negAmountAt(pos), price);
    }
  }
  log::warn("Not enough amount to buy {} on market {} ({} max)", ma, _market,
            MonetaryAmount(integralAmountRep, _market.base(), _volAndPriNbDecimals.volNbDecimals));
  ret.clear();
  return ret;
}

namespace {
inline std::optional<MonetaryAmount> ComputeAvgPrice(Market mk,
                                                     const MarketOrderBook::AmountPerPriceVec& amountsPerPrice) {
  if (amountsPerPrice.empty()) {
    return {};
  }
  if (amountsPerPrice.size() == 1) {
    return amountsPerPrice.front().price;
  }
  MonetaryAmount ret(0, mk.quote());
  MonetaryAmount totalAmount(0, mk.base());
  for (const MarketOrderBook::AmountAtPrice& amountAtPrice : amountsPerPrice) {
    ret += amountAtPrice.amount.toNeutral() * amountAtPrice.price;
    totalAmount += amountAtPrice.amount;
  }
  return ret / totalAmount.toNeutral();
}
}  // namespace

std::optional<MonetaryAmount> MarketOrderBook::computeAvgPriceAtWhichAmountWouldBeBoughtImmediately(
    MonetaryAmount ma) const {
  return ComputeAvgPrice(_market, computePricesAtWhichAmountWouldBeBoughtImmediately(ma));
}

std::optional<MonetaryAmount> MarketOrderBook::computeMinPriceAtWhichAmountWouldBeSoldImmediately(
    MonetaryAmount ma) const {
  if (ma.currencyCode() != _market.base()) {
    throw exception("Given amount should be in the base currency of this market");
  }
  AmountType integralAmountRep = 0;
  const std::optional<AmountType> integralTotalAmountOpt = ma.amount(_volAndPriNbDecimals.volNbDecimals);
  if (!integralTotalAmountOpt) {
    return std::nullopt;
  }
  const AmountType integralTotalAmount = *integralTotalAmountOpt;

  for (int pos = _lowestAskPricePos - 1; pos >= 0 && integralAmountRep <= integralTotalAmount; --pos) {
    integralAmountRep += _orders[pos].amount;
    if (integralAmountRep >= integralTotalAmount) {
      return priceAt(pos);
    }
  }
  return std::nullopt;
}

MarketOrderBook::AmountPerPriceVec MarketOrderBook::computePricesAtWhichAmountWouldBeSoldImmediately(
    MonetaryAmount ma) const {
  if (ma.currencyCode() != _market.base()) {
    throw exception("Given amount should be in the base currency of this market");
  }
  AmountType integralAmountRep = 0;
  const std::optional<AmountType> integralTotalAmountOpt = ma.amount(_volAndPriNbDecimals.volNbDecimals);
  AmountPerPriceVec ret;
  if (!integralTotalAmountOpt) {
    log::debug("Not enough amount to sell {} on market {}", ma, _market);
    return ret;
  }
  const AmountType integralTotalAmount = *integralTotalAmountOpt;

  for (int pos = _lowestAskPricePos - 1; pos >= 0; --pos) {
    MonetaryAmount price = priceAt(pos);
    assert(_orders[pos].amount != 0);
    if (integralTotalAmount - integralAmountRep <= _orders[pos].amount) {
      ret.emplace_back(
          MonetaryAmount(integralTotalAmount - integralAmountRep, _market.base(), _volAndPriNbDecimals.volNbDecimals),
          price);
      return ret;
    }
    integralAmountRep += _orders[pos].amount;
    ret.emplace_back(amountAt(pos), price);
  }
  log::debug("Not enough amount to sell {} on market {}", ma, _market);
  ret.clear();
  return ret;
}

std::optional<MonetaryAmount> MarketOrderBook::computeAvgPriceAtWhichAmountWouldBeSoldImmediately(
    MonetaryAmount ma) const {
  return ComputeAvgPrice(_market, computePricesAtWhichAmountWouldBeSoldImmediately(ma));
}

std::optional<MonetaryAmount> MarketOrderBook::computeAvgPriceForTakerAmount(MonetaryAmount amountInBaseOrQuote) const {
  if (amountInBaseOrQuote.currencyCode() == _market.base()) {
    return computeAvgPriceAtWhichAmountWouldBeSoldImmediately(amountInBaseOrQuote);
  }
  MonetaryAmount avgPrice(0, _market.quote());
  MonetaryAmount remQuoteAmount = amountInBaseOrQuote;
  const int nbOrders = _orders.size();
  for (int pos = _highestBidPricePos + 1; pos < nbOrders; ++pos) {
    const MonetaryAmount amount = negAmountAt(pos);
    const MonetaryAmount price = priceAt(pos);
    const MonetaryAmount maxAmountToTakeFromThisLine = amount.toNeutral() * price;

    if (maxAmountToTakeFromThisLine < remQuoteAmount) {
      // We can eat all from this line, take the max and continue
      avgPrice += maxAmountToTakeFromThisLine.toNeutral() * price;
      remQuoteAmount -= maxAmountToTakeFromThisLine;
    } else {
      // We can finish here
      avgPrice += remQuoteAmount.toNeutral() * price;
      return avgPrice / amountInBaseOrQuote.toNeutral();
    }
  }
  return {};
}

std::optional<MonetaryAmount> MarketOrderBook::computeWorstPriceForTakerAmount(
    MonetaryAmount amountInBaseOrQuote) const {
  if (amountInBaseOrQuote.currencyCode() == _market.base()) {
    return computeMinPriceAtWhichAmountWouldBeSoldImmediately(amountInBaseOrQuote);
  }
  MonetaryAmount remQuoteAmount = amountInBaseOrQuote;
  const int nbOrders = _orders.size();
  for (int pos = _highestBidPricePos + 1; pos < nbOrders; ++pos) {
    const MonetaryAmount amount = negAmountAt(pos);
    const MonetaryAmount price = priceAt(pos);
    const MonetaryAmount maxAmountToTakeFromThisLine = amount.toNeutral() * price;

    if (maxAmountToTakeFromThisLine < remQuoteAmount) {
      // We can eat all from this line, take the max and continue
      remQuoteAmount -= maxAmountToTakeFromThisLine;
    } else {
      // We can finish here
      return price;
    }
  }
  return {};
}

std::optional<MonetaryAmount> MarketOrderBook::convert(MonetaryAmount amountInBaseOrQuote,
                                                       const PriceOptions& priceOptions) const {
  std::optional<MonetaryAmount> avgPrice = computeAvgPrice(amountInBaseOrQuote, priceOptions);
  if (!avgPrice) {
    return std::nullopt;
  }
  return amountInBaseOrQuote.currencyCode() == _market.base()
             ? amountInBaseOrQuote.toNeutral() * (*avgPrice)
             : MonetaryAmount(amountInBaseOrQuote / *avgPrice, _market.base());
}

// NOLINTNEXTLINE(misc-no-recursion)
std::pair<MonetaryAmount, MonetaryAmount> MarketOrderBook::operator[](int relativePosToLimitPrice) const {
  if (relativePosToLimitPrice == 0) {
    const auto [v11, v12] = (*this)[-1];
    const auto [v21, v22] = (*this)[1];
    return std::make_pair(v11 + (v21 - v11) / 2, v12 + (v22 - v12) / 2);
  }
  if (relativePosToLimitPrice < 0) {
    const int pos = _lowestAskPricePos + relativePosToLimitPrice;
    return std::make_pair(priceAt(pos), amountAt(pos));
  }
  // > 0
  const int pos = _highestBidPricePos + relativePosToLimitPrice;
  return std::make_pair(priceAt(pos), negAmountAt(pos));
}

std::optional<MonetaryAmount> MarketOrderBook::convertBaseAmountToQuote(MonetaryAmount amountInBaseCurrency) const {
  if (amountInBaseCurrency.currencyCode() != _market.base()) {
    throw exception("Given amount should be in the base currency of this market");
  }
  MonetaryAmount quoteAmount(0, _market.quote());
  for (int pos = _lowestAskPricePos - 1; pos >= 0; --pos) {
    const MonetaryAmount amount = amountAt(pos);
    const MonetaryAmount price = priceAt(pos);

    if (amount < amountInBaseCurrency) {
      // We can eat all from this line, take the max and continue
      quoteAmount += amount.toNeutral() * price;
      amountInBaseCurrency -= amount;
    } else {
      // We can finish here
      return quoteAmount + amountInBaseCurrency.toNeutral() * price;
    }
  }
  return {};
}

std::optional<MonetaryAmount> MarketOrderBook::convertQuoteAmountToBase(MonetaryAmount amountInQuoteCurrency) const {
  if (amountInQuoteCurrency.currencyCode() != _market.quote()) {
    throw exception("Given amount should be in the quote currency of this market");
  }
  MonetaryAmount baseAmount(0, _market.base());
  const int nbOrders = _orders.size();
  for (int pos = _highestBidPricePos + 1; pos < nbOrders; ++pos) {
    const MonetaryAmount amount = negAmountAt(pos);
    const MonetaryAmount price = priceAt(pos);
    const MonetaryAmount maxAmountToTakeFromThisLine = price * amount.toNeutral();

    if (maxAmountToTakeFromThisLine < amountInQuoteCurrency) {
      // We can eat all from this line, take the max and continue
      baseAmount += amount;
      amountInQuoteCurrency -= maxAmountToTakeFromThisLine;
    } else {
      // We can finish here
      return baseAmount + MonetaryAmount(1, _market.base()) * (amountInQuoteCurrency / price);
    }
  }
  return {};
}

MonetaryAmount MarketOrderBook::getHighestTheoreticalPrice() const {
  if (_orders.empty()) {
    // We have no hint about what could be the highest price, return an arbitrary maximum price
    return MonetaryAmount(std::numeric_limits<AmountType>::max(), _market.quote());
  }
  // Should be sufficiently large, without exceeding authorized prices for exchanges
  return priceAt(_orders.size() - 1) * 100;
}

MonetaryAmount MarketOrderBook::getLowestTheoreticalPrice() const {
  // Use 10 as default number of decimals if empty orderbook (like Satoshi)
  return {1, _market.quote(),
          _orders.empty() ? static_cast<int8_t>(10) : (lowestAskPrice() - highestBidPrice()).nbDecimals()};
}

namespace {
std::optional<MonetaryAmount> ComputeRelativePrice(bool isBuy, const MarketOrderBook& marketOrderBook,
                                                   int relativePrice) {
  assert(relativePrice != 0);
  if (isBuy) {
    // Bounds check
    if (relativePrice > 0) {
      relativePrice = std::min(relativePrice, marketOrderBook.nbBidPrices());
    } else {
      relativePrice = std::max(relativePrice, -marketOrderBook.nbAskPrices());
    }
    relativePrice = -relativePrice;
  } else {
    if (relativePrice > 0) {
      relativePrice = std::min(relativePrice, marketOrderBook.nbAskPrices());
    } else {
      relativePrice = std::max(relativePrice, -marketOrderBook.nbBidPrices());
    }
  }
  if (relativePrice == 0) {
    return std::nullopt;
  }
  return marketOrderBook[relativePrice].first;
}
}  // namespace

std::optional<MonetaryAmount> MarketOrderBook::computeLimitPrice(CurrencyCode fromCurrencyCode,
                                                                 const PriceOptions& priceOptions) const {
  if (empty()) {
    return std::nullopt;
  }
  if (priceOptions.isRelativePrice()) {
    const bool isBuy = fromCurrencyCode == _market.quote();
    return ComputeRelativePrice(isBuy, *this, priceOptions.relativePrice());
  }
  CurrencyCode marketCode = _market.base();
  switch (priceOptions.priceStrategy()) {
    case PriceStrategy::kTaker:
      [[fallthrough]];
    case PriceStrategy::kNibble:
      marketCode = _market.quote();
      [[fallthrough]];
    case PriceStrategy::kMaker:
      return fromCurrencyCode == marketCode ? lowestAskPrice() : highestBidPrice();
    default:
      unreachable();
  }
}

std::optional<MonetaryAmount> MarketOrderBook::computeAvgPrice(MonetaryAmount from,
                                                               const PriceOptions& priceOptions) const {
  if (empty()) {
    return std::nullopt;
  }
  if (priceOptions.isFixedPrice()) {
    return MonetaryAmount(priceOptions.fixedPrice(), _market.quote());
  }
  if (priceOptions.isRelativePrice()) {
    const bool isBuy = from.currencyCode() == _market.quote();
    return ComputeRelativePrice(isBuy, *this, priceOptions.relativePrice());
  }
  CurrencyCode marketCode = _market.base();
  switch (priceOptions.priceStrategy()) {
    case PriceStrategy::kTaker: {
      std::optional<MonetaryAmount> optRet = computeAvgPriceForTakerAmount(from);
      if (optRet) {
        return optRet;
      }
      log::warn("{} is too big to be matched immediately on {}, return limit price instead", from, _market);
      [[fallthrough]];
    }
    case PriceStrategy::kNibble:
      marketCode = _market.quote();
      [[fallthrough]];
    case PriceStrategy::kMaker:
      return from.currencyCode() == marketCode ? lowestAskPrice() : highestBidPrice();
    default:
      unreachable();
  }
}

SimpleTable MarketOrderBook::getTable(std::string_view exchangeName,
                                      std::optional<MonetaryAmount> conversionPriceRate) const {
  string h1("Sellers of ");
  string baseStr = _market.base().str();
  h1.append(baseStr).append(" (asks)");
  string h2(exchangeName);
  h2.append(" ").append(baseStr).append(" price in ");
  _market.quote().appendStrTo(h2);
  string h3(exchangeName);
  if (conversionPriceRate) {
    h3.append(" ").append(baseStr).append(" price in ").append(conversionPriceRate->currencyStr());
  }
  string h4("Buyers of ");
  h4.append(baseStr).append(" (bids)");

  SimpleTable table;
  if (conversionPriceRate) {
    table.emplace_back(std::move(h1), std::move(h2), std::move(h3), std::move(h4));
  } else {
    table.emplace_back(std::move(h1), std::move(h2), std::move(h4));
  }

  for (int op = _orders.size(); op > 0; --op) {
    const int pos = op - 1;
    MonetaryAmount amount(std::abs(_orders[pos].amount), CurrencyCode(), _volAndPriNbDecimals.volNbDecimals);
    MonetaryAmount price = priceAt(pos);
    SimpleTable::Row row(amount.str());
    row.emplace_back(price.amountStr());
    if (conversionPriceRate) {
      MonetaryAmount convertedPrice = price.toNeutral() * conversionPriceRate->toNeutral();

      row.emplace_back(convertedPrice.str());
    }
    row.emplace_back("");
    if (_orders[pos].amount > 0) {
      row.front().swap(row.back());
    }
    table.push_back(std::move(row));
  }
  return table;
}

}  // namespace cct
