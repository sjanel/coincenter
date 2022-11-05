#include "marketorderbook.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>

#include "cct_exception.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_log.hpp"
#include "priceoptions.hpp"
#include "stringhelpers.hpp"
#include "unreachable.hpp"

namespace cct {

MarketOrderBook::MarketOrderBook(Market market, OrderBookLineSpan orderLines, VolAndPriNbDecimals volAndPriNbDecimals)
    : _market(market), _volAndPriNbDecimals(volAndPriNbDecimals) {
  const int nbPrices = static_cast<int>(orderLines.size());
  _orders.reserve(nbPrices);
  if (_volAndPriNbDecimals == VolAndPriNbDecimals()) {
    for (const OrderBookLine& l : orderLines) {
      _volAndPriNbDecimals.volNbDecimals =
          std::min(_volAndPriNbDecimals.volNbDecimals, l._amount.currentMaxNbDecimals());
      _volAndPriNbDecimals.priNbDecimals =
          std::min(_volAndPriNbDecimals.priNbDecimals, l._price.currentMaxNbDecimals());
    }
  }
  if (nbPrices == 0) {
    _lowestAskPricePos = 0;
    _highestBidPricePos = 0;
  } else {
    for (const OrderBookLine& l : orderLines) {
      assert(l._amount.currencyCode() == market.base() && l._price.currencyCode() == market.quote());
      // amounts cannot be nullopt here
      if (l._amount == 0) {
        // Just ignore empty lines
        continue;
      }
      AmountPrice::AmountType amountIntegral = *l._amount.amount(_volAndPriNbDecimals.volNbDecimals);
      AmountPrice::AmountType priceIntegral = *l._price.amount(_volAndPriNbDecimals.priNbDecimals);

      _orders.emplace_back(amountIntegral, priceIntegral);
    }

    std::ranges::sort(_orders, [](AmountPrice lhs, AmountPrice rhs) { return lhs.price < rhs.price; });
    auto adjacentFindIt =
        std::ranges::adjacent_find(_orders, [](AmountPrice lhs, AmountPrice rhs) { return lhs.price == rhs.price; });
    if (adjacentFindIt != _orders.end()) {
      throw exception("Forbidden duplicate price {} in the order book for market {}", adjacentFindIt->price, market);
    }

    auto highestBidPriceIt = std::ranges::partition_point(_orders, [](AmountPrice a) { return a.amount > 0; });
    _highestBidPricePos = static_cast<int>(highestBidPriceIt - _orders.begin() - 1);
    _lowestAskPricePos = static_cast<int>(
        std::find_if(highestBidPriceIt, _orders.end(), [](AmountPrice a) { return a.amount < 0; }) - _orders.begin());
  }
}

MarketOrderBook::MarketOrderBook(MonetaryAmount askPrice, MonetaryAmount askVolume, MonetaryAmount bidPrice,
                                 MonetaryAmount bidVolume, VolAndPriNbDecimals volAndPriNbDecimals, int depth)
    : _market(askVolume.currencyCode(), askPrice.currencyCode()),
      _volAndPriNbDecimals(volAndPriNbDecimals),
      _isArtificiallyExtended(depth > 1) {
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
  for (int d = 0; d < depth; ++d) {
    AmountPrice amountPrice = d == 0 ? refBidAmountPrice : _orders[depth - d];
    amountPrice.price -= stepPrice * d;
    if (d != 0 && amountPrice.amount < kMaxVol) {
      amountPrice.amount += simulatedStepVol / 2;
    }
    _orders[depth - d - 1] = std::move(amountPrice);
  }
  _highestBidPricePos = depth - 1;
  _lowestAskPricePos = _highestBidPricePos + 1;

  // Finally add ask lines
  for (int d = 0; d < depth; ++d) {
    AmountPrice amountPrice = d == 0 ? refAskAmountPrice : _orders[depth + d - 1];
    amountPrice.price += stepPrice * d;
    if (d != 0 && -amountPrice.amount < kMaxVol) {
      amountPrice.amount -= simulatedStepVol / 2;
    }
    _orders[depth + d] = std::move(amountPrice);
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

MonetaryAmount MarketOrderBook::computeCumulAmountBoughtImmediatelyAt(MonetaryAmount p) const {
  AmountType integralAmountRep = 0;
  const int nbOrders = _orders.size();
  for (int pos = _lowestAskPricePos; pos < nbOrders && priceAt(pos) <= p; ++pos) {
    integralAmountRep -= _orders[pos].amount;  // Minus as amounts are negative here
  }
  return MonetaryAmount(integralAmountRep, _market.base(), _volAndPriNbDecimals.volNbDecimals);
}

MonetaryAmount MarketOrderBook::computeCumulAmountSoldImmediatelyAt(MonetaryAmount p) const {
  AmountType integralAmountRep = 0;
  for (int pos = _highestBidPricePos; pos >= 0 && priceAt(pos) >= p; --pos) {
    integralAmountRep += _orders[pos].amount;
  }
  return MonetaryAmount(integralAmountRep, _market.base(), _volAndPriNbDecimals.volNbDecimals);
}

std::optional<MonetaryAmount> MarketOrderBook::computeMaxPriceAtWhichAmountWouldBeBoughtImmediately(
    MonetaryAmount a) const {
  if (a.currencyCode() != _market.base()) {
    throw exception("Given amount should be in the base currency of this market");
  }
  AmountType integralAmountRep = 0;
  const std::optional<AmountType> integralTotalAmountToBuyOpt = a.amount(_volAndPriNbDecimals.volNbDecimals);
  if (!integralTotalAmountToBuyOpt) {
    return std::nullopt;
  }
  const AmountType integralTotalAmountToBuy = *integralTotalAmountToBuyOpt;
  const int nbOrders = _orders.size();
  for (int pos = _highestBidPricePos + 1; pos < nbOrders && integralAmountRep <= integralTotalAmountToBuy; ++pos) {
    integralAmountRep -= _orders[pos].amount;  // -= because amount is < 0 here
    if (integralAmountRep >= integralTotalAmountToBuy) {
      return priceAt(pos);
    }
  }
  return std::nullopt;
}

MarketOrderBook::AmountPerPriceVec MarketOrderBook::computePricesAtWhichAmountWouldBeBoughtImmediately(
    MonetaryAmount a) const {
  if (a.currencyCode() != _market.base()) {
    throw exception("Given amount should be in the base currency of this market");
  }
  AmountType integralAmountRep = 0;
  const std::optional<AmountType> integralTotalAmountToBuyOpt = a.amount(_volAndPriNbDecimals.volNbDecimals);
  AmountPerPriceVec ret;
  if (!integralTotalAmountToBuyOpt) {
    log::warn("Not enough amount to buy {} on market {}", a, _market);
    return ret;
  }
  const AmountType integralTotalAmountToBuy = *integralTotalAmountToBuyOpt;

  const int nbOrders = _orders.size();

  for (int pos = _highestBidPricePos + 1; pos < nbOrders; ++pos) {
    MonetaryAmount price(_orders[pos].price, _market.quote(), _volAndPriNbDecimals.priNbDecimals);
    if (_orders[pos].amount != 0) {
      if (integralTotalAmountToBuy - integralAmountRep <= -_orders[pos].amount) {
        ret.emplace_back(MonetaryAmount(integralTotalAmountToBuy - integralAmountRep, _market.base(),
                                        _volAndPriNbDecimals.volNbDecimals),
                         price);
        return ret;
      }
      integralAmountRep -= _orders[pos].amount;  // -= because amount is < 0 here
      ret.emplace_back(negAmountAt(pos), price);
    }
  }
  log::warn("Not enough amount to buy {} on market {} ({} max)", a, _market,
            MonetaryAmount(integralAmountRep, _market.base(), _volAndPriNbDecimals.volNbDecimals));
  ret.clear();
  return ret;
}

namespace {
inline std::optional<MonetaryAmount> ComputeAvgPrice(Market m,
                                                     const MarketOrderBook::AmountPerPriceVec& amountsPerPrice) {
  if (amountsPerPrice.empty()) {
    return std::optional<MonetaryAmount>();
  }
  if (amountsPerPrice.size() == 1) {
    return amountsPerPrice.front().price;
  }
  MonetaryAmount ret(0, m.quote());
  MonetaryAmount totalAmount(0, m.base());
  for (const MarketOrderBook::AmountAtPrice& amountAtPrice : amountsPerPrice) {
    ret += amountAtPrice.amount.toNeutral() * amountAtPrice.price;
    totalAmount += amountAtPrice.amount;
  }
  ret /= totalAmount.toNeutral();
  return ret;
}
}  // namespace

std::optional<MonetaryAmount> MarketOrderBook::computeAvgPriceAtWhichAmountWouldBeBoughtImmediately(
    MonetaryAmount a) const {
  return ComputeAvgPrice(_market, computePricesAtWhichAmountWouldBeBoughtImmediately(a));
}

std::optional<MonetaryAmount> MarketOrderBook::computeMinPriceAtWhichAmountWouldBeSoldImmediately(
    MonetaryAmount a) const {
  if (a.currencyCode() != _market.base()) {
    throw exception("Given amount should be in the base currency of this market");
  }
  AmountType integralAmountRep = 0;
  const std::optional<AmountType> integralTotalAmountToBuyOpt = a.amount(_volAndPriNbDecimals.volNbDecimals);
  AmountPerPriceVec ret;
  if (!integralTotalAmountToBuyOpt) {
    return std::nullopt;
  }
  const AmountType integralTotalAmountToBuy = *integralTotalAmountToBuyOpt;

  for (int pos = _lowestAskPricePos - 1; pos >= 0 && integralAmountRep <= integralTotalAmountToBuy; --pos) {
    integralAmountRep += _orders[pos].amount;
    if (integralAmountRep >= integralTotalAmountToBuy) {
      return priceAt(pos);
    }
  }
  return std::nullopt;
}

MarketOrderBook::AmountPerPriceVec MarketOrderBook::computePricesAtWhichAmountWouldBeSoldImmediately(
    MonetaryAmount a) const {
  if (a.currencyCode() != _market.base()) {
    throw exception("Given amount should be in the base currency of this market");
  }
  AmountType integralAmountRep = 0;
  const std::optional<AmountType> integralTotalAmountToBuyOpt = a.amount(_volAndPriNbDecimals.volNbDecimals);
  AmountPerPriceVec ret;
  if (!integralTotalAmountToBuyOpt) {
    log::debug("Not enough amount to sell {} on market {}", a, _market);
    return ret;
  }
  const AmountType integralTotalAmountToBuy = *integralTotalAmountToBuyOpt;

  for (int pos = _lowestAskPricePos - 1; pos >= 0; --pos) {
    MonetaryAmount price = priceAt(pos);
    assert(_orders[pos].amount != 0);
    if (integralTotalAmountToBuy - integralAmountRep <= _orders[pos].amount) {
      ret.emplace_back(MonetaryAmount(integralTotalAmountToBuy - integralAmountRep, _market.base(),
                                      _volAndPriNbDecimals.volNbDecimals),
                       price);
      return ret;
    }
    integralAmountRep += _orders[pos].amount;
    ret.emplace_back(amountAt(pos), price);
  }
  log::debug("Not enough amount to sell {} on market {}", a, _market);
  ret.clear();
  return ret;
}

std::optional<MonetaryAmount> MarketOrderBook::computeAvgPriceAtWhichAmountWouldBeSoldImmediately(
    MonetaryAmount a) const {
  return ComputeAvgPrice(_market, computePricesAtWhichAmountWouldBeSoldImmediately(a));
}

std::optional<MonetaryAmount> MarketOrderBook::computeAvgPriceForTakerAmount(MonetaryAmount amountInBaseOrQuote) const {
  if (amountInBaseOrQuote.currencyCode() == _market.base()) {
    return computeAvgPriceAtWhichAmountWouldBeSoldImmediately(amountInBaseOrQuote);
  }
  MonetaryAmount avgPrice(0, _market.quote());
  MonetaryAmount remQuoteAmount = amountInBaseOrQuote;
  const int nbOrders = _orders.size();
  for (int pos = _highestBidPricePos + 1; pos < nbOrders; ++pos) {
    MonetaryAmount amount = negAmountAt(pos);
    MonetaryAmount price = priceAt(pos);
    MonetaryAmount maxAmountToTakeFromThisLine = amount.toNeutral() * price;
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
  return std::optional<MonetaryAmount>();
}

std::optional<MonetaryAmount> MarketOrderBook::computeWorstPriceForTakerAmount(
    MonetaryAmount amountInBaseOrQuote) const {
  if (amountInBaseOrQuote.currencyCode() == _market.base()) {
    return computeMinPriceAtWhichAmountWouldBeSoldImmediately(amountInBaseOrQuote);
  }
  MonetaryAmount remQuoteAmount = amountInBaseOrQuote;
  const int nbOrders = _orders.size();
  for (int pos = _highestBidPricePos + 1; pos < nbOrders; ++pos) {
    MonetaryAmount amount = negAmountAt(pos);
    MonetaryAmount price = priceAt(pos);
    MonetaryAmount maxAmountToTakeFromThisLine = amount.toNeutral() * price;
    if (maxAmountToTakeFromThisLine < remQuoteAmount) {
      // We can eat all from this line, take the max and continue
      remQuoteAmount -= maxAmountToTakeFromThisLine;
    } else {
      // We can finish here
      return price;
    }
  }
  return std::optional<MonetaryAmount>();
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

std::pair<MonetaryAmount, MonetaryAmount> MarketOrderBook::operator[](int relativePosToLimitPrice) const {
  if (relativePosToLimitPrice == 0) {
    auto [v11, v12] = (*this)[-1];
    auto [v21, v22] = (*this)[1];
    return std::make_pair(v11 + (v21 - v11) / 2, v12 + (v22 - v12) / 2);
  }
  if (relativePosToLimitPrice < 0) {
    int pos = _lowestAskPricePos + relativePosToLimitPrice;
    return std::make_pair(priceAt(pos), amountAt(pos));
  }
  // > 0
  int pos = _highestBidPricePos + relativePosToLimitPrice;
  return std::make_pair(priceAt(pos), negAmountAt(pos));
}

std::optional<MonetaryAmount> MarketOrderBook::convertBaseAmountToQuote(MonetaryAmount amountInBaseCurrency) const {
  if (amountInBaseCurrency.currencyCode() != _market.base()) {
    throw exception("Given amount should be in the base currency of this market");
  }
  MonetaryAmount quoteAmount(0, _market.quote());
  for (int pos = _lowestAskPricePos - 1; pos >= 0; --pos) {
    MonetaryAmount amount = amountAt(pos);
    MonetaryAmount price = priceAt(pos);
    if (amount < amountInBaseCurrency) {
      // We can eat all from this line, take the max and continue
      quoteAmount += amount.toNeutral() * price;
      amountInBaseCurrency -= amount;
    } else {
      // We can finish here
      return quoteAmount + amountInBaseCurrency.toNeutral() * price;
    }
  }
  return std::optional<MonetaryAmount>();
}

std::optional<MonetaryAmount> MarketOrderBook::convertQuoteAmountToBase(MonetaryAmount amountInQuoteCurrency) const {
  if (amountInQuoteCurrency.currencyCode() != _market.quote()) {
    throw exception("Given amount should be in the quote currency of this market");
  }
  MonetaryAmount baseAmount(0, _market.base());
  const int nbOrders = _orders.size();
  for (int pos = _highestBidPricePos + 1; pos < nbOrders; ++pos) {
    MonetaryAmount amount = negAmountAt(pos);
    MonetaryAmount price = priceAt(pos);
    MonetaryAmount maxAmountToTakeFromThisLine = price * amount.toNeutral();
    if (maxAmountToTakeFromThisLine < amountInQuoteCurrency) {
      // We can eat all from this line, take the max and continue
      baseAmount += amount;
      amountInQuoteCurrency -= maxAmountToTakeFromThisLine;
    } else {
      // We can finish here
      return baseAmount + MonetaryAmount(1, _market.base()) * (amountInQuoteCurrency / price);
    }
  }
  return std::optional<MonetaryAmount>();
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
  return MonetaryAmount(1, _market.quote(), _orders.empty() ? 10 : (lowestAskPrice() - highestBidPrice()).nbDecimals());
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
  _market.quote().appendStr(h2);
  string h3(exchangeName);
  if (conversionPriceRate) {
    h3.append(" ").append(baseStr).append(" price in ").append(conversionPriceRate->currencyStr());
  }
  string h4("Buyers of ");
  h4.append(baseStr).append(" (bids)");

  SimpleTable t;
  if (conversionPriceRate) {
    t.emplace_back(std::move(h1), std::move(h2), std::move(h3), std::move(h4));
  } else {
    t.emplace_back(std::move(h1), std::move(h2), std::move(h4));
  }

  for (int op = _orders.size(); op > 0; --op) {
    const int pos = op - 1;
    MonetaryAmount amount(std::abs(_orders[pos].amount), CurrencyCode(), _volAndPriNbDecimals.volNbDecimals);
    MonetaryAmount price = priceAt(pos);
    SimpleTable::Row r(amount.str());
    r.emplace_back(price.amountStr());
    if (conversionPriceRate) {
      MonetaryAmount convertedPrice = price.toNeutral() * conversionPriceRate->toNeutral();

      r.emplace_back(convertedPrice.str());
    }
    r.emplace_back("");
    if (_orders[pos].amount > 0) {
      r.front().swap(r.back());
    }
    t.push_back(std::move(r));
  }
  return t;
}

}  // namespace cct
