#include "marketorderbook.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>

#include "cct_exception.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_log.hpp"
#include "cct_variadictable.hpp"

namespace cct {

MarketOrderBook::MarketOrderBook(Market market, OrderBookLineSpan orderLines, VolAndPriNbDecimals volAndPriNbDecimals)
    : _market(market), _volAndPriNbDecimals(volAndPriNbDecimals), _isArtificiallyExtended(false) {
  const int nbPrices = static_cast<int>(orderLines.size());
  _orders.reserve(nbPrices);
  if (_volAndPriNbDecimals == VolAndPriNbDecimals()) {
    for (const OrderBookLine& l : orderLines) {
      _volAndPriNbDecimals.volNbDecimals = std::min(_volAndPriNbDecimals.volNbDecimals, l._amount.maxNbDecimals());
      _volAndPriNbDecimals.priNbDecimals = std::min(_volAndPriNbDecimals.priNbDecimals, l._price.maxNbDecimals());
    }
  }
  if (nbPrices == 0) {
    _lowestAskPricePos = 0;
    _highestBidPricePos = 0;
  } else {
    for (const OrderBookLine& l : orderLines) {
      assert(l._amount.currencyCode() == market.base() && l._price.currencyCode() == market.quote());
      // amounts cannot be nullopt here
      if (l._amount.isZero()) {
        // Just ignore empty lines
        continue;
      }
      AmountPrice::AmountType amountIntegral = *l._amount.amount(_volAndPriNbDecimals.volNbDecimals);
      AmountPrice::AmountType priceIntegral = *l._price.amount(_volAndPriNbDecimals.priNbDecimals);

      _orders.emplace_back(amountIntegral, priceIntegral);
    }

    std::sort(_orders.begin(), _orders.end(), [](AmountPrice lhs, AmountPrice rhs) { return lhs.price < rhs.price; });
    auto adjacentFindIt = std::adjacent_find(_orders.begin(), _orders.end(),
                                             [](AmountPrice lhs, AmountPrice rhs) { return lhs.price == rhs.price; });
    if (adjacentFindIt != _orders.end()) {
      throw exception("Forbidden duplicate price " + std::to_string(adjacentFindIt->price) +
                      " in the order book for market " + market.str());
    }

    auto highestBidPriceIt =
        std::partition_point(_orders.begin(), _orders.end(), [](AmountPrice a) { return a.amount > 0; });
    _highestBidPricePos = static_cast<int>(highestBidPriceIt - _orders.begin() - 1);
    _lowestAskPricePos = static_cast<int>(
        std::find_if(highestBidPriceIt, _orders.end(), [](AmountPrice a) { return a.amount < 0; }) - _orders.begin());
  }
}

MarketOrderBook::MarketOrderBook(MonetaryAmount askPrice, MonetaryAmount askVolume, MonetaryAmount bidPrice,
                                 MonetaryAmount bidVolume, int depth, VolAndPriNbDecimals volAndPriNbDecimals)
    : _market(askVolume.currencyCode(), askPrice.currencyCode()),
      _volAndPriNbDecimals(volAndPriNbDecimals),
      _isArtificiallyExtended(depth > 1) {
  if (depth == 0) {
    throw exception("Invalid depth, should be strictly positive");
  }
  if (bidPrice == askPrice || bidVolume.isZero() || askVolume.isZero()) {
    throw exception("Invalid ticker information for MarketOrderBook");
  }
  _orders.reserve(depth);
  if (_volAndPriNbDecimals == VolAndPriNbDecimals()) {
    _volAndPriNbDecimals.volNbDecimals = std::max(askVolume.nbDecimals(), bidVolume.nbDecimals());
    _volAndPriNbDecimals.priNbDecimals = std::max(askPrice.nbDecimals(), bidPrice.nbDecimals());
  }
  const AmountPrice::AmountType stepPrice = *(askPrice - bidPrice).amount(_volAndPriNbDecimals.priNbDecimals);

  // Add bid lines first
  const AmountPrice refBidAmountPrice(*bidVolume.amount(_volAndPriNbDecimals.volNbDecimals),
                                      *bidPrice.amount(_volAndPriNbDecimals.priNbDecimals));
  const AmountPrice refAskAmountPrice(-(*askVolume.amount(_volAndPriNbDecimals.volNbDecimals)),
                                      *askPrice.amount(_volAndPriNbDecimals.priNbDecimals));
  const AmountPrice::AmountType simulatedStepVol = std::midpoint(refBidAmountPrice.amount, -refAskAmountPrice.amount);

  for (AmountPrice::AmountType d = depth; d != 0; --d) {
    AmountPrice amountPrice = refBidAmountPrice;
    if (d != 1) {
      amountPrice.price -= stepPrice * (d - 1);
      amountPrice.amount += (3L * simulatedStepVol * (d - 1)) / 2;  // TODO: add overflow check here
    }
    _orders.push_back(amountPrice);
  }
  _highestBidPricePos = _orders.size() - 1;
  _lowestAskPricePos = _highestBidPricePos + 1;

  // Finally add ask lines
  for (AmountPrice::AmountType d = 0; d < depth; ++d) {
    AmountPrice amountPrice = refAskAmountPrice;
    if (d != 0) {
      amountPrice.price += stepPrice * d;
      amountPrice.amount -= (3L * simulatedStepVol * d) / 2;  // TODO: add overflow check here
    }
    _orders.push_back(amountPrice);
  }
}

std::optional<MonetaryAmount> MarketOrderBook::averagePrice() const {
  if (_orders.empty()) {
    return std::optional<MonetaryAmount>();
  }
  // std::midpoint computes safely the average of two values (without overflow)
  return MonetaryAmount(std::midpoint(_orders[_lowestAskPricePos].price, _orders[_highestBidPricePos].price),
                        _market.quote(), _volAndPriNbDecimals.priNbDecimals);
}

MonetaryAmount MarketOrderBook::computeCumulAmountBoughtImmediatelyAt(MonetaryAmount p) const {
  MonetaryAmount::AmountType integralAmountRep = 0;
  const int nbOrders = _orders.size();
  for (int pos = _lowestAskPricePos; pos < nbOrders && priceAt(pos) <= p; ++pos) {
    integralAmountRep -= _orders[pos].amount;  // Minus as amounts are negative here
  }
  return MonetaryAmount(integralAmountRep, _market.base(), _volAndPriNbDecimals.volNbDecimals);
}

MonetaryAmount MarketOrderBook::computeCumulAmountSoldImmediatelyAt(MonetaryAmount p) const {
  MonetaryAmount::AmountType integralAmountRep = 0;
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
  MonetaryAmount::AmountType integralAmountRep = 0;
  const std::optional<MonetaryAmount::AmountType> integralTotalAmountToBuyOpt =
      a.amount(_volAndPriNbDecimals.volNbDecimals);
  if (!integralTotalAmountToBuyOpt) {
    return std::nullopt;
  }
  const MonetaryAmount::AmountType integralTotalAmountToBuy = *integralTotalAmountToBuyOpt;
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
  MonetaryAmount::AmountType integralAmountRep = 0;
  const std::optional<MonetaryAmount::AmountType> integralTotalAmountToBuyOpt =
      a.amount(_volAndPriNbDecimals.volNbDecimals);
  AmountPerPriceVec ret;
  if (!integralTotalAmountToBuyOpt) {
    log::warn("Not enough amount to buy {} on market {}", a.str(), _market.str());
    return ret;
  }
  const MonetaryAmount::AmountType integralTotalAmountToBuy = *integralTotalAmountToBuyOpt;

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
  log::warn("Not enough amount to buy {} on market {} ({} max)", a.str(), _market.str(),
            MonetaryAmount(integralAmountRep, _market.base(), _volAndPriNbDecimals.volNbDecimals).str());
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
  MonetaryAmount ret("0", m.quote());
  MonetaryAmount totalAmount("0", m.base());
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
  MonetaryAmount::AmountType integralAmountRep = 0;
  const std::optional<MonetaryAmount::AmountType> integralTotalAmountToBuyOpt =
      a.amount(_volAndPriNbDecimals.volNbDecimals);
  AmountPerPriceVec ret;
  if (!integralTotalAmountToBuyOpt) {
    return std::nullopt;
  }
  const MonetaryAmount::AmountType integralTotalAmountToBuy = *integralTotalAmountToBuyOpt;

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
  MonetaryAmount::AmountType integralAmountRep = 0;
  const std::optional<MonetaryAmount::AmountType> integralTotalAmountToBuyOpt =
      a.amount(_volAndPriNbDecimals.volNbDecimals);
  AmountPerPriceVec ret;
  if (!integralTotalAmountToBuyOpt) {
    log::debug("Not enough amount to sell {} on market {}", a.str(), _market.str());
    return ret;
  }
  const MonetaryAmount::AmountType integralTotalAmountToBuy = *integralTotalAmountToBuyOpt;

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
  log::debug("Not enough amount to sell {} on market {}", a.str(), _market.str());
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
  MonetaryAmount avgPrice("0", _market.quote());
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

std::optional<MonetaryAmount> MarketOrderBook::convertAtAvgPrice(MonetaryAmount amountInBaseOrQuote) const {
  std::optional<MonetaryAmount> avgPrice = averagePrice();
  if (!avgPrice) {
    return avgPrice;
  }
  return amountInBaseOrQuote.currencyCode() == _market.base()
             ? *avgPrice * amountInBaseOrQuote.toNeutral()
             : MonetaryAmount(amountInBaseOrQuote / *avgPrice, _market.base());
}

std::optional<MonetaryAmount> MarketOrderBook::convertBaseAmountToQuote(MonetaryAmount amountInBaseCurrency) const {
  if (amountInBaseCurrency.currencyCode() != _market.base()) {
    throw exception("Given amount should be in the base currency of this market");
  }
  MonetaryAmount quoteAmount("0", _market.quote());
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
  MonetaryAmount baseAmount("0", _market.base());
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
      return baseAmount + MonetaryAmount("1", _market.base()) * (amountInQuoteCurrency / price);
    }
  }
  return std::optional<MonetaryAmount>();
}

void MarketOrderBook::print(std::ostream& os) const {
  cct::FixedCapacityVector<std::string, 3> cols;
  cols.emplace_back("Sellers of ").append(_market.base().str()).append(" (asks)");
  cols.emplace_back(_market.base().str()).append(" price in ").append(_market.quote().str());
  cols.emplace_back("Buyers of ").append(_market.base().str()).append(" (bids)");
  VariadicTable<std::string, std::string, std::string> vt(std::move(cols));
  for (int op = _orders.size(); op > 0; --op) {
    const int pos = op - 1;
    MonetaryAmount amount(std::abs(_orders[pos].amount), CurrencyCode::kNeutral, _volAndPriNbDecimals.volNbDecimals);
    if (_orders[pos].amount < 0) {
      vt.addRow(amount.amountStr(), priceAt(pos).amountStr(), "");
    } else {
      vt.addRow("", priceAt(pos).amountStr(), amount.amountStr());
    }
  }
  vt.print(os);
}

void MarketOrderBook::print(std::ostream& os, MonetaryAmount conversionPriceRate) const {
  cct::FixedCapacityVector<std::string, 4> cols;
  cols.emplace_back("Sellers of ").append(_market.base().str()).append(" (asks)");
  cols.emplace_back(_market.base().str()).append(" price in ").append(_market.quote().str());
  cols.emplace_back(_market.base().str()).append(" price in ").append(conversionPriceRate.currencyCode().str());
  cols.emplace_back("Buyers of ").append(_market.base().str()).append(" (bids)");
  VariadicTable<std::string, std::string, std::string, std::string> vt(std::move(cols));
  for (int op = _orders.size(); op > 0; --op) {
    const int pos = op - 1;
    MonetaryAmount amount(std::abs(_orders[pos].amount), CurrencyCode::kNeutral, _volAndPriNbDecimals.volNbDecimals);
    MonetaryAmount price = priceAt(pos);
    MonetaryAmount convertedPrice = price.toNeutral() * conversionPriceRate.toNeutral();
    if (_orders[pos].amount < 0) {
      vt.addRow(amount.str(), price.amountStr(), convertedPrice.str(), "");
    } else {
      vt.addRow("", price.amountStr(), convertedPrice.str(), amount.str());
    }
  }
  vt.print(os);
}

}  // namespace cct
