#include "stringoptionparser.hpp"

#include <algorithm>

#include "cct_cctype.hpp"
#include "cct_const.hpp"
#include "cct_invalid_argument_exception.hpp"

namespace cct {
namespace {
ExchangeNames GetExchanges(std::string_view str) {
  ExchangeNames exchanges;
  if (!str.empty()) {
    std::size_t first, last;
    for (first = 0, last = str.find(','); last != std::string_view::npos; last = str.find(',', last + 1)) {
      exchanges.emplace_back(std::string_view(str.begin() + first, str.begin() + last));
      first = last + 1;
    }
    exchanges.emplace_back(std::string_view(str.begin() + first, str.end()));
  }
  return exchanges;
}

std::string_view StrBeforeComma(std::string_view opt, std::size_t startPos, std::size_t commaPos) {
  return std::string_view(opt.begin() + startPos,
                          commaPos == std::string_view::npos ? opt.end() : opt.begin() + commaPos);
}

std::string_view StrEnd(std::string_view opt, std::size_t startPos) {
  return std::string_view(opt.begin() + startPos, opt.end());
}

bool IsExchangeName(std::string_view str) {
  string lowerStr = ToLower(str);
  return std::ranges::any_of(kSupportedExchanges, [&lowerStr](std::string_view ex) {
    return lowerStr.starts_with(ex) && (lowerStr.size() == ex.size() || lowerStr[ex.size()] == '_');
  });
}

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
  return std::string_view(opt.begin() + begPos, opt.begin() + endPos);
}

auto GetNextPercentageAmount(std::string_view opt, std::string_view sepWithPercentageAtLast, std::size_t &pos) {
  std::string_view amountStr = GetNextStr(opt, sepWithPercentageAtLast, pos);

  if (amountStr.empty()) {
    if (pos == opt.size()) {
      throw invalid_argument("Expected a start amount");
    }
    // We matched one '-' representing a negative number
    amountStr = GetNextStr(opt, sepWithPercentageAtLast, pos);
    amountStr = std::string_view(amountStr.data() - 1U, amountStr.size() + 1U);
  }
  MonetaryAmount startAmount(amountStr);
  bool isPercentage = pos > 0U && pos < opt.size() && opt[pos - 1] == '%';
  if (isPercentage) {
    assert(sepWithPercentageAtLast.back() == '%');
    std::string_view sepWithoutPercentage(sepWithPercentageAtLast.begin(), sepWithPercentageAtLast.end() - 1);
    startAmount = MonetaryAmount(startAmount, CurrencyCode(GetNextStr(opt, sepWithoutPercentage, pos)));
    if (startAmount.abs().toNeutral() > MonetaryAmount(100)) {
      throw invalid_argument("A percentage cannot be larger than 100");
    }
  }
  return std::make_pair(std::move(startAmount), isPercentage);
}

}  // namespace

ExchangeNames StringOptionParser::getExchanges() const { return GetExchanges(_opt); }

StringOptionParser::MarketExchanges StringOptionParser::getMarketExchanges() const {
  std::size_t commaPos = getNextCommaPos(0, false);
  std::string_view marketStr(_opt.begin(), commaPos == std::string_view::npos ? _opt.end() : _opt.begin() + commaPos);
  std::size_t dashPos = marketStr.find('-');
  if (dashPos == std::string_view::npos) {
    throw invalid_argument("Expected a dash");
  }
  std::size_t startExchangesPos =
      commaPos == std::string_view::npos ? _opt.size() : _opt.find_first_not_of(' ', commaPos + 1);

  return MarketExchanges{Market(CurrencyCode(std::string_view(marketStr.begin(), marketStr.begin() + dashPos)),
                                CurrencyCode(std::string_view(marketStr.begin() + dashPos + 1, marketStr.end()))),
                         GetExchanges(StrEnd(_opt, startExchangesPos))};
}

StringOptionParser::CurrencyPrivateExchanges StringOptionParser::getCurrencyPrivateExchanges() const {
  std::string_view exchangesStr = _opt;
  std::string_view curStr;
  std::size_t commaPos = getNextCommaPos(0, false);
  std::string_view firstStr(_opt.data(), commaPos == std::string_view::npos ? _opt.size() : commaPos);
  if (!firstStr.empty() && !IsExchangeName(firstStr)) {
    curStr = firstStr;
    if (firstStr.size() == _opt.size()) {
      exchangesStr = std::string_view();
    } else {
      exchangesStr = std::string_view(_opt.begin() + commaPos + 1, _opt.end());
    }
  }

  return CurrencyPrivateExchanges(CurrencyCode(curStr), GetExchanges(exchangesStr));
}

StringOptionParser::CurrenciesPrivateExchanges StringOptionParser::getCurrenciesPrivateExchanges(
    bool currenciesShouldBeSet) const {
  std::size_t dashPos = _opt.find('-', 1);
  std::size_t commaPos = getNextCommaPos(dashPos == std::string_view::npos ? 0 : dashPos + 1, false);
  CurrencyCode fromTradeCurrency, toTradeCurrency;
  std::size_t startExchangesPos = 0;
  if (dashPos == std::string_view::npos && commaPos == std::string_view::npos && !_opt.empty()) {
    // Ambiguity to resolve - we assume there is no crypto acronym with the same name as an exchange
    if (!IsExchangeName(_opt)) {
      fromTradeCurrency = CurrencyCode(_opt);
      startExchangesPos = _opt.size();
    }
  } else {
    // no ambiguity
    if (commaPos == std::string_view::npos) {
      commaPos = _opt.size();
      startExchangesPos = commaPos;
    } else {
      startExchangesPos = commaPos + 1;
    }
    if (dashPos == std::string_view::npos) {
      fromTradeCurrency = CurrencyCode(std::string_view(_opt.data(), commaPos));
    } else {
      fromTradeCurrency = CurrencyCode(std::string_view(_opt.data(), dashPos));
      toTradeCurrency = CurrencyCode(StrBeforeComma(_opt, dashPos + 1, commaPos));
    }
  }
  if (currenciesShouldBeSet && (fromTradeCurrency.isNeutral() || toTradeCurrency.isNeutral())) {
    throw invalid_argument("Expected a dash");
  }
  return std::make_tuple(fromTradeCurrency, toTradeCurrency, GetExchanges(StrEnd(_opt, startExchangesPos)));
}

StringOptionParser::MonetaryAmountCurrencyPrivateExchanges
StringOptionParser::getMonetaryAmountCurrencyPrivateExchanges(bool withCurrency) const {
  std::size_t pos = 0;
  auto [startAmount, isPercentage] = GetNextPercentageAmount(_opt, "-,%", pos);
  CurrencyCode toTradeCurrency;
  if (withCurrency) {
    toTradeCurrency = CurrencyCode(GetNextStr(_opt, ',', pos));
  }

  return std::make_tuple(startAmount, isPercentage, toTradeCurrency, GetExchanges(GetNextStr(_opt, '\0', pos)));
}

StringOptionParser::CurrencyFromToPrivateExchange StringOptionParser::getCurrencyFromToPrivateExchange() const {
  std::size_t pos = 0;
  CurrencyCode cur(GetNextStr(_opt, ',', pos));
  ExchangeName from(GetNextStr(_opt, '-', pos));
  // Warning: in C++, order of evaluation of parameters is unspecified. Because GetNextStr has side
  // effects (it modifies 'pos') we need temporary variables here
  return std::make_tuple(std::move(cur), std::move(from), ExchangeName(GetNextStr(_opt, '-', pos)));
}

StringOptionParser::MonetaryAmountFromToPrivateExchange StringOptionParser::getMonetaryAmountFromToPrivateExchange()
    const {
  std::size_t pos = 0;
  auto [startAmount, isPercentage] = GetNextPercentageAmount(_opt, ",%", pos);
  ExchangeName from(GetNextStr(_opt, '-', pos));
  return std::make_tuple(std::move(startAmount), isPercentage, std::move(from),
                         ExchangeName(GetNextStr(_opt, '-', pos)));
}

std::size_t StringOptionParser::getNextCommaPos(std::size_t startPos, bool throwIfNone) const {
  std::size_t commaPos = _opt.find(',', startPos);
  if (throwIfNone && commaPos == std::string_view::npos) {
    throw invalid_argument("Expected a comma");
  }
  return commaPos;
}

StringOptionParser::CurrencyPublicExchanges StringOptionParser::getCurrencyPublicExchanges() const {
  std::size_t firstCommaPos = getNextCommaPos(0, false);
  CurrencyPublicExchanges ret;
  if (firstCommaPos == std::string_view::npos) {
    ret.first = CurrencyCode(_opt);
  } else {
    ret.first = CurrencyCode(std::string_view(_opt.begin(), _opt.begin() + firstCommaPos));
    ret.second = GetExchanges(StrEnd(_opt, firstCommaPos + 1));
  }
  return ret;
}

StringOptionParser::CurrenciesPublicExchanges StringOptionParser::getCurrenciesPublicExchanges() const {
  std::size_t firstCommaPos = getNextCommaPos(0, false);
  std::size_t dashPos = _opt.find('-', 1);
  CurrenciesPublicExchanges ret;
  if (firstCommaPos == std::string_view::npos) {
    firstCommaPos = _opt.size();
  } else {
    std::get<2>(ret) = GetExchanges(StrEnd(_opt, firstCommaPos + 1));
  }
  if (dashPos == std::string_view::npos) {
    std::get<0>(ret) = CurrencyCode(std::string_view(_opt.data(), firstCommaPos));
  } else {
    std::get<0>(ret) = CurrencyCode(std::string_view(_opt.data(), dashPos));
    std::get<1>(ret) = CurrencyCode(std::string_view(_opt.begin() + dashPos + 1, _opt.begin() + firstCommaPos));
  }
  return ret;
}

vector<std::string_view> StringOptionParser::getCSVValues() const {
  std::size_t pos = 0;
  vector<std::string_view> ret;
  if (!_opt.empty()) {
    do {
      std::size_t nextCommaPos = getNextCommaPos(pos, false);
      if (nextCommaPos == std::string_view::npos) {
        nextCommaPos = _opt.size();
      }
      if (pos != nextCommaPos) {
        ret.emplace_back(_opt.begin() + pos, _opt.begin() + nextCommaPos);
      }
      if (nextCommaPos == _opt.size()) {
        break;
      }
      pos = nextCommaPos + 1;
    } while (true);
  }
  return ret;
}

}  // namespace cct