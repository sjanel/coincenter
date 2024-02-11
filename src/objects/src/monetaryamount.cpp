#include "monetaryamount.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <compare>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <limits>
#include <optional>
#include <ostream>
#include <ranges>
#include <sstream>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "cct_config.hpp"
#include "cct_exception.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "currencycode.hpp"
#include "mathhelpers.hpp"
#include "stringhelpers.hpp"

namespace cct {
namespace {

/// Source: https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html
/// Theorem 15
constexpr int kNbMaxDoubleDecimals = std::numeric_limits<double>::max_digits10;

constexpr void RemovePrefixSpaces(std::string_view &str) {
  str.remove_prefix(std::ranges::find_if(str, [](char ch) { return ch != ' '; }) - str.begin());
}
constexpr void RemoveTrailingSpaces(std::string_view &str) {
  str.remove_suffix(std::ranges::find_if(std::ranges::reverse_view(str), [](char ch) { return ch != ' '; }) -
                    std::ranges::rbegin(str));
}
constexpr void RemoveTrailingZeros(std::string_view &str) {
  str.remove_suffix(std::ranges::find_if(std::ranges::reverse_view(str), [](char ch) { return ch != '0'; }) -
                    std::ranges::rbegin(str));
}

inline int ParseNegativeChar(std::string_view &amountStr) {
  int negMult = 1;
  if (amountStr.empty()) {
    return negMult;
  }
  RemovePrefixSpaces(amountStr);
  if (amountStr.front() < '0') {
    static_assert('-' < '0' && '+' < '0' && '.' < '0' && ' ' < '0');
    switch (amountStr.front()) {
      case '-':
        negMult = -1;
        [[fallthrough]];
      case '+':  // Let's accept inputs like: "+3" -> "3"
        amountStr.remove_prefix(1UL);
        RemovePrefixSpaces(amountStr);
        break;
      case '.':  // Let's accept inputs like: ".5" -> "0.5"
        break;
      default:
        throw exception("Parsing error, unexpected first char {}", amountStr.front());
    }
  }
  return negMult;
}

/// Converts a string into a fixed precision integral containing both the integer and decimal part.
/// @param amountStr the string to convert
/// @param heuristicRoundingFromDouble if true, more than 5 consecutive zeros or 9 in the decimals part will be rounded
inline auto AmountIntegralFromStr(std::string_view amountStr, bool heuristicRoundingFromDouble = false) {
  std::pair<MonetaryAmount::AmountType, int8_t> ret;
  ret.second = 0;

  if (amountStr.empty()) {
    ret.first = 0;
    return ret;
  }
  std::size_t dotPos = amountStr.find('.');
  MonetaryAmount::AmountType roundingUpNinesDouble = 0;
  MonetaryAmount::AmountType decPart;
  MonetaryAmount::AmountType integerPart;
  if (dotPos == std::string_view::npos) {
    decPart = 0;
    integerPart = FromString<MonetaryAmount::AmountType>(amountStr);
  } else {
    RemoveTrailingSpaces(amountStr);
    RemoveTrailingZeros(amountStr);

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
        throw exception("Received amount string {} whose integral part is too big", amountStr);
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

  ret.first = integerPart * ipow10(ret.second) + decPart + roundingUpNinesDouble;
  return ret;
}

}  // namespace

MonetaryAmount::MonetaryAmount(std::string_view amountCurrencyStr, IfNoAmount ifNoAmount) {
  const int negMult = ParseNegativeChar(amountCurrencyStr);

  auto last = amountCurrencyStr.begin();
  const auto endIt = amountCurrencyStr.end();
  static_assert(' ' < '+' && '+' < '-' && '+' < '.');  // Trick: all '.', '+', '-' are before digits in the ASCII code
  while (last != endIt && *last >= '+' && *last <= '9') {
    ++last;
  }
  const std::string_view amountStr(amountCurrencyStr.begin(), last);
  int8_t nbDecimals;
  std::tie(_amount, nbDecimals) = AmountIntegralFromStr(amountStr);
  _amount *= negMult;
  std::string_view currencyStr(last, endIt);
  RemoveTrailingSpaces(currencyStr);
  RemovePrefixSpaces(currencyStr);
  if (ifNoAmount == IfNoAmount::kThrow && !currencyStr.empty() && amountStr.empty()) {
    throw invalid_argument("Cannot construct MonetaryAmount with a currency without any amount");
  }
  _curWithDecimals = CurrencyCode(currencyStr);
  sanitizeDecimals(nbDecimals, maxNbDecimals());
}

MonetaryAmount::MonetaryAmount(std::string_view amountStr, CurrencyCode currencyCode) : _curWithDecimals(currencyCode) {
  const int negMult = ParseNegativeChar(amountStr);
  int8_t nbDecimals;
  std::tie(_amount, nbDecimals) = AmountIntegralFromStr(amountStr);
  _amount *= negMult;
  sanitizeDecimals(nbDecimals, maxNbDecimals());
}

MonetaryAmount::MonetaryAmount(double amount, CurrencyCode currencyCode) : _curWithDecimals(currencyCode) {
  std::stringstream amtBuf;
  amtBuf << std::setprecision(kNbMaxDoubleDecimals) << std::fixed << amount;
  int8_t nbDecimals;
  std::string_view strView = amtBuf.view();
  const int negMult = ParseNegativeChar(strView);

  std::tie(_amount, nbDecimals) = AmountIntegralFromStr(strView, true);
  _amount *= negMult;

  sanitizeDecimals(nbDecimals, maxNbDecimals());
}

MonetaryAmount::MonetaryAmount(double amount, CurrencyCode currencyCode, RoundType roundType, int8_t nbDecimals)
    : MonetaryAmount(amount, currencyCode) {
  round(nbDecimals, roundType);
}

std::optional<MonetaryAmount::AmountType> MonetaryAmount::amount(int8_t nbDecimals) const {
  AmountType integralAmount = _amount;
  const int8_t ourNbDecimals = this->nbDecimals();
  for (; nbDecimals < ourNbDecimals; ++nbDecimals) {
    integralAmount /= 10;
  }
  for (; ourNbDecimals < nbDecimals; --nbDecimals) {
    if (integralAmount > std::numeric_limits<AmountType>::max() / 10 ||
        integralAmount < std::numeric_limits<AmountType>::min() / 10) {
      return std::nullopt;
    }
    integralAmount *= 10;
  }
  return integralAmount;
}

constexpr MonetaryAmount::AmountType MonetaryAmount::decimalPart() const {
  const auto div = ipow10(static_cast<uint8_t>(nbDecimals()));
  return _amount - (_amount / div) * div;
}

namespace {

constexpr auto SafeConvertSameDecimals(MonetaryAmount::AmountType &lhsAmount, MonetaryAmount::AmountType &rhsAmount,
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
        --lhsNbDecimals;
        --lhsNbDigits;
        lhsAmount /= 10;
      }
    }
  }
  return lhsNbDecimals;
}
}  // namespace

void MonetaryAmount::round(MonetaryAmount step, RoundType roundType) {
  AmountType rhsAmount = step._amount;
  assert(rhsAmount > 0);
  int8_t nowNbDecimals = SafeConvertSameDecimals(_amount, rhsAmount, nbDecimals(), step.nbDecimals());
  AmountType epsilon = _amount % rhsAmount;
  if (epsilon != 0) {
    _amount -= epsilon;
    if (_amount + epsilon < 0) {
      if (_amount >= std::numeric_limits<AmountType>::min() + rhsAmount &&  // Protection against overflow
          (roundType == RoundType::kDown || (roundType == RoundType::kNearest && rhsAmount < -2 * epsilon))) {
        _amount -= rhsAmount;
      }
    } else {
      if (_amount <= std::numeric_limits<AmountType>::max() - rhsAmount &&  // Protection against overflow
          (roundType == RoundType::kUp || (roundType == RoundType::kNearest && 2 * epsilon >= rhsAmount))) {
        _amount += rhsAmount;
      }
    }
  }
  sanitizeDecimals(nowNbDecimals, nowNbDecimals);
}

void MonetaryAmount::round(int8_t nbDecimals, RoundType roundType) {
  int8_t currentNbDecimals = this->nbDecimals();
  for (; currentNbDecimals < nbDecimals; ++currentNbDecimals) {
    if (_amount > std::numeric_limits<AmountType>::max() / 10 ||
        _amount < std::numeric_limits<AmountType>::min() / 10) {
      nbDecimals = currentNbDecimals;
      log::debug("Desired rounding cannot be applied");
      break;
    }
    _amount *= 10;
  }
  if (nbDecimals < currentNbDecimals) {
    const AmountType epsilon = ipow10(currentNbDecimals - nbDecimals);
    if (_amount < 0) {
      if (roundType != RoundType::kUp) {
        const AmountType rem = epsilon + (_amount % epsilon);
        if (_amount >= std::numeric_limits<AmountType>::min() + rem &&  // Protection against overflow
            (roundType == RoundType::kDown || 2 * rem < epsilon)) {
          _amount -= rem;
        }
      }
    } else {
      if (roundType != RoundType::kDown) {
        const AmountType rem = epsilon - (_amount % epsilon);
        if (_amount <= std::numeric_limits<AmountType>::max() - rem &&  // Protection against overflow
            (roundType == RoundType::kUp || 2 * rem <= epsilon)) {
          _amount += rem;
        }
      }
    }
  }

  sanitizeDecimals(currentNbDecimals, nbDecimals);
}

bool MonetaryAmount::isCloseTo(MonetaryAmount otherAmount, double relativeDifference) const {
  const double ourAmount = std::abs(toDouble());
  const double boundMin = ourAmount * (1.0 - relativeDifference);
  const double boundMax = ourAmount * (1.0 + relativeDifference);

  if (boundMin < 0 || boundMax < 0) {
    throw exception("Unexpected bounds [{}-{}]", boundMin, boundMax);
  }
  const double closestAmount = std::abs(otherAmount.toDouble());
  return closestAmount > boundMin && closestAmount < boundMax;
}

std::strong_ordering MonetaryAmount::operator<=>(const MonetaryAmount &other) const {
  if (currencyCode() != other.currencyCode()) {
    throw exception("Cannot compare amounts with different currency");
  }
  const auto lhsNbDecimals = nbDecimals();
  const auto rhsNbDecimals = other.nbDecimals();
  if (lhsNbDecimals == rhsNbDecimals) {
    return _amount <=> other._amount;
  }
  const auto lhsIntAmount = integerPart();
  const auto rhsIntAmount = other.integerPart();
  if (lhsIntAmount != rhsIntAmount) {
    return lhsIntAmount <=> rhsIntAmount;
  }
  // Same integral part, so expanding one's number of decimals towards the other one is safe
  const auto adjustDecimals = [](AmountType lhsAmount, AmountType rhsAmount, int8_t lhsNbD, int8_t rhsNbD) {
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
    return lhsAmount <=> rhsAmount;
  };
  return adjustDecimals(_amount, other._amount, lhsNbDecimals, rhsNbDecimals);
}

MonetaryAmount MonetaryAmount::operator+(MonetaryAmount other) const {
  if (_amount == 0 && _curWithDecimals.isNeutral()) {
    return other;
  }
  if (other._amount == 0 && other._curWithDecimals.isNeutral()) {
    return *this;
  }
  if (currencyCode() != other.currencyCode()) {
    throw exception("Addition is only possible on amounts with same currency");
  }
  auto lhsAmount = _amount;
  auto rhsAmount = other._amount;
  int8_t resNbDecimals = SafeConvertSameDecimals(lhsAmount, rhsAmount, nbDecimals(), other.nbDecimals());
  AmountType resAmount = lhsAmount + rhsAmount;
  if (resAmount >= kMaxAmountFullNDigits || resAmount <= -kMaxAmountFullNDigits) {
    resAmount /= 10;
    --resNbDecimals;
  }
  return {resAmount, _curWithDecimals, resNbDecimals};
}

MonetaryAmount MonetaryAmount::operator*(AmountType mult) const {
  AmountType amount = _amount;
  auto nbDecs = nbDecimals();
  if (mult < -1 || mult > 1) {  // for * -1, * 0 and * -1 result is trivial without overflow
    // Beware of overflows, they can come faster than we think with multiplications.
    const auto nbDigitsMult = ndigits(mult);
    const auto nbDigitsAmount = ndigits(_amount);
    const auto nbDigitsToTruncate = nbDigitsAmount + nbDigitsMult - std::numeric_limits<AmountType>::digits10;
    if (nbDigitsToTruncate > 0) {
      log::trace("Reaching numeric limits of MonetaryAmount for {} * {}, truncate {} digits", _amount, mult,
                 nbDigitsToTruncate);
      amount /= ipow10(static_cast<uint8_t>(nbDigitsToTruncate));
      if (static_cast<std::remove_const_t<decltype(nbDigitsToTruncate)>>(nbDecs) >= nbDigitsToTruncate) {
        nbDecs -= static_cast<decltype(nbDecs)>(nbDigitsToTruncate);
      } else {
        log::warn("Cannot truncate decimal part, I need to truncate integral part");
      }
    }
  }
  return {amount * mult, _curWithDecimals, nbDecs};
}

MonetaryAmount MonetaryAmount::operator*(MonetaryAmount mult) const {
  if (!_curWithDecimals.isNeutral() && !mult._curWithDecimals.isNeutral()) {
    throw exception("Cannot multiply two non neutral MonetaryAmounts");
  }
  AmountType lhsAmount = _amount;
  AmountType rhsAmount = mult._amount;
  int8_t lhsNbDecimals = nbDecimals();
  int8_t rhsNbDecimals = mult.nbDecimals();
  int lhsNbDigits = ndigits(_amount);
  int rhsNbDigits = ndigits(mult._amount);

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
  CurrencyCode resCurrency = _curWithDecimals.isNeutral() ? mult._curWithDecimals : _curWithDecimals;
  return {lhsAmount * rhsAmount, resCurrency, static_cast<int8_t>(lhsNbDecimals + rhsNbDecimals)};
}

MonetaryAmount MonetaryAmount::operator/(MonetaryAmount div) const {
  CurrencyCode resCurrency;
  if (!_curWithDecimals.isNeutral() && !div._curWithDecimals.isNeutral()) {
    if (CCT_UNLIKELY(currencyCode() != div.currencyCode())) {
      throw exception("Cannot divide two non neutral MonetaryAmounts of different currency");
    }
    // Divide same currency have a neutral result
  } else {
    resCurrency = _curWithDecimals.isNeutral() ? div._curWithDecimals : _curWithDecimals;
  }

  AmountType lhsAmount = _amount;
  AmountType rhsAmount = div._amount;
  assert(rhsAmount != 0);
  const int negMult = ((lhsAmount < 0 && rhsAmount > 0) || (lhsAmount > 0 && rhsAmount < 0)) ? -1 : 1;

  // Switch to an unsigned temporarily to ensure that lhs > rhs before the divide.
  // Indeed, on 64 bits the unsigned integral type can hold one more digit than its signed counterpart.
  static_assert(std::numeric_limits<UnsignedAmountType>::digits10 > std::numeric_limits<AmountType>::digits10);

  int8_t lhsNbDigits = static_cast<int8_t>(ndigits(_amount));
  const int8_t lhsNbDigitsToAdd = std::numeric_limits<UnsignedAmountType>::digits10 - lhsNbDigits;
  UnsignedAmountType lhs =
      static_cast<UnsignedAmountType>(std::abs(lhsAmount)) * ipow10(static_cast<uint8_t>(lhsNbDigitsToAdd));
  UnsignedAmountType rhs = static_cast<UnsignedAmountType>(std::abs(rhsAmount));

  int8_t lhsNbDecimals = nbDecimals() + lhsNbDigitsToAdd;

  UnsignedAmountType totalIntPart = 0;
  int8_t nbDecs = lhsNbDecimals - div.nbDecimals();
  int8_t totalPartNbDigits;
  while (true) {
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
    const auto multPower = ipow10(static_cast<uint8_t>(nbDigitsToAdd));
    totalIntPart *= multPower;
    lhs *= multPower;
    nbDecs += nbDigitsToAdd;
  }

  if (nbDecs < 0) {
    throw exception("Overflow during divide");
  }

  const int8_t nbDigitsTruncate = totalPartNbDigits - std::numeric_limits<AmountType>::digits10;
  if (nbDigitsTruncate > 0) {
    if (nbDecs < nbDigitsTruncate) {
      throw exception("Overflow during divide");
    }
    totalIntPart /= ipow10(static_cast<uint8_t>(nbDigitsTruncate));
    nbDecs -= nbDigitsTruncate;
  }

  return {static_cast<AmountType>(totalIntPart) * negMult, resCurrency, nbDecs};
}

std::ostream &operator<<(std::ostream &os, const MonetaryAmount &ma) { return os << ma.str(); }

}  // namespace cct
