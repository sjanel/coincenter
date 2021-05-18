#include "monetaryamount.hpp"

#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <sstream>

#include "cct_config.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"

namespace cct {
namespace {

/// Source: https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html
/// Theorem 15
constexpr int kNbMaxDoubleDecimals = std::numeric_limits<double>::max_digits10;

/// Converts a string into a fixed precision integral containing both the integer and decimal part.
/// @param amountStr the string to convert
/// @param heuristicRoundingFromDouble if true, more than 5 consecutive zeros or 9 in the decimals part will be rounded
std::pair<MonetaryAmount::AmountType, int8_t> AmountIntegralFromStr(std::string_view amountStr,
                                                                    bool heuristicRoundingFromDouble = false) {
  bool isNeg = false;
  if (!amountStr.empty()) {
    char firstChar = amountStr.front();
    if (firstChar == '-') {
      isNeg = true;
      amountStr.remove_prefix(1);
    } else if (firstChar != '.' && (firstChar < '0' || firstChar > '9')) {
      throw exception("Parsing error, unexpected first char " + std::string(1, firstChar));
    }
  }
  std::size_t dotPos = amountStr.find_first_of('.');
  int8_t nbDecimals = 0;
  MonetaryAmount::AmountType roundingUpNinesDouble = 0;
  MonetaryAmount::AmountType decPart = 0, integerPart = 0;
  if (dotPos != std::string::npos) {
    while (amountStr.back() == '0') {
      amountStr.remove_suffix(1);
    }
    if (heuristicRoundingFromDouble && (amountStr.size() - dotPos - 1) == kNbMaxDoubleDecimals) {
      std::size_t bestFindPos = 0;
      for (std::string_view pattern : {"000", "999"}) {
        std::size_t findPos = amountStr.rfind(pattern);
        if (findPos != std::string::npos && findPos > dotPos) {
          while (amountStr[findPos - 1] == pattern.front()) {
            --findPos;
          }
          if (amountStr[findPos - 1] == '.') {
            continue;
          }
          bestFindPos = std::max(bestFindPos, findPos);
        }
      }
      if (bestFindPos != 0) {
        const bool roundingUp = amountStr[bestFindPos] == '9';
        log::trace("Heuristic rounding {} for {}", roundingUp ? "up" : "down", amountStr);
        amountStr.remove_suffix(amountStr.size() - bestFindPos);
        if (roundingUp) {
          roundingUpNinesDouble = 1;
        }
      }
    }
    nbDecimals = static_cast<int8_t>(amountStr.size() - dotPos - 1);
    // dotPos is still valid as we erased only past elements
    if (amountStr.size() > std::numeric_limits<MonetaryAmount::AmountType>::digits10 + 1) {
      int8_t nbDigitsToRemove =
          static_cast<int8_t>(amountStr.size() - std::numeric_limits<MonetaryAmount::AmountType>::digits10 - 1);
      if (nbDigitsToRemove > nbDecimals) {
        throw exception("Received amount string " + std::string(amountStr) + " whose integral part is too big");
      }
      log::trace("Received amount string '{}' too big for MonetaryAmount, truncating {} digits", amountStr,
                 nbDigitsToRemove);
      amountStr.remove_suffix(nbDigitsToRemove);
      nbDecimals -= nbDigitsToRemove;
    }
    std::from_chars(amountStr.data() + dotPos + 1, amountStr.data() + amountStr.size(), decPart);
    std::from_chars(amountStr.data(), amountStr.data() + dotPos, integerPart);
  } else {
    std::from_chars(amountStr.data(), amountStr.data() + amountStr.size(), integerPart);
  }

  MonetaryAmount::AmountType integralAmount = integerPart * cct::ipow(10, nbDecimals) + decPart + roundingUpNinesDouble;
  if (isNeg) {
    integralAmount *= -1;
  }
  return {integralAmount, nbDecimals};
}

}  // namespace

MonetaryAmount::MonetaryAmount(std::string_view amountCurrencyStr) {
  assert(!amountCurrencyStr.empty());
  auto last = amountCurrencyStr.begin() + 1;  // skipping optional '-'
  auto endIt = amountCurrencyStr.end();
  while (last != endIt && ((*last >= '0' && *last <= '9') || *last == '.')) {
    ++last;
  }
  std::tie(_amount, _nbDecimals) = AmountIntegralFromStr(std::string_view(amountCurrencyStr.begin(), last));
  while (last != endIt && *last == ' ') {
    ++last;
  }
  _currencyCode = CurrencyCode(std::string_view(last, endIt));
  assert(isSane());
}

MonetaryAmount::MonetaryAmount(std::string_view amountStr, CurrencyCode currencyCode) : _currencyCode(currencyCode) {
  std::tie(_amount, _nbDecimals) = AmountIntegralFromStr(amountStr);
  assert(isSane());
}

MonetaryAmount::MonetaryAmount(double amount, CurrencyCode currencyCode) : _currencyCode(currencyCode) {
  std::stringstream amtBuf;
  amtBuf << std::setprecision(kNbMaxDoubleDecimals) << std::fixed << amount;
  std::tie(_amount, _nbDecimals) = AmountIntegralFromStr(amtBuf.str(), true);
  assert(isSane());
}

std::optional<MonetaryAmount::AmountType> MonetaryAmount::amount(int8_t nbDecimals) const {
  assert(nbDecimals >= 0);
  AmountType integralAmount = _amount;
  for (; nbDecimals < _nbDecimals; ++nbDecimals) {
    integralAmount /= 10;
  }
  for (; _nbDecimals < nbDecimals; --nbDecimals) {
    if (CCT_UNLIKELY(integralAmount > std::numeric_limits<AmountType>::max() / 10 ||
                     integralAmount < std::numeric_limits<AmountType>::min() / 10)) {
      return std::nullopt;
    }
    integralAmount *= 10;
  }
  return integralAmount;
}

namespace {

inline int8_t SafeConvertSameDecimals(MonetaryAmount::AmountType &lhsAmount, MonetaryAmount::AmountType &rhsAmount,
                                      int8_t lhsNbDecimals, int8_t rhsNbDecimals) {
  int lhsNbDigits = ndigits(lhsAmount);
  int rhsNbDigits = ndigits(rhsAmount);
  while (lhsNbDecimals != rhsNbDecimals) {
    if (lhsNbDecimals < rhsNbDecimals) {
      if (lhsNbDigits < std::numeric_limits<MonetaryAmount::AmountType>::digits10) {
        ++lhsNbDecimals;
        ++lhsNbDigits;
        lhsAmount *= 10;
      } else {
        log::trace("Reaching numeric limits of MonetaryAmount for {} & {}, truncate", lhsAmount, rhsAmount);
        --rhsNbDecimals;
        --rhsNbDigits;
        rhsAmount /= 10;
      }
    } else {
      if (rhsNbDigits < std::numeric_limits<MonetaryAmount::AmountType>::digits10) {
        ++rhsNbDecimals;
        ++rhsNbDigits;
        rhsAmount *= 10;
      } else {
        log::trace("Reaching numeric limits of MonetaryAmount for {} & {}, truncate", lhsAmount, rhsAmount);
        --lhsNbDecimals;
        --lhsNbDigits;
        lhsAmount /= 10;
      }
    }
  }
  return lhsNbDecimals;
}
}  // namespace

MonetaryAmount MonetaryAmount::round(MonetaryAmount step, RoundType roundType) const {
  AmountType lhsAmount = _amount;
  AmountType rhsAmount = step._amount;
  assert(rhsAmount != 0);
  int8_t resNbDecimals = SafeConvertSameDecimals(lhsAmount, rhsAmount, _nbDecimals, step._nbDecimals);
  AmountType epsilon = lhsAmount % rhsAmount;
  if (lhsAmount < 0) {
    roundType = roundType == RoundType::kDown ? RoundType::kUp : RoundType::kDown;
  }
  AmountType resAmount =
      roundType == RoundType::kDown || epsilon == 0 ? lhsAmount - epsilon : lhsAmount + (rhsAmount - epsilon);
  MonetaryAmount ret(resAmount, _currencyCode, resNbDecimals);
  ret.simplify();
  assert(ret.isSane());
  return ret;
}

bool MonetaryAmount::operator<(MonetaryAmount o) const {
  if (CCT_UNLIKELY(_currencyCode != o._currencyCode)) {
    throw exception("Cannot compare amounts with different currency");
  }
  if (_nbDecimals == o._nbDecimals) {
    return _amount < o._amount;
  }
  AmountType lhsIntAmount = integerPart();
  AmountType rhsIntAmount = o.integerPart();
  if (lhsIntAmount < rhsIntAmount) {
    return true;
  }
  if (lhsIntAmount > rhsIntAmount) {
    return false;
  }
  // Same integral part, so expanding one's number of decimals towards the other one is safe
  auto adjustDecimals = [](AmountType lhsAmount, AmountType rhsAmount, int8_t lhsNbD,
                           int8_t rhsNbD) -> std::pair<AmountType, AmountType> {
    for (int8_t nbD = lhsNbD; nbD < rhsNbD; ++nbD) {
      assert(lhsAmount <= (std::numeric_limits<AmountType>::max() / 10) &&
             (lhsAmount >= (std::numeric_limits<AmountType>::min() / 10)));
      lhsAmount *= 10;
    }
    for (int8_t nbD = rhsNbD; nbD < lhsNbD; ++nbD) {
      assert(rhsAmount <= (std::numeric_limits<AmountType>::max() / 10) &&
             (rhsAmount >= (std::numeric_limits<AmountType>::min() / 10)));
      rhsAmount *= 10;
    }
    return {lhsAmount, rhsAmount};
  };
  auto amounts = adjustDecimals(_amount, o._amount, _nbDecimals, o._nbDecimals);
  return amounts.first < amounts.second;
}

bool MonetaryAmount::operator==(MonetaryAmount o) const {
  return _amount == o._amount && _nbDecimals == o._nbDecimals && _currencyCode == o._currencyCode;
}

MonetaryAmount MonetaryAmount::operator+(MonetaryAmount o) const {
  if (CCT_UNLIKELY(_currencyCode != o._currencyCode)) {
    throw exception("Addition is only possible on amounts with same currency");
  }
  AmountType lhsAmount = _amount;
  AmountType rhsAmount = o._amount;
  int8_t resNbDecimals = SafeConvertSameDecimals(lhsAmount, rhsAmount, _nbDecimals, o._nbDecimals);
  AmountType resAmount = lhsAmount + rhsAmount;
  constexpr AmountType kMaxAmountFullNDigits = ipow(10, std::numeric_limits<AmountType>::digits10);
  if (resAmount >= kMaxAmountFullNDigits || resAmount <= -kMaxAmountFullNDigits) {
    resAmount /= 10;
    --resNbDecimals;
  }
  MonetaryAmount ret(resAmount, _currencyCode, resNbDecimals);
  ret.simplify();
  assert(ret.isSane());
  return ret;
}

MonetaryAmount MonetaryAmount::operator*(AmountType mult) const {
  AmountType amount = _amount;
  int8_t nbDecimals = _nbDecimals;
  if (mult < -1 || mult > 1) {  // for * -1, * 0 and * -1 result is trivial without overflow
    // Beware of overflows, they can come faster than we think with multiplications.
    int nbDigitsMult = ndigits(mult);
    int nbDigitsAmount = ndigits(_amount);
    int nbDigitsToTruncate = nbDigitsAmount + nbDigitsMult - std::numeric_limits<AmountType>::digits10;
    if (nbDigitsToTruncate > 0) {
      log::trace("Reaching numeric limits of MonetaryAmount for {} * {}, truncate {} digits", _amount, mult,
                 nbDigitsToTruncate);
      if (nbDecimals >= nbDigitsToTruncate) {
        while (nbDigitsToTruncate > 0) {
          --nbDecimals;
          amount /= 10;
          --nbDigitsToTruncate;
        }
      } else {
        log::warn("Cannot truncate decimal part, I need to truncate integral part");
        while (nbDigitsToTruncate > 0) {
          amount /= 10;
          --nbDigitsToTruncate;
        }
      }
    }
  }
  MonetaryAmount ret(amount * mult, _currencyCode, nbDecimals);
  ret.simplify();
  assert(ret.isSane());
  return ret;
}

MonetaryAmount MonetaryAmount::operator*(MonetaryAmount mult) const {
  AmountType lhsAmount = _amount;
  AmountType rhsAmount = mult._amount;
  int8_t lhsNbDecimals = _nbDecimals;
  int8_t rhsNbDecimals = mult._nbDecimals;
  int lhsNbDigits = ndigits(_amount);
  int rhsNbDigits = ndigits(mult._amount);
  CurrencyCode resCurrency = _currencyCode.isNeutral() ? mult.currencyCode() : _currencyCode;
  if (CCT_UNLIKELY(!_currencyCode.isNeutral() && !mult.currencyCode().isNeutral())) {
    throw exception("Cannot multiply two non neutral MonetaryAmounts");
  }
  if (lhsNbDigits + rhsNbDigits > std::numeric_limits<AmountType>::digits10) {
    log::trace("Reaching numeric limits of MonetaryAmount for {} * {}, truncate", _amount, mult._amount);
  }
  while (lhsNbDigits + rhsNbDigits > std::numeric_limits<AmountType>::digits10) {
    // We need to truncate, choose the MonetaryAmount with the highest number of decimals in priority
    if (rhsNbDecimals == lhsNbDecimals && lhsNbDecimals == 0) {
      log::warn("Cannot truncate decimal part, I need to truncate integral part");
      if (lhsNbDigits < rhsNbDigits) {
        --rhsNbDigits;
        rhsAmount /= 10;
      } else {
        --lhsNbDigits;
        lhsAmount /= 10;
      }
    } else {
      if (lhsAmount % 10 == 0 || (rhsAmount % 10 != 0 && rhsNbDecimals < lhsNbDecimals)) {
        // Truncate from Lhs
        --lhsNbDecimals;
        --lhsNbDigits;
        lhsAmount /= 10;
      } else {
        // Truncate from Rhs
        --rhsNbDecimals;
        --rhsNbDigits;
        rhsAmount /= 10;
      }
    }
  }
  MonetaryAmount ret(lhsAmount * rhsAmount, resCurrency, lhsNbDecimals + rhsNbDecimals);
  ret.sanitizeNbDecimals();
  ret.simplify();
  assert(ret.isSane());
  return ret;
}

MonetaryAmount MonetaryAmount::operator/(MonetaryAmount div) const {
  AmountType lhsAmount = _amount;
  AmountType rhsAmount = div._amount;
  assert(rhsAmount != 0);
  int8_t lhsNbDecimals = _nbDecimals;
  int8_t rhsNbDecimals = div._nbDecimals;
  int8_t lhsNbDigits = static_cast<int8_t>(ndigits(_amount));
  const int negMult = ((lhsAmount < 0 && rhsAmount > 0) || (lhsAmount > 0 && rhsAmount < 0)) ? -1 : 1;
  UnsignedAmountType lhs = std::abs(lhsAmount);
  UnsignedAmountType rhs = std::abs(rhsAmount);
  CurrencyCode resCurrency = CurrencyCode::kNeutral;
  if (!_currencyCode.isNeutral() && !div.currencyCode().isNeutral()) {
    if (CCT_UNLIKELY(_currencyCode != div.currencyCode())) {
      throw exception("Cannot divide two non neutral MonetaryAmounts of different currency");
    }
    // Divide same currency have a neutral result
  } else {
    resCurrency = _currencyCode.isNeutral() ? div.currencyCode() : _currencyCode;
  }

  // Switch to an unsigned temporarily to ensure that lhs > rhs before the divide.
  // Indeed, on 64 bits the unsigned integral type can hold one more digit than its signed counterpart.
  static_assert(std::numeric_limits<UnsignedAmountType>::digits10 > std::numeric_limits<AmountType>::digits10);

  const int8_t lhsNbDigitsToAdd = std::numeric_limits<UnsignedAmountType>::digits10 - lhsNbDigits;
  lhs *= ipow(static_cast<UnsignedAmountType>(10), static_cast<uint8_t>(lhsNbDigitsToAdd));
  lhsNbDecimals += lhsNbDigitsToAdd;
  lhsNbDigits += lhsNbDigitsToAdd;

  UnsignedAmountType totalIntPart = 0;
  int8_t nbDecimals = lhsNbDecimals - rhsNbDecimals;
  int8_t totalPartNbDigits;
  do {
    totalIntPart += lhs / rhs;  // Add integral part
    totalPartNbDigits = static_cast<int8_t>(ndigits(totalIntPart));
    lhs %= rhs;  // Keep the rest
    if (lhs == 0) {
      break;
    }
    const int8_t nbDigitsToAdd = std::numeric_limits<UnsignedAmountType>::digits10 -
                                 std::max(totalPartNbDigits, static_cast<int8_t>(ndigits(lhs)));
    if (nbDigitsToAdd == 0) {
      break;
    }
    const auto kMultPower = ipow(static_cast<UnsignedAmountType>(10), static_cast<uint8_t>(nbDigitsToAdd));
    totalIntPart *= kMultPower;
    lhs *= kMultPower;
    nbDecimals += nbDigitsToAdd;
  } while (true);

  if (nbDecimals < 0) {
    throw exception("Overflow during divide");
  }

  const int8_t nbDigitsTruncate = totalPartNbDigits - std::numeric_limits<AmountType>::digits10;
  if (nbDigitsTruncate > 0) {
    if (nbDecimals < nbDigitsTruncate) {
      throw exception("Overflow during divide");
    }
    totalIntPart /= ipow(static_cast<UnsignedAmountType>(10), static_cast<uint8_t>(nbDigitsTruncate));
    nbDecimals -= nbDigitsTruncate;
  }

  MonetaryAmount ret(totalIntPart * negMult, resCurrency, nbDecimals);
  ret.sanitizeNbDecimals();
  ret.simplify();
  assert(ret.isSane());
  return ret;
}

std::string MonetaryAmount::amountStr() const {
  const int isNeg = static_cast<int>(_amount < 0);
  const int nbDigits = ndigits(_amount);
  const int nbZerosToInsertFront = std::max(0, static_cast<int>(_nbDecimals + 1 - nbDigits));

  std::string ret(static_cast<size_t>(isNeg) + nbDigits, '-');
  std::to_chars(ret.data(), ret.data() + ret.size(), _amount);

  ret.insert(ret.begin() + isNeg, nbZerosToInsertFront, '0');

  if (_nbDecimals > 0) {
    ret.insert(ret.end() - _nbDecimals, '.');
  }

  return ret;
}

std::string MonetaryAmount::str() const {
  std::string ret = amountStr();
  if (!_currencyCode.isNeutral()) {
    ret.push_back(' ');
    ret.append(_currencyCode.str());
  }
  return ret;
}

std::ostream &operator<<(std::ostream &os, const MonetaryAmount &m) {
  os << m.str();
  return os;
}

}  // namespace cct
