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

  ExchangeRetrieverBase() noexcept = default;

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
          string ex("Several private exchanges found for ");
          ex.append(privateExchangeName.str()).append(" - remove ambiguity by specifying key name");
          throw exception(std::move(ex));
        }
        pExchange = std::addressof(exchange);
      }
    }
    if (!pExchange) {
      string ex("Cannot find exchange ");
      ex.append(privateExchangeName.str());
      throw exception(std::move(ex));
    }
    return *pExchange;
  }

  enum class Order { kInitial, kSelection };

 private:
  template <class Names, class Matcher>
  SelectedExchanges select(Order order, const Names &names, Matcher matcher) const {
    SelectedExchanges ret;
    if (names.empty()) {
      ret.reserve(static_cast<typename SelectedExchanges::size_type>(_exchanges.size()));
      std::transform(_exchanges.begin(), _exchanges.end(), std::back_inserter(ret),
                     [](ExchangeT &e) { return std::addressof(e); });
    } else {
      switch (order) {
        case Order::kInitial:
          for (ExchangeT &e : _exchanges) {
            if (std::any_of(names.begin(), names.end(), [&e, &matcher](const auto &n) { return matcher(e, n); })) {
              ret.push_back(std::addressof(e));
            }
          }
          break;
        case Order::kSelection:
          for (const auto &n : names) {
            auto nameMatch = [&n, &matcher](ExchangeT &e) { return matcher(e, n); };
            auto endIt = _exchanges.end();
            auto oldSize = ret.size();
            for (auto foundIt = std::find_if(_exchanges.begin(), endIt, nameMatch); foundIt != _exchanges.end();
                 foundIt = std::find_if(std::next(foundIt), endIt, nameMatch)) {
              ret.push_back(std::addressof(*foundIt));
            }
            if (ret.size() == oldSize) {
              string ex("Unable to find ");
              ex.append(ToString(n));
              ex.append(" in the exchange list");
              throw exception(std::move(ex));
            }
          }
          break;
        default:
          throw exception("Unknown Order");
      }
    }

    return ret;
  }

  template <class NameType>
  struct Matcher {
    static_assert(std::is_same_v<NameType, ExchangeName> || std::is_same_v<NameType, PrivateExchangeName>);

    bool operator()(const ExchangeT &e, const NameType &n) const {
      if constexpr (std::is_same_v<NameType, ExchangeName>) {
        return e.name() == n;
      } else {
        return e.matchesKeyNameWildcard(n);
      }
    }
  };

  template <class NamesContainerType>
  using NameType = std::remove_cvref_t<decltype(*std::declval<NamesContainerType>().begin())>;

 public:
  /// Retrieve all selected exchange addresses matching given public names, or all if empty span is given.
  /// Returned exchanges order can be controlled thanks to 'order' parameter:
  ///  - 'kInitial'   : matching 'Exchanges' are returned according to their initial order (at creation of
  ///                  'this' object)
  ///  - 'kSelection' : matching 'Exchanges' are returned according to given 'exchangeNames' order,
  ///                   or initial order if empty
  template <class Names>
  SelectedExchanges select(Order order, const Names &exchangeNames) const {
    return select(order, exchangeNames, Matcher<NameType<Names>>());
  }

  /// Among all 'Exchange's, retrieve at most one 'Exchange' per public echange matching public exchange names.
  /// Order of 'Exchange's will respect the same order as the 'exchangeNames' given in input.
  /// Examples
  ///   {"kraken_user1", "kucoin_user1"}                 -> {"kraken_user1", "kucoin_user1"}
  ///   {"kraken_user1", "kraken_user2", "kucoin_user1"} -> {"kraken_user1", "kucoin_user1"}
  ///   {"huobi",        "kucoin_user1"}                 -> {"huobi_user1",  "kucoin_user1"}
  template <class Names>
  UniquePublicSelectedExchanges selectOneAccount(const Names &exchangeNames) const {
    SelectedExchanges selectedExchanges = select(Order::kSelection, exchangeNames, Matcher<NameType<Names>>());
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
  ///   {"huobi",        "kucoin_user1"}                 -> {"huobi",  "kucoin"}
  template <class Names>
  PublicExchangesVec selectPublicExchanges(const Names &exchangeNames) const {
    auto selectedExchanges = selectOneAccount(exchangeNames);
    PublicExchangesVec selectedPublicExchanges;
    std::transform(selectedExchanges.begin(), selectedExchanges.end(), std::back_inserter(selectedPublicExchanges),
                   [](ExchangeT *e) { return std::addressof(e->apiPublic()); });
    return selectedPublicExchanges;
  }

 private:
  std::span<ExchangeT> _exchanges;
};
}  // namespace cct