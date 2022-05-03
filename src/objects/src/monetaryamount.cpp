#include "monetaryamount.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <sstream>

#include "cct_cctype.hpp"
#include "cct_config.hpp"
#include "cct_exception.hpp"
#include "stringhelpers.hpp"

namespace cct {
namespace {

/// Source: https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html
/// Theorem 15
constexpr int kNbMaxDoubleDecimals = std::numeric_limits<double>::max_digits10;

inline void RemovePrefixSpaces(std::string_view &str) {
  str.remove_prefix(std::find_if_not(str.begin(), str.end(), [](char c) { return c == ' '; }) - str.begin());
}
inline void RemoveTrailing(std::string_view &str, char r) {
  str.remove_suffix(std::find_if_not(str.rbegin(), str.rend(), [r](char c) { return c == r; }) - str.rbegin());
}

inline bool ParseNegativeChar(std::string_view &amountStr) {
  bool isNeg = false;
  if (amountStr.front() < '0') {
    static_assert('-' < '0' && '+' < '0' && '.' < '0' && ' ' < '0');
    switch (amountStr.front()) {
      case '-':
        isNeg = true;
        [[fallthrough]];
      case '+':  // Let's accept inputs like: "+3" -> "3"
        // Remove at least one char + possible spaces after it
        amountStr.remove_prefix(
            std::find_if_not(amountStr.begin() + 1, amountStr.end(), [](char c) { return c == ' '; }) -
            amountStr.begin());
        break;
      case '.':  // Let's accept inputs like: ".5" -> "0.5"
        break;
      default: {
        string ex("Parsing error, unexpected first char ");
        ex.push_back(amountStr.front());
        throw exception(std::move(ex));
      }
    }
  }
  return isNeg;
}

/// Converts a string into a fixed precision integral containing both the integer and decimal part.
/// @param amountStr the string to convert
/// @param heuristicRoundingFromDouble if true, more than 5 consecutive zeros or 9 in the decimals part will be rounded
inline std::pair<MonetaryAmount::AmountType, int8_t> AmountIntegralFromStr(std::string_view amountStr,
                                                                           bool heuristicRoundingFromDouble = false) {
  std::pair<MonetaryAmount::AmountType, int8_t> ret;
  ret.second = 0;

  if (amountStr.empty()) {
    ret.first = 0;
    return ret;
  }
  const bool isNeg = ParseNegativeChar(amountStr);
  std::size_t dotPos = amountStr.find('.');
  MonetaryAmount::AmountType roundingUpNinesDouble = 0;
  MonetaryAmount::AmountType decPart;
  MonetaryAmount::AmountType integerPart;
  if (dotPos == std::string_view::npos) {
    decPart = 0;
    integerPart = FromString<MonetaryAmount::AmountType>(amountStr);
  } else {
    // Remove trailing zeros
    RemoveTrailing(amountStr, '0');
    if (heuristicRoundingFromDouble && (amountStr.size() - dotPos - 1) == kNbMaxDoubleDecimals) {
      std::size_t bestFindPos = 0;
      for (std::string_view pattern : {"000", "999"}) {
        std::size_t findPos = amountStr.rfind(pattern);
        if (findPos != std::string_view::npos && findPos > dotPos) {
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
    ret.second = static_cast<int8_t>(amountStr.size() - dotPos - 1);
    // dotPos is still valid as we erased only past elements
    if (amountStr.size() > std::numeric_limits<MonetaryAmount::AmountType>::digits10 + 1) {
      int8_t nbDigitsToRemove =
          static_cast<int8_t>(amountStr.size() - std::numeric_limits<MonetaryAmount::AmountType>::digits10 - 1);
      if (nbDigitsToRemove > ret.second) {
        string ex("Received amount string ");
        ex.append(amountStr).append(" whose integral part is too big");
        throw exception(std::move(ex));
      }
      log::trace("Received amount string '{}' too big for MonetaryAmount, truncating {} digits", amountStr,
                 nbDigitsToRemove);
      amountStr.remove_suffix(nbDigitsToRemove);
      ret.second -= nbDigitsToRemove;
    }
    std::string_view decPartStr = amountStr.substr(dotPos + 1);
    decPart = decPartStr.empty() ? 0 : FromString<MonetaryAmount::AmountType>(decPartStr);
    if (dotPos == 0) {
      integerPart = 0;
    } else {
      integerPart =
          FromString<MonetaryAmount::AmountType>(std::string_view(amountStr.data(), amountStr.data() + dotPos));
    }
  }

  ret.first = integerPart * ipow(10, ret.second) + decPart + roundingUpNinesDouble;
  if (isNeg) {
    ret.first *= -1;
  }
  return ret;
}

}  // namespace

MonetaryAmount::MonetaryAmount(std::string_view amountCurrencyStr) {
  RemovePrefixSpaces(amountCurrencyStr);
  RemoveTrailing(amountCurrencyStr, ' ');

  auto last = amountCurrencyStr.begin();
  auto endIt = amountCurrencyStr.end();
  while (last != endIt && *last <= '9') {  // Trick: all '.', '+', '-' are before digits in the ASCII code
    ++last;
  }
  std::string_view amountStr(amountCurrencyStr.begin(), last);
  RemoveTrailing(amountStr, ' ');
  std::tie(_amount, _nbDecimals) = AmountIntegralFromStr(amountStr);
  _currencyCode = CurrencyCode(std::string_view(last, endIt));
  assert(isSane());
}

MonetaryAmount::MonetaryAmount(std::string_view amountStr, CurrencyCode currencyCode) : _currencyCode(currencyCode) {
  RemovePrefixSpaces(amountStr);
  RemoveTrailing(amountStr, ' ');
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
    if (integralAmount > std::numeric_limits<AmountType>::max() / 10 ||
        integralAmount < std::numeric_limits<AmountType>::min() / 10) {
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
  assert(rhsAmount > 0);
  int8_t resNbDecimals = SafeConvertSameDecimals(lhsAmount, rhsAmount, _nbDecimals, step._nbDecimals);
  AmountType epsilon = lhsAmount % rhsAmount;
  AmountType resAmount = lhsAmount - epsilon;
  if (epsilon != 0) {
    if (lhsAmount < 0) {
      if (resAmount >= std::numeric_limits<AmountType>::min() + rhsAmount &&  // Protection against overflow
          (roundType == RoundType::kDown || (roundType == RoundType::kNearest && -epsilon >= rhsAmount / 2U))) {
        resAmount -= rhsAmount;
      }
    } else {
      if (resAmount <= std::numeric_limits<AmountType>::max() - rhsAmount &&  // Protection against overflow
          (roundType == RoundType::kUp || (roundType == RoundType::kNearest && epsilon >= rhsAmount / 2U))) {
        resAmount += rhsAmount;
      }
    }
  }

  return MonetaryAmount(resAmount, _currencyCode, resNbDecimals);
}

std::strong_ordering MonetaryAmount::operator<=>(const MonetaryAmount &o) const {
  if (CCT_UNLIKELY(_currencyCode != o._currencyCode)) {
    throw exception("Cannot compare amounts with different currency");
  }
  if (_nbDecimals == o._nbDecimals) {
    return _amount <=> o._amount;
  }
  AmountType lhsIntAmount = integerPart();
  AmountType rhsIntAmount = o.integerPart();
  if (lhsIntAmount != rhsIntAmount) {
    return lhsIntAmount <=> rhsIntAmount;
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
  return amounts.first <=> amounts.second;
}

MonetaryAmount MonetaryAmount::operator+(MonetaryAmount o) const {
  if (CCT_UNLIKELY(_currencyCode != o._currencyCode)) {
    throw exception("Addition is only possible on amounts with same currency");
  }
  AmountType lhsAmount = _amount;
  AmountType rhsAmount = o._amount;
  int8_t resNbDecimals = SafeConvertSameDecimals(lhsAmount, rhsAmount, _nbDecimals, o._nbDecimals);
  AmountType resAmount = lhsAmount + rhsAmount;
  if (resAmount >= kMaxAmountFullNDigits || resAmount <= -kMaxAmountFullNDigits) {
    resAmount /= 10;
    --resNbDecimals;
  }
  return MonetaryAmount(resAmount, _currencyCode, resNbDecimals);
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
  return MonetaryAmount(amount * mult, _currencyCode, nbDecimals);
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

  while (lhsNbDigits + rhsNbDigits > std::numeric_limits<AmountType>::digits10) {
    // We need to truncate, choose the MonetaryAmount with the highest number of decimals in priority
    if (rhsNbDecimals == lhsNbDecimals && lhsNbDecimals == 0) {
      log::warn("Cannot truncate decimal part, truncating integral part");
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

  return MonetaryAmount(lhsAmount * rhsAmount, resCurrency, lhsNbDecimals + rhsNbDecimals);
}

MonetaryAmount MonetaryAmount::operator/(MonetaryAmount div) const {
  AmountType lhsAmount = _amount;
  AmountType rhsAmount = div._amount;
  assert(rhsAmount != 0);
  const int negMult = ((lhsAmount < 0 && rhsAmount > 0) || (lhsAmount > 0 && rhsAmount < 0)) ? -1 : 1;
  CurrencyCode resCurrency;
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

  int8_t lhsNbDigits = static_cast<int8_t>(ndigits(_amount));
  const int8_t lhsNbDigitsToAdd = std::numeric_limits<UnsignedAmountType>::digits10 - lhsNbDigits;
  UnsignedAmountType lhs =
      static_cast<UnsignedAmountType>(std::abs(lhsAmount)) * ipow(10, static_cast<uint8_t>(lhsNbDigitsToAdd));
  UnsignedAmountType rhs = static_cast<UnsignedAmountType>(std::abs(rhsAmount));

  int8_t lhsNbDecimals = _nbDecimals + lhsNbDigitsToAdd;

  lhsNbDigits += lhsNbDigitsToAdd;

  UnsignedAmountType totalIntPart = 0;
  int8_t nbDecimals = lhsNbDecimals - div._nbDecimals;
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

  return MonetaryAmount(totalIntPart * negMult, resCurrency, nbDecimals);
}

string MonetaryAmount::amountStr() const {
  const int isNeg = static_cast<int>(_amount < 0);
  const int nbDigits = ndigits(_amount);

  string ret(static_cast<size_t>(isNeg) + nbDigits, '-');
  std::to_chars(ret.data(), ret.data() + ret.size(), _amount);

  if (_nbDecimals + 1 > nbDigits) {
    ret.insert(ret.begin() + isNeg, _nbDecimals + 1 - nbDigits, '0');
  }

  if (_nbDecimals > 0) {
    ret.insert(ret.end() - _nbDecimals, '.');
  }

  return ret;
}

string MonetaryAmount::str() const {
  string ret = amountStr();
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
