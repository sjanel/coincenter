#pragma once

namespace proto {
class PublicTrade;
}

namespace cct {

struct ProtoPublicTradeComp {
  bool operator()(const ::proto::PublicTrade &lhs, const ::proto::PublicTrade &rhs) const;
};

struct ProtoPublicTradeEqual {
  bool operator()(const ::proto::PublicTrade &lhs, const ::proto::PublicTrade &rhs) const;
};

}  // namespace cct