#include "exchangename.hpp"

#include <cstddef>
#include <string_view>

#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "toupperlower.hpp"

namespace cct {
ExchangeName::ExchangeName(std::string_view globalExchangeName) : _nameWithKey(globalExchangeName) {
  if (globalExchangeName.empty()) {
    throw invalid_argument("Exchange name cannot be empty");
  }
  for (std::size_t charPos = 0, sz = globalExchangeName.size(); charPos < sz && _nameWithKey[charPos] != '_';
       ++charPos) {
    _nameWithKey[charPos] = tolower(_nameWithKey[charPos]);
  }
}

ExchangeName::ExchangeName(std::string_view exchangeName, std::string_view keyName)
    : _nameWithKey(ToLower(exchangeName)) {
  if (_nameWithKey.find('_') != string::npos) {
    throw invalid_argument("Invalid exchange name {}", _nameWithKey);
  }
  if (!keyName.empty()) {
    _nameWithKey.push_back('_');
    _nameWithKey.append(keyName);
  }
}

}  // namespace cct