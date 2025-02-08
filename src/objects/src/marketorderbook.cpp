#include "marketorderbook.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <numeric>
#include <optional>
#include <string_view>
#include <utility>

#include "amount-price.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "exchange-name-enum.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "order-book-line.hpp"
#include "overflow-check.hpp"
#include "priceoptions.hpp"
#include "priceoptionsdef.hpp"
#include "simpletable.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"
#include "unreachable.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct {

MarketOrderBook::MarketOrderBook(TimePoint timeStamp, Market market, const MarketOrderBookLines& orderLines,
                                 VolAndPriNbDecimals volAndPriNbDecimals)
    : _time(timeStamp), _market(market), _volAndPriNbDecimals(volAndPriNbDecimals) {
  const auto nbPrices = orderLines.size();
  if (nbPrices == 0) {
    log::error("Creating an empty order book - it is invalid");
    return;
  }
  if (_volAndPriNbDecimals == VolAndPriNbDecimals()) {
    for (const OrderBookLine& orderBookLine : orderLines) {
      _volAndPriNbDecimals.volNbDecimals =
          std::min(_volAndPriNbDecimals.volNbDecimals, orderBookLine.amount().currentMaxNbDecimals());
      _volAndPriNbDecimals.priNbDecimals =
          std::min(_volAndPriNbDecimals.priNbDecimals, orderBookLine.price().currentMaxNbDecimals());
    }
  }

  _orders.reserve(nbPrices);

  for (const OrderBookLine& orderBookLine : orderLines) {
    if (orderBookLine.amount().currencyCode() != market.base() ||
        orderBookLine.price().currencyCode() != market.quote()) {
      throw exception("Invalid market order book currencies");
    }

    const auto optAmountInt = orderBookLine.amount().amount(_volAndPriNbDecimals.volNbDecimals);
    if (!optAmountInt) {
      throw exception("Unable to retrieve amount");
    }
    const auto optPriceInt = orderBookLine.price().amount(_volAndPriNbDecimals.priNbDecimals);
    if (!optPriceInt) {
      throw exception("Unable to retrieve price");
    }

    // It's not expected at this point to not have a value for asked number of decimals
    _orders.emplace_back(*optAmountInt, *optPriceInt);
  }

  std::ranges::sort(_orders, [](auto lhs, auto rhs) { return lhs.price < rhs.price; });

  auto it = _orders.begin();
  while ((it = std::adjacent_find(it, _orders.end(), [](auto lhs, auto rhs) { return lhs.price == rhs.price; })) !=
         _orders.end()) {
    auto nextIt = std::next(it);
    log::warn("Forbidden duplicate price {} at amounts {} & {} in the order book for market {}, merging them",
              it->price, it->amount, nextIt->amount, market);
    if (WillSumOverflow(nextIt->amount, it->amount)) {
      nextIt->amount = std::midpoint(nextIt->amount, it->amount);
    } else {
      nextIt->amount += it->amount;
    }
    // Remove the first duplicated price line (we summed the amounts on the next line)
    if (nextIt->amount == 0) {
      // If the sum has 0 amount, remove the next one as well
      _orders.erase(it, std::next(nextIt));
    } else {
      _orders.erase(it);
    }
  }

  const auto positiveAmounts = [](auto amountPrice) { return amountPrice.amount > 0; };
  if (!std::ranges::is_partitioned(_orders, positiveAmounts)) {
    throw exception("Invalid market order book - check input data");
  }

  const auto highestBidPriceIt = std::ranges::partition_point(_orders, positiveAmounts);

  using PricePosT = decltype(_highestBidPricePos);

  _highestBidPricePos = static_cast<PricePosT>(highestBidPriceIt - _orders.begin() - 1);
  _lowestAskPricePos = static_cast<PricePosT>(
      std::find_if(highestBidPriceIt, _orders.end(), [](auto amountPrice) { return amountPrice.amount < 0; }) -
      _orders.begin());
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

  askVolume.round(_volAndPriNbDecimals.volNbDecimals, MonetaryAmount::RoundType::kNearest);
  bidVolume.round(_volAndPriNbDecimals.volNbDecimals, MonetaryAmount::RoundType::kNearest);

  if (askVolume == 0) {
    throw exception("Number of decimals {} is too small for given start volume {}", _volAndPriNbDecimals.volNbDecimals,
                    askVolume);
  }
  if (bidVolume == 0) {
    throw exception("Number of decimals {} is too small for given start volume {}", _volAndPriNbDecimals.volNbDecimals,
                    bidVolume);
  }

  if (askPrice <= bidPrice) {
    throw exception(
        "Invalid ask price {} and bid price {} for MarketOrderbook creation. Ask price should be larger than Bid "
        "price",
        askPrice, bidPrice);
  }

  const auto inputAskPrice = askPrice;
  const auto inputBidPrice = bidPrice;

  askPrice.round(_volAndPriNbDecimals.priNbDecimals, MonetaryAmount::RoundType::kNearest);
  bidPrice.round(_volAndPriNbDecimals.priNbDecimals, MonetaryAmount::RoundType::kNearest);

  if (askPrice == bidPrice) {
    // This is rare but can happen with inaccurate number of decimals / prices
    // This is almost a hack but necessary to fix some incorrect data sometimes sent from Kucoin ticker.
    // Let's fix it manually for at most one decimal difference (if it's more, data is really incorrect, better to
    // throw)
    const auto diffNbDecimalsAsk = inputAskPrice.nbDecimals() - _volAndPriNbDecimals.priNbDecimals;
    const auto diffNbDecimalsBid = inputBidPrice.nbDecimals() - _volAndPriNbDecimals.priNbDecimals;
    if (diffNbDecimalsAsk == 1) {
      _volAndPriNbDecimals.priNbDecimals = inputAskPrice.nbDecimals();
      askPrice = inputAskPrice;
    } else if (diffNbDecimalsBid == 1) {
      _volAndPriNbDecimals.priNbDecimals = inputBidPrice.nbDecimals();
      bidPrice = inputBidPrice;
    } else {
      // do nothing, will throw just below
    }
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
  const auto stepPrice = *optStepPrice;

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

  const AmountPriceInt refBidAmountPrice(*optBidVol, *optBidPri);
  const AmountPriceInt refAskAmountPrice(-(*optAskVol), *optAskPri);
  const auto simulatedStepVol = std::midpoint(refBidAmountPrice.amount, -refAskAmountPrice.amount);

  constexpr auto kMaxVol = std::numeric_limits<AmountPriceInt::AmountType>::max() / 2;

  _orders.resize(depth * 2);

  // Add bid lines first
  for (int currentDepth = 0; currentDepth < depth; ++currentDepth) {
    auto amountPrice = currentDepth == 0 ? refBidAmountPrice : _orders[depth - currentDepth];

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
    auto amountPrice = currentDepth == 0 ? refAskAmountPrice : _orders[depth + currentDepth - 1];

    amountPrice.price += stepPrice * currentDepth;
    if (currentDepth != 0 && -amountPrice.amount < kMaxVol) {
      amountPrice.amount -= simulatedStepVol / 2;
    }
    _orders[depth + currentDepth] = std::move(amountPrice);
  }
}

MarketOrderBook::MarketOrderBook(TimePoint timeStamp, Market market, AmountPriceVector&& orders,
                                 int32_t highestBidPricePos, int32_t lowestAskPricePos,
                                 VolAndPriNbDecimals volAndPriNbDecimals)
    : _time(timeStamp),
      _market(market),
      _orders(std::move(orders)),
      _highestBidPricePos(highestBidPricePos),
      _lowestAskPricePos(lowestAskPricePos),
      _volAndPriNbDecimals(volAndPriNbDecimals) {}

bool MarketOrderBook::isValid() const {
  if (_orders.size() < 2U) {
    log::error("Market order book is invalid as size is {}", _orders.size());
    return false;
  }
  if (!std::ranges::is_sorted(_orders, [](auto lhs, auto rhs) { return lhs.price < rhs.price; })) {
    log::error("Market order book is invalid because orders are not sorted by price");
    return false;
  }
  if (std::ranges::adjacent_find(_orders, [](auto lhs, auto rhs) { return lhs.price == rhs.price; }) != _orders.end()) {
    log::error("Market order book is invalid because of duplicate prices");
    return false;
  }
  if (!std::ranges::is_partitioned(_orders, [](auto amountPrice) { return amountPrice.amount > 0; })) {
    log::error("Market order book is invalid because lines are not partitioned by asks / bids");
    return false;
  }
  return true;
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

AmountPrice MarketOrderBook::avgPriceAndMatchedVolumeSell(MonetaryAmount baseAmount, MonetaryAmount price) const {
  MonetaryAmount avgPrice(0, _market.quote());

  MonetaryAmount remainingBaseAmount = baseAmount;
  for (int pos = _lowestAskPricePos - 1; pos >= 0; --pos) {
    const MonetaryAmount linePrice = priceAt(pos);
    if (linePrice < price) {
      break;
    }
    const MonetaryAmount lineAmount = amountAt(pos);
    const MonetaryAmount amountToEat = std::min(lineAmount, remainingBaseAmount);

    avgPrice += amountToEat.toNeutral() * linePrice;
    remainingBaseAmount -= amountToEat;
    if (remainingBaseAmount == 0) {
      break;
    }
  }
  MonetaryAmount matchedAmount = baseAmount - remainingBaseAmount;
  if (matchedAmount != 0) {
    avgPrice /= matchedAmount.toNeutral();
  }
  return {matchedAmount, avgPrice};
}

AmountPrice MarketOrderBook::avgPriceAndMatchedVolumeBuy(MonetaryAmount amountInBaseOrQuote,
                                                         MonetaryAmount price) const {
  MonetaryAmount remainingAmountInBaseOrQuote = amountInBaseOrQuote;
  MonetaryAmount matchedAmount(0, _market.base());
  MonetaryAmount avgPrice(0, _market.quote());
  const int nbOrders = _orders.size();
  for (int pos = _highestBidPricePos + 1; pos < nbOrders; ++pos) {
    const MonetaryAmount linePrice = priceAt(pos);
    if (linePrice > price) {
      break;
    }
    const MonetaryAmount lineAmount = negAmountAt(pos);
    MonetaryAmount quoteAmountToEat = lineAmount.toNeutral() * linePrice;

    if (remainingAmountInBaseOrQuote.currencyCode() == _market.quote()) {
      if (quoteAmountToEat < remainingAmountInBaseOrQuote) {
        matchedAmount += lineAmount;
      } else {
        quoteAmountToEat = remainingAmountInBaseOrQuote;
        matchedAmount += MonetaryAmount(remainingAmountInBaseOrQuote / linePrice, _market.base());
      }
      remainingAmountInBaseOrQuote -= quoteAmountToEat;
      avgPrice += quoteAmountToEat;
    } else {
      // amountInBaseOrQuote is in base currency
      const MonetaryAmount baseAmountToEat = std::min(lineAmount, remainingAmountInBaseOrQuote);
      matchedAmount += baseAmountToEat;
      remainingAmountInBaseOrQuote -= baseAmountToEat;

      avgPrice += baseAmountToEat.toNeutral() * linePrice;
    }

    if (remainingAmountInBaseOrQuote == 0 || pos + 1 == nbOrders) {
      break;
    }
  }
  if (matchedAmount != 0) {
    avgPrice /= matchedAmount.toNeutral();
  }
  return {matchedAmount, avgPrice};
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

MarketOrderBook::AmountPerPriceVec MarketOrderBook::computeMatchedParts(TradeSide tradeSide, MonetaryAmount amount,
                                                                        MonetaryAmount price) const {
  AmountPerPriceVec ret;
  const int nbOrders = _orders.size();
  const auto volumeNbDecimals = _volAndPriNbDecimals.volNbDecimals;
  const std::optional<AmountType> integralTotalAmountOpt = amount.amount(volumeNbDecimals);
  if (!integralTotalAmountOpt) {
    return ret;
  }
  AmountType integralTotalAmount = *integralTotalAmountOpt;
  const auto countAmount = [volumeNbDecimals, &ret, &integralTotalAmount, cur = amount.currencyCode()](
                               MonetaryAmount linePrice, const AmountType intAmount) {
    if (intAmount < integralTotalAmount) {
      ret.emplace_back(MonetaryAmount(intAmount, cur, volumeNbDecimals), linePrice);
      integralTotalAmount -= intAmount;
    } else {
      ret.emplace_back(MonetaryAmount(integralTotalAmount, cur, volumeNbDecimals), linePrice);
      integralTotalAmount = 0;
    }
  };
  switch (tradeSide) {
    case TradeSide::buy:
      for (int pos = _highestBidPricePos + 1; pos < nbOrders && integralTotalAmount > 0; ++pos) {
        // amount is < 0 here
        const auto linePrice = priceAt(pos);
        if (price < linePrice) {
          break;
        }
        countAmount(linePrice, -_orders[pos].amount);
      }
      break;
    case TradeSide::sell:
      for (int pos = _lowestAskPricePos - 1; pos >= 0 && integralTotalAmount > 0; --pos) {
        const auto linePrice = priceAt(pos);
        if (price > linePrice) {
          break;
        }
        countAmount(linePrice, _orders[pos].amount);
      }
      break;
    default:
      unreachable();
  }
  return ret;
}

AmountPrice MarketOrderBook::avgPriceAndMatchedVolume(TradeSide tradeSide, MonetaryAmount amount,
                                                      MonetaryAmount price) const {
  switch (tradeSide) {
    case TradeSide::buy:
      return avgPriceAndMatchedVolumeBuy(amount, price);
    case TradeSide::sell:
      return avgPriceAndMatchedVolumeSell(amount, price);
    default:
      throw exception("Unexpected trade side {}", static_cast<int>(tradeSide));
  }
}

AmountPrice MarketOrderBook::avgPriceAndMatchedAmountTaker(MonetaryAmount amountInBaseOrQuote) const {
  if (amountInBaseOrQuote.currencyCode() == _market.base()) {
    return avgPriceAndMatchedVolumeSell(amountInBaseOrQuote, MonetaryAmount(0, _market.quote()));
  }
  const auto [matchedVolume, price] = avgPriceAndMatchedVolumeBuy(
      amountInBaseOrQuote, MonetaryAmount(std::numeric_limits<MonetaryAmount::AmountType>::max(), _market.quote()));
  return {matchedVolume.toNeutral() * price, price};
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
AmountPrice MarketOrderBook::operator[](int relativePosToLimitPrice) const {
  if (relativePosToLimitPrice == 0) {
    const auto [v11, v12] = (*this)[-1];
    const auto [v21, v22] = (*this)[1];
    return {v11 + (v21 - v11) / 2, v12 + (v22 - v12) / 2};
  }
  if (relativePosToLimitPrice < 0) {
    const int pos = _lowestAskPricePos + relativePosToLimitPrice;
    return {amountAt(pos), priceAt(pos)};
  }
  // > 0
  const int pos = _highestBidPricePos + relativePosToLimitPrice;
  return {negAmountAt(pos), priceAt(pos)};
}

std::optional<MonetaryAmount> MarketOrderBook::convertBaseAmountToQuote(MonetaryAmount amountInBaseCurrency) const {
  if (amountInBaseCurrency.currencyCode() != _market.base()) {
    throw exception("Given amount should be in the base currency of this market");
  }
  MonetaryAmount quoteAmount(0, _market.quote());
  for (int pos = _lowestAskPricePos - 1; pos >= 0; --pos) {
    const MonetaryAmount amount = amountAt(pos);
    const MonetaryAmount price = priceAt(pos);
    const MonetaryAmount amountToEat = std::min(amount, amountInBaseCurrency);

    quoteAmount += amountToEat.toNeutral() * price;
    amountInBaseCurrency -= amountToEat;
    if (amountInBaseCurrency == 0) {
      return quoteAmount;
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
  return marketOrderBook[relativePrice].price;
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
    case PriceStrategy::taker:
      [[fallthrough]];
    case PriceStrategy::nibble:
      marketCode = _market.quote();
      [[fallthrough]];
    case PriceStrategy::maker:
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
    case PriceStrategy::taker: {
      auto [avgMatchedFrom, avgPri] = avgPriceAndMatchedAmountTaker(from);
      if (avgMatchedFrom < from) {
        log::warn(
            "{} is too big to be matched immediately on {}, return limit price instead ({} matched amount among total "
            "of {})",
            from, _market, avgMatchedFrom, from);
      }
      return avgPri;
    }
    case PriceStrategy::nibble:
      marketCode = _market.quote();
      [[fallthrough]];
    case PriceStrategy::maker:
      return from.currencyCode() == marketCode ? lowestAskPrice() : highestBidPrice();
    default:
      unreachable();
  }
}

SimpleTable MarketOrderBook::getTable(ExchangeNameEnum exchangeNameEnum,
                                      std::optional<MonetaryAmount> conversionPriceRate) const {
  string h1("Sellers of ");
  string baseStr = _market.base().str();
  h1.append(baseStr).append(" (asks)");
  std::string_view exchangeName = EnumToString(exchangeNameEnum);
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
    table::Row row(amount.str());
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
