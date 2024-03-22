#include "exchangename.hpp"

#include <algorithm>
#include <string_view>
#include <type_traits>

#include "cct_const.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "toupperlower-string.hpp"
#include "toupperlower.hpp"

namespace cct {

bool ExchangeName::IsValid(std::string_view str) {
  if (str.size() < kMinExchangeNameLength) {
    return false;
  }
  const auto lowerStr = ToLower(str);
  const auto exchangePos =
      std::ranges::find_if(kSupportedExchanges, [&lowerStr](std::string_view ex) { return lowerStr.starts_with(ex); }) -
      std::begin(kSupportedExchanges);
  if (exchangePos == kNbSupportedExchanges) {
    return false;
  }
  const auto publicExchangeName = kSupportedExchanges[exchangePos];
  if (publicExchangeName.size() == lowerStr.size()) {
    return true;
  }
  if (lowerStr[publicExchangeName.size()] != '_') {
    return false;
  }
  std::string_view keyName(lowerStr.begin() + publicExchangeName.size() + 1U, lowerStr.end());
  return !keyName.empty();
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
  if (std::ranges::find_if(kSupportedExchanges, [this](const auto exchangeStr) {
        return exchangeStr == this->_nameWithKey;
      }) == std::end(kSupportedExchanges)) {
    throw invalid_argument("Invalid exchange name '{}'", exchangeName);
  }
  if (!keyName.empty()) {
    _nameWithKey.push_back('_');
    _nameWithKey.append(keyName);
  }
}

}  // namespace cct