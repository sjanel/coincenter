#include "exchangename.hpp"

#include "cct_exception.hpp"
#include "toupperlower.hpp"

namespace cct {
ExchangeName::ExchangeName(std::string_view globalExchangeName) : _nameWithKey(globalExchangeName) {
  for (std::size_t charPos = 0, sz = globalExchangeName.size(); charPos < sz && _nameWithKey[charPos] != '_';
       ++charPos) {
    _nameWithKey[charPos] = tolower(_nameWithKey[charPos]);
  }
}

ExchangeName::ExchangeName(std::string_view exchangeName, std::string_view keyName)
    : _nameWithKey(ToLower(exchangeName)) {
  if (_nameWithKey.find('_') != string::npos) {
    throw exception("Invalid exchange name {}", _nameWithKey);
  }
  if (!keyName.empty()) {
    _nameWithKey.push_back('_');
    _nameWithKey.append(keyName);
  }
}

}  // namespace cct