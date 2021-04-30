#include "currencyexchange.hpp"

namespace cct {

CurrencyExchange::CurrencyExchange(CurrencyCode standardCode, CurrencyCode exchangeCode, CurrencyCode altCode,
                                   Deposit deposit, Withdraw withdraw)
    : _standardCode(standardCode),
      _exchangeCode(exchangeCode),
      _altCode(altCode),
      _canDeposit(deposit == Deposit::kAvailable),
      _canWithdraw(withdraw == Withdraw::kAvailable) {}

std::string CurrencyExchange::str() const {
  std::string ret(_standardCode.str());
  ret.push_back('-');
  ret.push_back('D');
  ret.push_back(_canDeposit ? '1' : '0');
  ret.push_back('W');
  ret.push_back(_canWithdraw ? '1' : '0');
  ret.push_back('-');
  if (_exchangeCode != _standardCode || _altCode != _standardCode) {
    ret.push_back('(');
    if (_exchangeCode != _standardCode) {
      ret.append(_exchangeCode.str());
    }
    if (_altCode != _standardCode) {
      if (_exchangeCode != _standardCode) {
        ret.push_back(',');
      }
      ret.append(_altCode.str());
    }
    ret.push_back(')');
  }
  return ret;
}

std::ostream &operator<<(std::ostream &os, const CurrencyExchange &c) {
  os << c.str();
  return os;
}

}  // namespace cct