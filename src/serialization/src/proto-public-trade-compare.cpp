#include "proto-public-trade-compare.hpp"

#include "currencycode.hpp"
#include "monetaryamount.hpp"
#include "public-trade.pb.h"

namespace cct {

namespace {
auto PriceMonetaryAmount(const ::proto::PublicTrade& obj) {
  return MonetaryAmount(obj.priceamount(), CurrencyCode{}, obj.pricenbdecimals());
}

auto VolumeMonetaryAmount(const ::proto::PublicTrade& obj) {
  return MonetaryAmount(obj.volumeamount(), CurrencyCode{}, obj.volumenbdecimals());
}
}  // namespace

bool ProtoPublicTradeComp::operator()(const ::proto::PublicTrade& lhs, const ::proto::PublicTrade& rhs) const {
  if (lhs.unixtimestampinms() != rhs.unixtimestampinms()) {
    return lhs.unixtimestampinms() < rhs.unixtimestampinms();
  }

  MonetaryAmount lhsAmount = VolumeMonetaryAmount(lhs);
  MonetaryAmount rhsAmount = VolumeMonetaryAmount(rhs);
  if (lhsAmount != rhsAmount) {
    return lhsAmount < rhsAmount;
  }

  MonetaryAmount lhsPrice = PriceMonetaryAmount(lhs);
  MonetaryAmount rhsPrice = PriceMonetaryAmount(rhs);
  if (lhsPrice != rhsPrice) {
    return lhsPrice < rhsPrice;
  }

  if (lhs.tradeside() != rhs.tradeside()) {
    return lhs.tradeside() < rhs.tradeside();
  }

  return false;
}

bool ProtoPublicTradeEqual::operator()(const ::proto::PublicTrade& lhs, const ::proto::PublicTrade& rhs) const {
  return lhs.unixtimestampinms() == rhs.unixtimestampinms() && VolumeMonetaryAmount(lhs) == VolumeMonetaryAmount(rhs) &&
         PriceMonetaryAmount(lhs) == PriceMonetaryAmount(rhs) && lhs.tradeside() == rhs.tradeside();
}

}  // namespace cct