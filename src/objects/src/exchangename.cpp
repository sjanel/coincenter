#include "exchangename.hpp"

#include <algorithm>
#include <string_view>
#include <type_traits>

#include "cct_const.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "toupperlower.hpp"

namespace cct {

bool ExchangeName::IsValid(std::string_view str) {
  return std::ranges::any_of(kSupportedExchanges, [lowerStr = ToLower(str)](std::string_view ex) {
    return lowerStr.starts_with(ex) && (lowerStr.size() == ex.size() || lowerStr[ex.size()] == '_');
  });
}

ExchangeName::ExchangeName(std::string_view globalExchangeName) : _nameWithKey(globalExchangeName) {
  if (!IsValid(globalExchangeName)) {
    throw invalid_argument("Invalid exchange name '{}'", globalExchangeName);
  }
  const auto sz = globalExchangeName.size();
  for (std::remove_const_t<decltype(sz)> charPos = 0; charPos < sz && _nameWithKey[charPos] != '_'; ++charPos) {
    _nameWithKey[charPos] = tolower(_nameWithKey[charPos]);
  }
}

ExchangeName::ExchangeName(std::string_view exchangeName, std::string_view keyName)
    : _nameWithKey(ToLower(exchangeName)) {
  if (_nameWithKey.find('_') != string::npos) {
    throw invalid_argument("Invalid exchange name '{}'", _nameWithKey);
  }
  if (!keyName.empty()) {
    _nameWithKey.push_back('_');
    _nameWithKey.append(keyName);
  }
}

}  // namespace cct