#include "stringoptionparser.hpp"

#include <algorithm>

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
}  // namespace

PublicExchangeNames StringOptionParser::getExchanges() const { return GetExchanges(_opt); }

PrivateExchangeNames StringOptionParser::getPrivateExchanges() const { return GetPrivateExchanges(_opt); }

StringOptionParser::CurrencyPrivateExchanges StringOptionParser::getCurrencyPrivateExchanges() const {
  std::size_t commaPos = getNextCommaPos(0, false);
  std::string_view curStr(_opt.data(), commaPos == string::npos ? _opt.size() : commaPos);
  std::string_view exchangesStr;
  if (commaPos != string::npos) {
    exchangesStr = std::string_view(_opt.data() + commaPos + 1, _opt.data() + _opt.size());
  }
  return CurrencyPrivateExchanges(CurrencyCode(curStr), GetPrivateExchanges(exchangesStr));
}

StringOptionParser::MarketExchanges StringOptionParser::getMarketExchanges() const {
  std::size_t commaPos = getNextCommaPos(0, false);
  std::string_view marketStr(_opt.begin(), commaPos == string::npos ? _opt.end() : _opt.begin() + commaPos);
  std::size_t dashPos = marketStr.find('-');
  if (dashPos == std::string_view::npos) {
    throw std::invalid_argument("Expected a dash");
  }
  std::size_t startExchangesPos = commaPos == string::npos ? _opt.size() : _opt.find_first_not_of(' ', commaPos + 1);

  return MarketExchanges{Market(CurrencyCode(std::string_view(marketStr.begin(), marketStr.begin() + dashPos)),
                                CurrencyCode(std::string_view(marketStr.begin() + dashPos + 1, marketStr.end()))),
                         GetExchanges(std::string_view(_opt.begin() + startExchangesPos, _opt.end()))};
}

StringOptionParser::MonetaryAmountExchanges StringOptionParser::getMonetaryAmountExchanges() const {
  std::size_t commaPos = getNextCommaPos(0, false);
  std::size_t startExchangesPos = commaPos == string::npos ? _opt.size() : _opt.find_first_not_of(' ', commaPos + 1);
  return MonetaryAmountExchanges{
      MonetaryAmount(std::string_view(_opt.begin(), commaPos == string::npos ? _opt.end() : _opt.begin() + commaPos)),
      GetExchanges(std::string_view(_opt.begin() + startExchangesPos, _opt.end()))};
}

StringOptionParser::CurrencyCodeFromToPrivateExchange StringOptionParser::getFromToCurrencyCodePrivateExchange() const {
  std::size_t dashPos = _opt.find('-', 1);
  if (dashPos == string::npos) {
    throw std::invalid_argument("Expected a dash");
  }
  CurrencyCode fromTradeCurrency(std::string_view(_opt.begin(), _opt.begin() + dashPos));
  std::size_t commaPos = getNextCommaPos(dashPos + 1);
  std::size_t startExchangesPos = commaPos == string::npos ? _opt.size() : _opt.find_first_not_of(' ', commaPos + 1);
  CurrencyCode toTradeCurrency(std::string_view(_opt.begin() + dashPos + 1, _opt.begin() + commaPos));
  return std::make_tuple(fromTradeCurrency, toTradeCurrency,
                         PrivateExchangeName(std::string_view(_opt.begin() + startExchangesPos, _opt.end())));
}

StringOptionParser::MonetaryAmountCurrencyCodePrivateExchange
StringOptionParser::getMonetaryAmountCurrencyCodePrivateExchange() const {
  std::size_t dashPos = _opt.find('-', 1);
  if (dashPos == string::npos) {
    throw std::invalid_argument("Expected a dash");
  }
  MonetaryAmount startTradeAmount(std::string_view(_opt.begin(), _opt.begin() + dashPos));
  std::size_t commaPos = getNextCommaPos(dashPos + 1);
  std::size_t startExchangesPos = commaPos == string::npos ? _opt.size() : _opt.find_first_not_of(' ', commaPos + 1);
  CurrencyCode toTradeCurrency(std::string_view(_opt.begin() + dashPos + 1, _opt.begin() + commaPos));
  return std::make_tuple(startTradeAmount, toTradeCurrency,
                         PrivateExchangeName(std::string_view(_opt.begin() + startExchangesPos, _opt.end())));
}

StringOptionParser::MonetaryAmountFromToPrivateExchange StringOptionParser::getMonetaryAmountFromToPrivateExchange()
    const {
  std::size_t commaPos = getNextCommaPos();
  MonetaryAmount amountToWithdraw(std::string_view(_opt.begin(), _opt.begin() + commaPos));
  std::string_view exchangeNames(_opt.begin() + commaPos + 1, _opt.end());
  std::size_t dashPos = exchangeNames.find('-');
  if (dashPos == string::npos) {
    throw std::invalid_argument("Expected a dash");
  }
  return MonetaryAmountFromToPrivateExchange{
      amountToWithdraw, PrivateExchangeName(std::string_view(exchangeNames.begin(), exchangeNames.begin() + dashPos)),
      PrivateExchangeName(std::string_view(exchangeNames.begin() + dashPos + 1, exchangeNames.end()))};
}

std::size_t StringOptionParser::getNextCommaPos(std::size_t startPos, bool throwIfNone) const {
  std::size_t commaPos = _opt.find(',', startPos);
  if (commaPos == string::npos && throwIfNone) {
    throw std::invalid_argument("Expected a comma");
  }
  return commaPos;
}

StringOptionParser::CurrencyCodePublicExchanges StringOptionParser::getCurrencyCodePublicExchanges() const {
  std::size_t commaPos = getNextCommaPos(0, false);
  CurrencyCodePublicExchanges ret;
  if (commaPos == string::npos) {
    ret.first = CurrencyCode(_opt);
  } else {
    ret.first = CurrencyCode(std::string_view(_opt.begin(), _opt.begin() + commaPos));
    ret.second = GetExchanges(std::string_view(_opt.begin() + commaPos + 1, _opt.end()));
  }
  return ret;
}
}  // namespace cct