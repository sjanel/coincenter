#include "stringoptionparser.hpp"

#include <cstddef>
#include <string_view>
#include <utility>

#include "cct_cctype.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"

namespace cct {
namespace {

template <class CharOrStringType>
std::string_view GetNextStr(std::string_view opt, CharOrStringType sep, std::size_t &pos) {
  auto endPos = opt.size();
  auto begPos = pos;
  while (begPos < endPos && isblank(opt[begPos])) {
    ++begPos;
  }
  endPos = opt.find_first_of(sep, begPos);
  if (endPos == std::string_view::npos) {
    endPos = opt.size();
    pos = endPos;
  } else {
    pos = endPos + 1;
  }
  while (endPos > 0U && isblank(opt[endPos - 1U])) {
    --endPos;
  }
  return {opt.begin() + begPos, opt.begin() + endPos};
}

}  // namespace

// At the end of the currency, either the end of the string or a comma is expected.
CurrencyCode StringOptionParser::parseCurrency(FieldIs fieldIs) {
  const std::size_t commaPos = _opt.find(',', _pos);
  const auto begIt = _opt.begin() + _pos;
  const bool isCommaPresent = commaPos != std::string_view::npos;
  const std::string_view firstStr(begIt, isCommaPresent ? _opt.begin() + commaPos : _opt.end());
  std::string_view curStr;
  if (!firstStr.empty() && !ExchangeName::IsValid(firstStr) && CurrencyCode::IsValid(firstStr)) {
    // disambiguate currency code from exchange name
    curStr = firstStr;
    _pos += curStr.size();
    if (isCommaPresent) {
      ++_pos;
    }
  } else if (fieldIs == FieldIs::kMandatory) {
    throw invalid_argument("Expected a valid currency code in '{}'", std::string_view(_opt.begin() + _pos, _opt.end()));
  }
  return curStr;
}

// At the end of the market, either the end of the string or a comma is expected.
Market StringOptionParser::parseMarket(FieldIs fieldIs) {
  const std::size_t commaPos = _opt.find(',', _pos);
  const auto begIt = _opt.begin() + _pos;
  const bool isCommaPresent = commaPos != std::string_view::npos;
  const std::string_view marketStr(begIt, isCommaPresent ? begIt + commaPos : _opt.end());
  const std::size_t dashPos = marketStr.find('-');
  if (dashPos == std::string_view::npos) {
    if (fieldIs == FieldIs::kMandatory) {
      throw invalid_argument("Expected a dash in '{}'", std::string_view(_opt.begin() + _pos, _opt.end()));
    }
    return {};
  }
  std::string_view firstCur(marketStr.begin(), marketStr.begin() + dashPos);
  if (!CurrencyCode::IsValid(firstCur)) {
    if (fieldIs == FieldIs::kMandatory) {
      throw invalid_argument("Expected a valid first currency in '{}'",
                             std::string_view(_opt.begin() + _pos, _opt.end()));
    }
    return {};
  }
  std::string_view secondCur(marketStr.begin() + dashPos + 1, marketStr.end());
  if (!CurrencyCode::IsValid(secondCur)) {
    if (fieldIs == FieldIs::kMandatory) {
      throw invalid_argument("Expected a valid second currency in '{}'",
                             std::string_view(_opt.begin() + _pos, _opt.end()));
    }
    return {};
  }
  _pos += marketStr.size();
  if (isCommaPresent) {
    ++_pos;
  }
  return {CurrencyCode(firstCur), CurrencyCode(secondCur)};
}

// At the end of the currency, either the end of the string, or a dash or comma is expected.
std::pair<MonetaryAmount, StringOptionParser::AmountType> StringOptionParser::parseNonZeroAmount(FieldIs fieldIs) {
  constexpr std::string_view sepWithPercentageAtLast = "-,%";
  std::size_t originalPos = _pos;
  auto amountStr = GetNextStr(_opt, sepWithPercentageAtLast, _pos);
  std::pair<MonetaryAmount, AmountType> ret{MonetaryAmount(), StringOptionParser::AmountType::kNotPresent};
  if (amountStr.empty()) {
    if (_pos == _opt.size()) {
      if (fieldIs == FieldIs::kMandatory) {
        throw invalid_argument("Expected a non-zero amount in '{}'",
                               std::string_view(_opt.begin() + originalPos, _opt.end()));
      }
      _pos = originalPos;
      return ret;
    }

    // We matched one '-' representing a negative number
    amountStr = GetNextStr(_opt, sepWithPercentageAtLast, _pos);
    amountStr = std::string_view(amountStr.data() - 1U, amountStr.size() + 1U);
  }
  if (ExchangeName::IsValid(amountStr)) {
    if (fieldIs == FieldIs::kMandatory) {
      throw invalid_argument("Expected a non-zero amount in '{}'",
                             std::string_view(_opt.begin() + originalPos, _opt.end()));
    }
    _pos = originalPos;
    return ret;
  }
  MonetaryAmount amount(amountStr, MonetaryAmount::IfNoAmount::kNoThrow);
  if (amount == 0) {
    if (fieldIs == FieldIs::kMandatory) {
      throw invalid_argument("Expected a non-zero amount");
    }
    _pos = originalPos;
    return ret;
  }
  const bool isPercentage = _pos > originalPos && _pos < _opt.size() && _opt[_pos - 1] == '%';
  if (isPercentage) {
    const std::string_view sepWithoutPercentage(sepWithPercentageAtLast.begin(), sepWithPercentageAtLast.end() - 1);
    amount = MonetaryAmount(amount, CurrencyCode(GetNextStr(_opt, sepWithoutPercentage, _pos)));
    if (amount.abs().toNeutral() > MonetaryAmount(100)) {
      throw invalid_argument("Invalid percentage in '{}'", std::string_view(_opt.begin() + originalPos, _opt.end()));
    }
    ret.second = StringOptionParser::AmountType::kPercentage;
  } else {
    ret.second = StringOptionParser::AmountType::kAbsolute;
  }
  ret.first = amount;
  return ret;
}

vector<string> StringOptionParser::getCSVValues() {
  vector<string> ret;
  if (!_opt.empty()) {
    do {
      auto nextCommaPos = _opt.find(',', _pos);
      if (nextCommaPos == std::string_view::npos) {
        nextCommaPos = _opt.size();
      }
      if (_pos != nextCommaPos) {
        ret.emplace_back(_opt.begin() + _pos, _opt.begin() + nextCommaPos);
      }
      if (nextCommaPos == _opt.size()) {
        break;
      }
      _pos = nextCommaPos + 1;
    } while (true);
  }
  return ret;
}

ExchangeNames StringOptionParser::parseExchanges(char sep) {
  std::string_view str(_opt.begin() + _pos, _opt.end());
  ExchangeNames exchanges;
  if (!str.empty()) {
    std::size_t first;
    std::size_t last;
    for (first = 0, last = str.find(sep); last != std::string_view::npos; last = str.find(sep, last + 1)) {
      std::string_view exchangeNameStr(str.begin() + first, str.begin() + last);
      exchanges.emplace_back(exchangeNameStr);
      first = last + 1;
      _pos += exchangeNameStr.size() + 1U;
    }
    // Add the last one as well
    std::string_view exchangeNameStr(str.begin() + first, str.end());
    exchanges.emplace_back(exchangeNameStr);
    _pos += exchangeNameStr.size();
  }
  return exchanges;
}

void StringOptionParser::checkEndParsing() const {
  if (_pos != _opt.size()) {
    throw invalid_argument("{} remaining characters not read", _opt.size() - _pos);
  }
}

}  // namespace cct
