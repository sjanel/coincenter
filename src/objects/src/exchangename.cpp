#include "exchangename.hpp"

#include <algorithm>
#include <string_view>
#include <type_traits>
#include <utility>

#include "cct_invalid_argument_exception.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "exchange-name-enum.hpp"
#include "toupperlower-string.hpp"
#include "toupperlower.hpp"

namespace cct {

namespace {

std::pair<ExchangeNameEnum, bool> ExtractExchangeNameEnum(std::string_view str) {
  static constexpr auto kMinExchangeNameLength =
      std::ranges::min_element(kSupportedExchanges, [](auto lhs, auto rhs) { return lhs.size() < rhs.size(); })->size();

  std::pair<ExchangeNameEnum, bool> res{};
  if (str.size() < kMinExchangeNameLength) {
    return res;
  }
  const auto lowerStr = ToLower(str);
  const auto exchangePos =
      std::ranges::find_if(kSupportedExchanges, [&lowerStr](std::string_view ex) { return lowerStr.starts_with(ex); }) -
      std::begin(kSupportedExchanges);
  if (exchangePos == kNbSupportedExchanges) {
    return res;
  }
  const auto publicExchangeName = kSupportedExchanges[exchangePos];
  res.first = static_cast<ExchangeNameEnum>(exchangePos);
  if (publicExchangeName.size() == lowerStr.size()) {
    res.second = true;
    return res;
  }
  if (lowerStr[publicExchangeName.size()] != '_') {
    return res;
  }
  res.second = str.size() > publicExchangeName.size() + 1U;
  return res;
}
}  // namespace

bool ExchangeName::IsValid(std::string_view str) { return ExtractExchangeNameEnum(str).second; }

ExchangeName::ExchangeName(std::string_view globalExchangeName) : _nameWithKey(globalExchangeName) {
  const auto [exchangeNameEnum, isValid] = ExtractExchangeNameEnum(globalExchangeName);
  if (!isValid) {
    throw invalid_argument("Invalid exchange name '{}'", globalExchangeName);
  }
  _exchangeNameEnum = exchangeNameEnum;
  const auto exchangeNameSize = EnumToString(exchangeNameEnum).size();
  if (_nameWithKey.size() > exchangeNameSize) {
    _begKeyNamePos = exchangeNameSize + 1UL;
  } else {
    _begKeyNamePos = kUndefinedKeyNamePos;
  }

  for (std::remove_const_t<decltype(exchangeNameSize)> charPos = 0; charPos < exchangeNameSize; ++charPos) {
    _nameWithKey[charPos] = tolower(_nameWithKey[charPos]);
  }
}

ExchangeName::ExchangeName(ExchangeNameEnum exchangeNameEnum, std::string_view keyName)
    : _exchangeNameEnum(exchangeNameEnum), _nameWithKey(EnumToString(exchangeNameEnum)) {
  if (keyName.empty()) {
    _begKeyNamePos = kUndefinedKeyNamePos;
  } else {
    _begKeyNamePos = _nameWithKey.size() + 1UL;
    _nameWithKey.push_back('_');
    _nameWithKey.append(keyName);
  }
}

}  // namespace cct