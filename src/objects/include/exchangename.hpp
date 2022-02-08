#pragma once

#include <span>
#include <string_view>

#include "cct_const.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"

namespace cct {

using ExchangeName = string;
using PublicExchangeNames = FixedCapacityVector<ExchangeName, kNbSupportedExchanges>;
using ExchangeNameSpan = std::span<const ExchangeName>;

class PrivateExchangeName {
 public:
  PrivateExchangeName() noexcept(std::is_nothrow_default_constructible_v<string>) = default;

  /// Constructs a PrivateExchangeName with a unique identifier name.
  /// Two cases:
  ///  - either there is no '_', in this case, keyName will be empty
  ///  - either there is a '_', in this case 'globalExchangeName' will be parsed as '<exchangeName>_<keyName>'
  /// Important: it is ok to have '_' in the key name itself, but forbidden in the exchange name as it is the
  /// first '_' that is important.
  explicit PrivateExchangeName(std::string_view globalExchangeName);

  PrivateExchangeName(std::string_view exchangeName, std::string_view keyName);

  std::string_view name() const { return std::string_view(_nameWithKey.begin(), _nameWithKey.begin() + _dashPos); }

  std::string_view keyName() const {
    return std::string_view(_nameWithKey.begin() + _dashPos + (_dashPos != _nameWithKey.size()), _nameWithKey.end());
  }

  bool isKeyNameDefined() const { return _dashPos < _nameWithKey.size(); }

  std::string_view str() const { return _nameWithKey; }

  bool operator==(const PrivateExchangeName &o) const { return _nameWithKey == o._nameWithKey; }
  bool operator!=(const PrivateExchangeName &o) const { return !(*this == o); }

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 private:
  string _nameWithKey;
  std::size_t _dashPos = 0;
};

using PrivateExchangeNames = SmallVector<PrivateExchangeName, kTypicalNbPrivateAccounts>;

inline std::string_view ToString(const ExchangeName &exchangeName) { return exchangeName; }
inline std::string_view ToString(const PrivateExchangeName &exchangeName) { return exchangeName.str(); }

template <class ExchangeNames>
inline string ConstructAccumulatedExchangeNames(const ExchangeNames &exchangeNames) {
  string exchangesStr(exchangeNames.empty() ? "all" : "");
  for (const auto &exchangeName : exchangeNames) {
    if (!exchangesStr.empty()) {
      exchangesStr.push_back(',');
    }
    exchangesStr.append(ToString(exchangeName));
  }
  return exchangesStr;
}
}  // namespace cct