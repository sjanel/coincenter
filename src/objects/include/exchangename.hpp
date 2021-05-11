#pragma once

#include <string>
#include <string_view>

#include "cct_const.hpp"
#include "cct_smallvector.hpp"

namespace cct {

using PublicExchangeName = std::string;
using PublicExchangeNames = cct::SmallVector<PublicExchangeName, kTypicalNbExchanges>;

class PrivateExchangeName {
 public:
  PrivateExchangeName() : _dashPos() {}

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

  const std::string &str() const { return _nameWithKey; }

  bool operator==(const PrivateExchangeName &o) const { return _nameWithKey == o._nameWithKey; }
  bool operator!=(const PrivateExchangeName &o) const { return !(*this == o); }

 private:
  std::string _nameWithKey;
  std::size_t _dashPos;
};

using PrivateExchangeNames = cct::SmallVector<PrivateExchangeName, kTypicalNbExchanges>;
}  // namespace cct