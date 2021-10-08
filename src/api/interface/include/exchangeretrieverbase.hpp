#pragma once

#include <algorithm>
#include <memory>
#include <span>
#include <type_traits>

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_fixedcapacityvector.hpp"
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
  using UniquePublicExchanges = PublicExchangesVec;

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

  enum class Order { kInitial, kSelection };

  /// Retrieve all selected exchange addresses matching given public names, or all if empty span is given.
  /// Returned exchanges order can be controlled thanks to 'order' parameter:
  ///  - 'kInitial'   : matching 'Exchanges' are returned according to their initial order (at creation of
  ///                  'this' object)
  ///  - 'kSelection' : matching 'Exchanges' are returned according to given 'exchangeNames' order,
  ///                   or initial order if empty
  SelectedExchanges retrieveSelectedExchanges(
      Order order, std::span<const PublicExchangeName> exchangeNames = std::span<const PublicExchangeName>()) const {
    SelectedExchanges ret;
    if (exchangeNames.empty()) {
      std::transform(_exchanges.begin(), _exchanges.end(), std::back_inserter(ret),
                     [](ExchangeT &e) { return std::addressof(e); });
    } else {
      switch (order) {
        case Order::kInitial:
          for (ExchangeT &e : _exchanges) {
            if (std::any_of(exchangeNames.begin(), exchangeNames.end(),
                            [&e](const PublicExchangeName &exchangeName) { return e.name() == exchangeName; })) {
              ret.push_back(std::addressof(e));
            }
          }
          break;
        case Order::kSelection:
          for (const PublicExchangeName &exchangeName : exchangeNames) {
            auto nameMatch = [&exchangeName](ExchangeT &e) { return e.name() == exchangeName; };
            auto endIt = _exchanges.end();
            auto oldSize = ret.size();
            for (auto foundIt = std::find_if(_exchanges.begin(), endIt, nameMatch); foundIt != _exchanges.end();
                 foundIt = std::find_if(std::next(foundIt), endIt, nameMatch)) {
              ret.push_back(std::addressof(*foundIt));
            }
            if (ret.size() == oldSize) {
              throw exception(
                  std::string("Unable to find public exchange ").append(exchangeName).append(" in the exchange list"));
            }
          }
          break;
        default:
          throw exception("Unknown Order");
      }
    }

    return ret;
  }

  /// Among all 'Exchange's, retrieve at most one 'Exchange' per public echange matching public exchange names.
  /// Order of 'Exchange's will respect the same order as the 'exchangeNames' given in input.
  /// Examples
  ///   {"kraken_user1", "kucoin_user1"}                 -> {"kraken_user1", "kucoin_user1"}
  ///   {"kraken_user1", "kraken_user2", "kucoin_user1"} -> {"kraken_user1", "kucoin_user1"}
  UniquePublicSelectedExchanges retrieveAtMostOneAccountSelectedExchanges(
      std::span<const PublicExchangeName> exchangeNames) const {
    auto selectedExchanges = retrieveSelectedExchanges(Order::kSelection, exchangeNames);
    UniquePublicSelectedExchanges ret;
    std::copy_if(selectedExchanges.begin(), selectedExchanges.end(), std::back_inserter(ret), [&ret](ExchangeT *e) {
      return std::none_of(ret.begin(), ret.end(), [e](ExchangeT *o) { return o->name() == e->name(); });
    });
    return ret;
  }

  /// Extract the 'ExchangePublic' from the 'Exchange' corresponding to given 'PublicExchangeNames'.
  /// Order of public exchanges will respect the same order as the 'exchangeNames' given in input.
  /// Examples
  ///   {"kraken_user1", "kucoin_user1"}                 -> {"kraken", "kucoin"}
  ///   {"kraken_user1", "kraken_user2", "kucoin_user1"} -> {"kraken", "kucoin"}
  UniquePublicExchanges retrieveUniquePublicExchanges(std::span<const PublicExchangeName> exchangeNames) const {
    auto selectedExchanges = retrieveAtMostOneAccountSelectedExchanges(exchangeNames);
    UniquePublicExchanges selectedPublicExchanges;
    std::transform(selectedExchanges.begin(), selectedExchanges.end(), std::back_inserter(selectedPublicExchanges),
                   [](ExchangeT *e) { return std::addressof(e->apiPublic()); });
    return selectedPublicExchanges;
  }

 private:
  std::span<ExchangeT> _exchanges;
};
}  // namespace cct