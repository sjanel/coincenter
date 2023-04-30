#pragma once

#include <cstdint>

namespace cct {
class CurrencyIdTimeConstraintsBmp {
 public:
  enum class ConstraintType : uint8_t { kCur, kReceivedBefore, kReceivedAfter, kId };

 private:
  static constexpr uint8_t kIdConstrained = 1U << static_cast<uint8_t>(ConstraintType::kId);
  static constexpr uint8_t kCurConstrained = 1U << static_cast<uint8_t>(ConstraintType::kCur);

 public:
  void set(ConstraintType constraintType) { _bmp |= (1U << static_cast<uint8_t>(constraintType)); }

  bool isConstrained(ConstraintType constraintType) const {
    return _bmp & (1U << static_cast<uint8_t>(constraintType));
  }

  bool empty() const { return _bmp == 0U; }

  bool isCurDependent() const { return isConstrained(ConstraintType::kCur); }
  bool isCurOnlyDependent() const { return _bmp == kCurConstrained; }
  bool isAtMostCurOnlyDependent() const { return ((_bmp | kCurConstrained) & ~kCurConstrained) == 0U; }

  bool isDepositIdOnlyDependent() const { return _bmp == kIdConstrained; }

  bool operator==(const CurrencyIdTimeConstraintsBmp &) const noexcept = default;

 private:
  uint8_t _bmp{};
};

}  // namespace cct
