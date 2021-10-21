#include "exchangename.hpp"

#include "cct_exception.hpp"

namespace cct {
PrivateExchangeName::PrivateExchangeName(std::string_view globalExchangeName)
    : _nameWithKey(globalExchangeName), _dashPos(globalExchangeName.find_first_of('_')) {
  if (_dashPos == std::string_view::npos) {
    _dashPos = _nameWithKey.size();
  }
}

PrivateExchangeName::PrivateExchangeName(std::string_view exchangeName, std::string_view keyName)
    : _nameWithKey(exchangeName), _dashPos(_nameWithKey.size()) {
  if (_nameWithKey.find_first_of('_') != string::npos) {
    throw exception("Invalid exchange name " + _nameWithKey);
  }
  if (!keyName.empty()) {
    _nameWithKey.push_back('_');
    _nameWithKey.append(keyName);
  }
}

}  // namespace cct