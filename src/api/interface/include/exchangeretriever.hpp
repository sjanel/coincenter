#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <type_traits>

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_smallvector.hpp"
#include "exchange.hpp"
#include "exchangename.hpp"
#include "exchangepublicapi.hpp"

namespace cct {

class ExchangeRetriever {
 public:
  using SelectedExchanges = SmallVector<Exchange *, kTypicalNbPrivateAccounts>;
  using UniquePublicSelectedExchanges = FixedCapacityVector<Exchange *, kNbSupportedExchanges>;
  using PublicExchangesVec = FixedCapacityVector<api::ExchangePublic *, kNbSupportedExchanges>;

  enum class Order : int8_t { kInitial, kSelection };
  enum class Filter : int8_t { kNone, kWithAccountWhenEmpty };

  ExchangeRetriever() noexcept = default;

  explicit ExchangeRetriever(std::span<Exchange> exchanges) : _exchanges(exchanges) {}

  std::span<Exchange> exchanges() const { return _exchanges; }

  /// Retrieve the unique Exchange corresponding to given exchange name.
  /// Raise exception in case of ambiguity
  Exchange &retrieveUniqueCandidate(const ExchangeName &exchangeName) const {
    Exchange *pExchange = nullptr;
    for (Exchange &exchange : _exchanges) {
      if (exchange.matches(exchangeName)) {
        if (pExchange != nullptr) {
          throw exception("Several private exchanges found for {} - remove ambiguity by specifying key name",
                          exchangeName.str());
        }
        pExchange = std::addressof(exchange);
      }
    }
    if (pExchange == nullptr) {
      throw exception("Cannot find exchange {:ek}", exchangeName);
    }
    return *pExchange;
  }

 private:
  template <class Names, class Matcher>
  SelectedExchanges select(Order order, const Names &names, Matcher matcher, Filter filter) const {
    SelectedExchanges ret;
    if (names.empty()) {
      auto exchangeAddress = [](Exchange &exchange) { return std::addressof(exchange); };
      switch (filter) {
        case Filter::kNone:
          ret.resize(static_cast<SelectedExchanges::size_type>(_exchanges.size()));
          std::ranges::transform(_exchanges, ret.begin(), exchangeAddress);
          break;
        case Filter::kWithAccountWhenEmpty:
          std::ranges::transform(
              _exchanges | std::views::filter([](Exchange &exchange) { return exchange.hasPrivateAPI(); }),
              std::back_inserter(ret), exchangeAddress);
          break;
        default:
          throw exception("Unknown filter");
      }
    } else {
      switch (order) {
        case Order::kInitial:
          for (Exchange &exchange : _exchanges) {
            if (std::ranges::any_of(names, [&exchange, &matcher](const auto &n) { return matcher(exchange, n); })) {
              ret.push_back(std::addressof(exchange));
            }
          }
          break;
        case Order::kSelection:
          for (const auto &name : names) {
            auto nameMatch = [&name, &matcher](Exchange &exchange) { return matcher(exchange, name); };
            auto endIt = _exchanges.end();
            auto oldSize = ret.size();
            for (auto foundIt = std::ranges::find_if(_exchanges, nameMatch); foundIt != _exchanges.end();
                 foundIt = std::find_if(std::next(foundIt), endIt, nameMatch)) {
              ret.push_back(std::addressof(*foundIt));
            }
            if (ret.size() == oldSize) {
              throw exception("Unable to find {} in the exchange list", name);
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
    static_assert(std::is_same_v<NameType, ExchangeName> || std::is_same_v<NameType, std::string_view>);

    bool operator()(const Exchange &e, const NameType &n) const {
      if constexpr (std::is_same_v<NameType, std::string_view>) {
        return e.name() == n;
      } else {
        return e.matches(n);
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
  /// filter can be set to kWithAccountWhenEmpty so that when exchange names is empty, only Exchange with accounts are
  /// returned
  template <class Names>
  SelectedExchanges select(Order order, const Names &exchangeNames, Filter filter = Filter::kNone) const {
    return select(order, exchangeNames, Matcher<NameType<Names>>(), filter);
  }

  /// Among all 'Exchange's, retrieve at most one 'Exchange' per public exchange matching public exchange names.
  /// Order of 'Exchange's will respect the same order as the 'exchangeNames' given in input.
  /// Examples
  ///   {"kraken_user1", "kucoin_user1"}                 -> {"kraken_user1", "kucoin_user1"}
  ///   {"kraken_user1", "kraken_user2", "kucoin_user1"} -> {"kraken_user1", "kucoin_user1"}
  ///   {"huobi",        "kucoin_user1"}                 -> {"huobi_user1",  "kucoin_user1"}
  template <class Names>
  UniquePublicSelectedExchanges selectOneAccount(const Names &exchangeNames, Filter filter = Filter::kNone) const {
    SelectedExchanges selectedExchanges = select(Order::kSelection, exchangeNames, Matcher<NameType<Names>>(), filter);
    UniquePublicSelectedExchanges ret;
    std::ranges::copy_if(selectedExchanges, std::back_inserter(ret), [&ret](Exchange *lhs) {
      return std::ranges::none_of(ret, [lhs](Exchange *rhs) { return lhs->name() == rhs->name(); });
    });
    return ret;
  }

  /// Extract the 'ExchangePublic' from the 'Exchange' corresponding to given 'ExchangeNames'.
  /// Order of public exchanges will respect the same order as the 'exchangeNames' given in input.
  /// Examples
  ///   {"kraken_user1", "kucoin_user1"}                 -> {"kraken", "kucoin"}
  ///   {"kraken_user1", "kraken_user2", "kucoin_user1"} -> {"kraken", "kucoin"}
  ///   {"huobi",        "kucoin_user1"}                 -> {"huobi",  "kucoin"}
  template <class Names>
  PublicExchangesVec selectPublicExchanges(const Names &exchangeNames) const {
    auto selectedExchanges = selectOneAccount(exchangeNames);
    PublicExchangesVec selectedPublicExchanges(selectedExchanges.size());
    std::ranges::transform(selectedExchanges, selectedPublicExchanges.begin(),
                           [](Exchange *exchange) { return std::addressof(exchange->apiPublic()); });
    return selectedPublicExchanges;
  }

 private:
  std::span<Exchange> _exchanges;
};

}  // namespace cct