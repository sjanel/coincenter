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
#include "exchangename.hpp"

namespace cct {
template <class ExchangeT>
class ExchangeRetrieverBase {
 public:
  using SelectedExchanges = SmallVector<ExchangeT *, kTypicalNbPrivateAccounts>;
  using UniquePublicSelectedExchanges = FixedCapacityVector<ExchangeT *, kNbSupportedExchanges>;
  using ExchangePublicT =
      std::conditional_t<std::is_const_v<ExchangeT>, std::add_const_t<typename ExchangeT::ExchangePublic>,
                         typename ExchangeT::ExchangePublic>;
  using PublicExchangesVec = FixedCapacityVector<ExchangePublicT *, kNbSupportedExchanges>;
  using UniquePublicExchanges =
      FlatSet<ExchangePublicT *, std::less<ExchangePublicT *>, amc::vec::EmptyAlloc, PublicExchangesVec>;

  ExchangeRetrieverBase() = default;

  explicit ExchangeRetrieverBase(std::span<ExchangeT> exchanges) : _exchanges(exchanges) {}

  std::span<ExchangeT> exchanges() const { return _exchanges; }

  /// Retrieve the unique Exchange corresponding to given private exchange name.
  /// Raise exception in case of ambiguity
  ExchangeT &retrieveUniqueCandidate(PrivateExchangeName privateExchangeName) const {
    ExchangeT *pExchange = nullptr;
    for (ExchangeT &exchange : _exchanges) {
      if (privateExchangeName.name() == exchange.name() &&
          (!privateExchangeName.isKeyNameDefined() || exchange.keyName() == privateExchangeName.keyName())) {
        if (pExchange) {
          throw exception("Several private exchanges found for " + privateExchangeName.str() +
                          " - remove ambiguity by specifying key name");
        }
        pExchange = std::addressof(exchange);
      }
    }
    if (!pExchange) {
      throw exception("Cannot find exchange " + privateExchangeName.str());
    }
    return *pExchange;
  }

  /// Retrieve all selected exchange addresses matching given public names, or all if empty span is given.
  /// Returned exchanges always follow their initial order (at creation of 'ExchangeRetrieverBase')
  SelectedExchanges retrieveSelectedExchanges(
      std::span<const PublicExchangeName> exchangeNames = std::span<const PublicExchangeName>()) const {
    SelectedExchanges ret;
    if (exchangeNames.empty()) {
      std::transform(_exchanges.begin(), _exchanges.end(), std::back_inserter(ret),
                     [](ExchangeT &e) { return std::addressof(e); });
    } else {
      for (ExchangeT &e : _exchanges) {
        if (std::any_of(exchangeNames.begin(), exchangeNames.end(),
                        [&e](const PublicExchangeName &exchangeName) { return e.name() == exchangeName; })) {
          ret.push_back(std::addressof(e));
        }
      }
    }

    return ret;
  }

  UniquePublicSelectedExchanges retrieveAtMostOneAccountSelectedExchanges(
      std::span<const PublicExchangeName> exchangeNames) const {
    auto selectedExchanges = retrieveSelectedExchanges(exchangeNames);
    UniquePublicSelectedExchanges ret;
    std::copy_if(selectedExchanges.begin(), selectedExchanges.end(), std::back_inserter(ret), [&ret](ExchangeT *e) {
      return std::none_of(ret.begin(), ret.end(), [e](ExchangeT *o) { return o->name() == e->name(); });
    });
    return ret;
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
}  // namespace cct