#include "proto-market-order-book-converter.hpp"

#include <chrono>
#include <cstdint>
#include <ranges>

#include "market-order-book.pb.h"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct {
::proto::MarketOrderBook ConvertMarketOrderBookToProto(const MarketOrderBook& marketOrderBook) {
  ::proto::MarketOrderBook protoObj;

  const auto [volNbDecimals, priNbDecimals] = marketOrderBook.volAndPriNbDecimals();
  const auto unixTimestampInMs = TimestampToMillisecondsSinceEpoch(marketOrderBook.time());

  protoObj.set_unixtimestampinms(unixTimestampInMs);
  protoObj.set_volumenbdecimals(volNbDecimals);
  protoObj.set_pricenbdecimals(priNbDecimals);

  auto& orderBook = *protoObj.mutable_orderbook();

  const int nbBids = marketOrderBook.nbBidPrices();
  for (int bidPos = 1; bidPos <= nbBids; ++bidPos) {
    const auto [volume, price] = marketOrderBook[-bidPos];
    auto& pricedVolume = *orderBook.add_bids();

    pricedVolume.set_volume(volume.amount(volNbDecimals).value());
    pricedVolume.set_price(price.amount(priNbDecimals).value());
  }

  const int nbAsks = marketOrderBook.nbAskPrices();
  for (int askPos = 1; askPos <= nbAsks; ++askPos) {
    const auto [volume, price] = marketOrderBook[askPos];
    auto& pricedVolume = *orderBook.add_asks();

    pricedVolume.set_volume(volume.amount(volNbDecimals).value());
    pricedVolume.set_price(price.amount(priNbDecimals).value());
  }

  return protoObj;
}

MarketOrderBook MarketOrderBookConverter::operator()(const ::proto::MarketOrderBook& marketOrderBookTimedData) {
  const TimePoint timeStamp(milliseconds(marketOrderBookTimedData.unixtimestampinms()));
  const VolAndPriNbDecimals volAndPriNbDecimals(marketOrderBookTimedData.volumenbdecimals(),
                                                marketOrderBookTimedData.pricenbdecimals());

  const auto& bids = marketOrderBookTimedData.orderbook().bids();
  const auto& asks = marketOrderBookTimedData.orderbook().asks();
  const int32_t lowestAskPricePos = static_cast<int32_t>(bids.size());
  const int32_t highestBidPricePos = lowestAskPricePos - 1;

  // We directly construct the MarketOrderBook here - we trust the protobuf data (it should have been written from a
  // valid MarketOrderBook at the source)
  // Possible optimization - allocate in a reusable arena of memory instead of allocating a new buffer for each new
  // object.
  MarketOrderBook::AmountPriceVector orders;

  orders.reserve(bids.size() + asks.size());

  for (const auto& bid : std::ranges::reverse_view(bids)) {
    orders.emplace_back(bid.volume(), bid.price());
  }

  for (const auto& ask : asks) {
    orders.emplace_back(-ask.volume(), ask.price());
  }

  return MarketOrderBook{timeStamp,          _market,           std::move(orders),
                         highestBidPricePos, lowestAskPricePos, volAndPriNbDecimals};
}

}  // namespace cct
