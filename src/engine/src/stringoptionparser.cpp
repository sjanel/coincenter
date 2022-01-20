#include "stringoptionparser.hpp"

#include <algorithm>

#include "cct_cctype.hpp"
#include "cct_const.hpp"

namespace cct {
namespace {
PublicExchangeNames GetExchanges(std::string_view str) {
  PublicExchangeNames exchanges;
  if (!str.empty()) {
    std::size_t first, last;
    for (first = 0, last = str.find(','); last != std::string_view::npos; last = str.find(',', last + 1)) {
      exchanges.emplace_back(str.begin() + first, str.begin() + last);
      first = last + 1;
    }
    exchanges.emplace_back(str.begin() + first, str.end());
  }
  return exchanges;
}

PrivateExchangeNames GetPrivateExchanges(std::string_view str) {
  PublicExchangeNames fullNames = GetExchanges(str);
  PrivateExchangeNames ret;
  ret.reserve(fullNames.size());
  std::transform(fullNames.begin(), fullNames.end(), std::back_inserter(ret),
                 [](std::string_view exchangeName) { return PrivateExchangeName(exchangeName); });
  return ret;
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
}  // namespace

PublicExchangeNames StringOptionParser::getExchanges() const { return GetExchanges(_opt); }

StringOptionParser::MarketExchanges StringOptionParser::getMarketExchanges() const {
  std::size_t commaPos = getNextCommaPos(0, false);
  std::string_view marketStr(_opt.begin(), commaPos == std::string_view::npos ? _opt.end() : _opt.begin() + commaPos);
  std::size_t dashPos = marketStr.find('-');
  if (dashPos == std::string_view::npos) {
    throw std::invalid_argument("Expected a dash");
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

  return CurrencyPrivateExchanges(CurrencyCode(curStr), GetPrivateExchanges(exchangesStr));
}

StringOptionParser::MonetaryAmountExchanges StringOptionParser::getMonetaryAmountExchanges() const {
  std::size_t commaPos = getNextCommaPos(0, false);
  std::size_t startExchangesPos =
      commaPos == std::string_view::npos ? _opt.size() : _opt.find_first_not_of(' ', commaPos + 1);
  return MonetaryAmountExchanges{MonetaryAmount(StrBeforeComma(_opt, 0, commaPos)),
                                 GetExchanges(StrEnd(_opt, startExchangesPos))};
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
    throw std::invalid_argument("Expected a dash");
  }
  return std::make_tuple(fromTradeCurrency, toTradeCurrency, GetPrivateExchanges(StrEnd(_opt, startExchangesPos)));
}

StringOptionParser::MonetaryAmountCurrencyPrivateExchanges
StringOptionParser::getMonetaryAmountCurrencyPrivateExchanges() const {
  std::size_t dashPos = _opt.find('-');
  if (dashPos == std::string_view::npos) {
    throw std::invalid_argument("Expected a dash");
  }
  if (dashPos == 0) {
    throw std::invalid_argument("Cannot start with a negative amount");
  }
  std::size_t pos = 1;
  while (pos < dashPos && (isdigit(_opt[pos]) || _opt[pos] == '.')) {
    ++pos;
  }
  std::string_view startAmountStr(_opt.data(), pos);
  while (pos < dashPos && isblank(_opt[pos])) {
    ++pos;
  }
  bool isPercentage = pos < dashPos && _opt[pos] == '%';
  if (isPercentage) {
    do {
      ++pos;
    } while (pos < dashPos && isblank(_opt[pos]));
  }
  std::string_view startCurStr(_opt.begin() + pos, _opt.begin() + dashPos);
  MonetaryAmount startAmount(startAmountStr, startCurStr);
  if (isPercentage && startAmount.toNeutral() > MonetaryAmount(100)) {
    throw std::invalid_argument("A percentage cannot be larger than 100");
  }
  std::size_t commaPos = getNextCommaPos(dashPos + 1, false);
  std::size_t startExchangesPos =
      commaPos == std::string_view::npos ? _opt.size() : _opt.find_first_not_of(' ', commaPos + 1);
  CurrencyCode toTradeCurrency(StrBeforeComma(_opt, dashPos + 1, commaPos));
  return std::make_tuple(startAmount, isPercentage, toTradeCurrency,
                         GetPrivateExchanges(StrEnd(_opt, startExchangesPos)));
}

StringOptionParser::MonetaryAmountFromToPrivateExchange StringOptionParser::getMonetaryAmountFromToPrivateExchange()
    const {
  std::size_t commaPos = getNextCommaPos();
  MonetaryAmount amountToWithdraw(std::string_view(_opt.begin(), _opt.begin() + commaPos));
  std::string_view exchangeNames = StrEnd(_opt, commaPos + 1);
  std::size_t dashPos = exchangeNames.find('-');
  if (dashPos == std::string_view::npos) {
    throw std::invalid_argument("Expected a dash");
  }
  return MonetaryAmountFromToPrivateExchange{
      amountToWithdraw, PrivateExchangeName(std::string_view(exchangeNames.begin(), exchangeNames.begin() + dashPos)),
      PrivateExchangeName(StrEnd(exchangeNames, dashPos + 1))};
}

std::size_t StringOptionParser::getNextCommaPos(std::size_t startPos, bool throwIfNone) const {
  std::size_t commaPos = _opt.find(',', startPos);
  if (commaPos == std::string_view::npos && throwIfNone) {
    throw std::invalid_argument("Expected a comma");
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