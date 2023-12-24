#include "stringoptionparser.hpp"

#include <cstddef>
#include <string_view>
#include <utility>

#include "cct_cctype.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "durationstring.hpp"
#include "exchange-names.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {

// At the end of the currency, either the end of the string or a comma is expected.
CurrencyCode StringOptionParser::parseCurrency(FieldIs fieldIs, char delimiter) {
  const auto delimiterPos = _opt.find(delimiter, _pos);
  const auto begIt = _opt.begin() + _pos;
  const bool isDelimiterPresent = delimiterPos != std::string_view::npos;
  const std::string_view tokenStr(begIt, isDelimiterPresent ? _opt.begin() + delimiterPos : _opt.end());

  if (!tokenStr.empty() && !ExchangeName::IsValid(tokenStr) && CurrencyCode::IsValid(tokenStr)) {
    // disambiguate currency code from exchange name
    _pos += tokenStr.size();
    if (isDelimiterPresent) {
      ++_pos;
    }
    return tokenStr;
  }
  if (fieldIs == FieldIs::kMandatory) {
    throw invalid_argument("Expected a valid currency code in '{}'", std::string_view(_opt.begin() + _pos, _opt.end()));
  }

  return {};
}

Duration StringOptionParser::parseDuration(FieldIs fieldIs) {
  auto dur = kUndefinedDuration;
  const std::string_view currentToken(_opt.begin() + _pos, _opt.end());
  const auto durationLen = DurationLen(currentToken);
  if (durationLen > 0) {
    const std::string_view durationStr(_opt.data() + _pos, static_cast<std::string_view::size_type>(durationLen));

    dur = ParseDuration(durationStr);
  } else if (fieldIs == FieldIs::kMandatory) {
    throw invalid_argument("Expected a valid duration in '{}'", currentToken);
  }

  _pos += durationLen;

  return dur;
}

// At the end of the market, either the end of the string or a comma is expected.
Market StringOptionParser::parseMarket(FieldIs fieldIs, char delimiter) {
  const auto oldPos = _pos;

  CurrencyCode firstCur = parseCurrency(fieldIs, '-');
  CurrencyCode secondCur;

  if (firstCur.isDefined()) {
    secondCur = parseCurrency(fieldIs, delimiter);
    if (secondCur.isNeutral()) {
      firstCur = CurrencyCode();
      _pos = oldPos;
    }
  }

  return {firstCur, secondCur};
}

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

// At the end of the currency, either the end of the string, or a dash or comma is expected.
std::pair<MonetaryAmount, StringOptionParser::AmountType> StringOptionParser::parseNonZeroAmount(FieldIs fieldIs) {
  static constexpr std::string_view sepWithPercentageAtLast = "-,%";
  const auto originalPos = _pos;
  auto amountStr = GetNextStr(_opt, sepWithPercentageAtLast, _pos);
  auto ret = std::make_pair(MonetaryAmount(), StringOptionParser::AmountType::kNotPresent);
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
    while (true) {
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
    }
  }
  return ret;
}

ExchangeNames StringOptionParser::parseExchanges(char exchangesSep, char endExchangesSep) {
  if (exchangesSep == endExchangesSep) {
    throw invalid_argument("Exchanges separator cannot be the same as end exchanges separator");
  }
  auto endPos = _opt.find(endExchangesSep, _pos);
  if (endPos == std::string_view::npos) {
    endPos = _opt.size();
  }
  std::string_view str(_opt.begin() + _pos, _opt.begin() + endPos);
  ExchangeNames exchanges;
  if (!str.empty()) {
    auto last = str.find(exchangesSep);
    decltype(last) first = 0;
    for (; last != std::string_view::npos; last = str.find(exchangesSep, last + 1)) {
      std::string_view exchangeNameStr(str.begin() + first, str.begin() + last);
      if (!exchangeNameStr.empty()) {
        if (!ExchangeName::IsValid(exchangeNameStr)) {
          return exchanges;
        }
        exchanges.emplace_back(exchangeNameStr);
      }
      first = last + 1;
      _pos += exchangeNameStr.size() + 1U;
    }
    // Add the last one as well, if it is an exchange name
    std::string_view exchangeNameStr(str.begin() + first, str.end());
    if (!ExchangeName::IsValid(exchangeNameStr)) {
      return exchanges;
    }
    exchanges.emplace_back(exchangeNameStr);
    _pos += exchangeNameStr.size();
    if (_pos < _opt.size() && _opt[_pos] == endExchangesSep) {
      ++_pos;
    }
  }
  return exchanges;
}

void StringOptionParser::checkEndParsing() const {
  if (_pos != _opt.size()) {
    throw invalid_argument("{} remaining characters not read", _opt.size() - _pos);
  }
}

}  // namespace cct
