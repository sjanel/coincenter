#pragma once

#include <algorithm>
#include <memory>
#include <span>
#include <type_traits>

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_flatset.hpp"
#include "cct_smallvector.hpp"
#include "exchange.hpp"
#include "exchangename.hpp"
#include "exchangepublicapi.hpp"

namespace cct {
namespace details {
template <class ExchangeT>
class ExchangeRetriever {
 public:
  using SelectedExchanges = SmallVector<ExchangeT *, kTypicalNbPrivateAccounts>;
  using UniquePublicSelectedExchanges = FixedCapacityVector<ExchangeT *, kNbSupportedExchanges>;
  using ExchangePublicT =
      std::conditional_t<std::is_const_v<ExchangeT>, const api::ExchangePublic, api::ExchangePublic>;
  using PublicExchangesVec = FixedCapacityVector<ExchangePublicT *, kNbSupportedExchanges>;
  using UniquePublicExchanges =
      FlatSet<ExchangePublicT *, std::less<ExchangePublicT *>, amc::vec::EmptyAlloc, PublicExchangesVec>;

  ExchangeRetriever() = default;

  explicit ExchangeRetriever(std::span<ExchangeT> exchanges) : _exchanges(exchanges) {}

  std::span<ExchangeT> exchanges() const { return _exchanges; }

  ExchangeT &retrieveUniqueCandidate(PrivateExchangeName privateExchangeName) const {
    SelectedExchanges ret;
    for (ExchangeT &exchange : _exchanges) {
      if (privateExchangeName.name() == exchange.name() &&
          (!privateExchangeName.isKeyNameDefined() || exchange.keyName() == privateExchangeName.keyName())) {
        ret.push_back(std::addressof(exchange));
      }
    }
    if (ret.empty()) {
      throw exception("Cannot find exchange " + privateExchangeName.str());
    }
    if (ret.size() > 1) {
      throw exception("Several private exchanges found for " + privateExchangeName.str() +
                      " - remove ambiguity by specifying key name");
    }
    return *ret.front();
  }

  SelectedExchanges retrieveSelectedExchanges(std::span<const PublicExchangeName> exchangeNames) const {
    SelectedExchanges ret;
    if (exchangeNames.empty()) {
      std::transform(_exchanges.begin(), _exchanges.end(), std::back_inserter(ret),
                     [](ExchangeT &e) { return std::addressof(e); });
    } else {
      for (std::string_view exchangeName : exchangeNames) {
        auto exchangeIt = std::find_if(_exchanges.begin(), _exchanges.end(),
                                       [exchangeName](const Exchange &e) { return e.name() == exchangeName; });
        if (exchangeIt == _exchanges.end()) {
          throw exception("Cannot find exchange " + std::string(exchangeName));
        }
        ret.push_back(std::addressof(*exchangeIt));
      }
    }

    return ret;
  }

  UniquePublicSelectedExchanges retrieveAtMostOneAccountSelectedExchanges(
      std::span<const PublicExchangeName> exchangeNames) const {
    auto selectedExchanges = retrieveSelectedExchanges(exchangeNames);
    std::sort(selectedExchanges.begin(), selectedExchanges.end(),
              [](ExchangeT *lhs, ExchangeT *rhs) { return lhs->name() < rhs->name(); });
    auto newEndIt = std::unique(selectedExchanges.begin(), selectedExchanges.end(),
                                [](ExchangeT *lhs, ExchangeT *rhs) { return lhs->name() == rhs->name(); });
    return UniquePublicSelectedExchanges(selectedExchanges.begin(), newEndIt);
  }

  UniquePublicExchanges retrieveUniquePublicExchanges(std::span<const PublicExchangeName> exchangeNames) const {
    auto selectedExchanges = retrieveSelectedExchanges(exchangeNames);
    UniquePublicExchanges selectedPublicExchanges;
    std::transform(selectedExchanges.begin(), selectedExchanges.end(),
                   std::inserter(selectedPublicExchanges, selectedPublicExchanges.end()),
                   [](ExchangeT *e) { return std::addressof(e->apiPublic()); });
    return selectedPublicExchanges;
  }

 private:
  std::span<ExchangeT> _exchanges;
};
}  // namespace details

using ExchangeRetriever = details::ExchangeRetriever<Exchange>;
using ConstExchangeRetriever = details::ExchangeRetriever<const Exchange>;
}  // namespace cct