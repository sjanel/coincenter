#include "exchangename.hpp"

#include "cct_exception.hpp"
#include "toupperlower.hpp"

namespace cct {
PrivateExchangeName::PrivateExchangeName(std::string_view globalExchangeName)
    : _nameWithKey(globalExchangeName), _dashPos(globalExchangeName.find('_')) {
  if (_dashPos == std::string_view::npos) {
    _dashPos = _nameWithKey.size();
  }
  std::transform(_nameWithKey.begin(), _nameWithKey.begin() + _dashPos, _nameWithKey.begin(),
                 [](char c) { return tolower(c); });
}

PrivateExchangeName::PrivateExchangeName(std::string_view exchangeName, std::string_view keyName)
    : _nameWithKey(tolower(exchangeName)), _dashPos(_nameWithKey.size()) {
  if (_nameWithKey.find('_') != string::npos) {
    throw exception("Invalid exchange name " + _nameWithKey);
  }
  if (!keyName.empty()) {
    _nameWithKey.push_back('_');
    _nameWithKey.append(keyName);
  }
}

}  // namespace cct