#include "stringoptionparser.hpp"

#include <algorithm>

namespace cct {
namespace {
PublicExchangeNames GetExchanges(std::string_view str) {
  PublicExchangeNames exchanges;
  std::size_t first, last;
  for (first = 0, last = str.find_first_of(','); last != std::string_view::npos;
       last = str.find_first_of(',', last + 1)) {
    exchanges.emplace_back(str.begin() + first, str.begin() + last);
    first = last + 1;
  }
  exchanges.emplace_back(str.begin() + first, str.end());
  return exchanges;
}
}  // namespace

PublicExchangeNames AnyParser::getExchanges() const { return GetExchanges(_opt); }

PrivateExchangeNames AnyParser::getPrivateExchanges() const {
  PublicExchangeNames fullNames = GetExchanges(_opt);
  PrivateExchangeNames ret;
  ret.reserve(fullNames.size());
  std::transform(fullNames.begin(), fullNames.end(), std::back_inserter(ret),
                 [](const std::string& exchangeName) { return PrivateExchangeName(exchangeName); });
  return ret;
}

AnyParser::MarketExchanges AnyParser::getMarketExchanges() const {
  std::size_t commaPos = getNextCommaPos();
  std::string_view marketStr(_opt.begin(), _opt.begin() + commaPos);
  std::size_t dashPos = marketStr.find_first_of('-');
  if (dashPos == std::string_view::npos) {
    throw std::invalid_argument("Expected a dash");
  }
  return MarketExchanges{
      Market(CurrencyCode(std::string_view(marketStr.begin(), marketStr.begin() + dashPos)),
             CurrencyCode(std::string_view(marketStr.begin() + dashPos + 1, marketStr.end()))),
      GetExchanges(std::string_view(_opt.begin() + _opt.find_first_not_of(' ', commaPos + 1), _opt.end()))};
}

AnyParser::MonetaryAmountExchanges AnyParser::getMonetaryAmountExchanges() const {
  std::size_t commaPos = getNextCommaPos();
  return MonetaryAmountExchanges{
      MonetaryAmount(std::string_view(_opt.begin(), _opt.begin() + commaPos)),
      GetExchanges(std::string_view(_opt.begin() + _opt.find_first_not_of(' ', commaPos + 1), _opt.end()))};
}

AnyParser::MonetaryAmountCurrencyCodePrivateExchange AnyParser::getMonetaryAmountCurrencyCodePrivateExchange() const {
  std::size_t dashPos = _opt.find_first_of('-');
  if (dashPos == std::string::npos) {
    throw std::invalid_argument("Expected a dash");
  }
  MonetaryAmount startTradeAmount(std::string_view(_opt.begin(), _opt.begin() + dashPos));
  std::size_t commaPos = getNextCommaPos(dashPos + 1);
  CurrencyCode toTradeCurrency(std::string_view(_opt.begin() + dashPos + 1, _opt.begin() + commaPos));
  return MonetaryAmountCurrencyCodePrivateExchange{
      startTradeAmount, toTradeCurrency,
      PrivateExchangeName(std::string_view(_opt.begin() + _opt.find_first_not_of(' ', commaPos + 1), _opt.end()))};
}

AnyParser::MonetaryAmountFromToPrivateExchange AnyParser::getMonetaryAmountFromToPrivateExchange() const {
  std::size_t commaPos = getNextCommaPos();
  MonetaryAmount amountToWithdraw(std::string_view(_opt.begin(), _opt.begin() + commaPos));
  std::string_view exchangeNames(_opt.begin() + commaPos + 1, _opt.end());
  std::size_t dashPos = exchangeNames.find_first_of('-');
  if (dashPos == std::string::npos) {
    throw std::invalid_argument("Expected a dash");
  }
  return MonetaryAmountFromToPrivateExchange{
      amountToWithdraw, PrivateExchangeName(std::string_view(exchangeNames.begin(), exchangeNames.begin() + dashPos)),
      PrivateExchangeName(std::string_view(exchangeNames.begin() + dashPos + 1, exchangeNames.end()))};
}

std::size_t AnyParser::getNextCommaPos(std::size_t startPos, bool throwIfNone) const {
  std::size_t commaPos = _opt.find_first_of(',', startPos);
  if (commaPos == std::string::npos && throwIfNone) {
    throw std::invalid_argument("Expected a comma");
  }
  return commaPos;
}

AnyParser::MonetaryAmountFromToPublicExchangeToCurrency AnyParser::getMonetaryAmountFromToPublicExchangeToCurrency()
    const {
  std::size_t beginPos = 0;
  std::size_t commaPos = getNextCommaPos(beginPos);
  MonetaryAmount amountToWithdraw(std::string_view(_opt.begin(), _opt.begin() + commaPos));
  // Source exchange
  beginPos = commaPos + 1;
  commaPos = getNextCommaPos(beginPos);
  PublicExchangeNames exchanges;
  exchanges.push_back(std::string(_opt.begin() + beginPos, _opt.begin() + commaPos));
  // Destination exchange
  beginPos = commaPos + 1;
  commaPos = getNextCommaPos(beginPos);
  exchanges.push_back(std::string(_opt.begin() + beginPos, _opt.begin() + commaPos));

  // (!) No other comma expected
  beginPos = commaPos + 1;
  commaPos = _opt.find_first_of(',', beginPos);
  if (commaPos != std::string::npos) {
    throw std::invalid_argument("Expected a comma");
  }

  // Currency
  CurrencyCode code(std::string_view(_opt.begin() + beginPos, _opt.end()));
  return MonetaryAmountFromToPublicExchangeToCurrency{amountToWithdraw, exchanges, code};
}
}  // namespace cct