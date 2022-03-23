#pragma once

#include <algorithm>
#include <ostream>
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

  std::string_view name() const {
    std::size_t dash = dashPos();
    return std::string_view(_nameWithKey.data(), dash == string::npos ? _nameWithKey.size() : dash);
  }

  std::string_view keyName() const {
    std::size_t dash = dashPos();
    return std::string_view(_nameWithKey.begin() + (dash == string::npos ? _nameWithKey.size() : dash + 1U),
                            _nameWithKey.end());
  }

  bool isKeyNameDefined() const { return dashPos() != string::npos; }

  std::string_view str() const { return _nameWithKey; }

  bool operator==(const PrivateExchangeName &) const = default;

  friend std::ostream &operator<<(std::ostream &os, const PrivateExchangeName &v) {
    os << v.str();
    return os;
  }

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 private:
  std::size_t dashPos() const { return _nameWithKey.find('_', kMinExchangeNameLength); }

  static constexpr std::size_t kMinExchangeNameLength =
      std::ranges::min_element(kSupportedExchanges, [](std::string_view lhs, std::string_view rhs) {
        return lhs.size() < rhs.size();
      })->size();

  string _nameWithKey;
};

using PrivateExchangeNames = SmallVector<PrivateExchangeName, kTypicalNbPrivateAccounts>;

inline std::string_view ToString(const ExchangeName &exchangeName) { return exchangeName; }
inline std::string_view ToString(std::string_view exchangeName) { return exchangeName; }
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