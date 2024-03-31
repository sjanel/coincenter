#pragma once

#include <string_view>

#include "cct_string.hpp"
#include "cct_type_traits.hpp"

namespace cct {

/// @brief Holds information about the account owner of a key.
class AccountOwner {
 public:
  /// @brief Creates an empty AccountOwner, that is, without any information.
  AccountOwner() noexcept = default;

  /// @brief Creates a new AccountOwner
  /// @param enName the person's name spelled in English that owns the account
  /// @param koName the person's name spelled in Korean that owns the account
  AccountOwner(std::string_view enName, std::string_view koName) : _enName(enName), _koName(koName) {}

  std::string_view enName() const { return _enName; }
  std::string_view koName() const { return _koName; }

  bool isFullyDefined() const { return !_enName.empty() && !_koName.empty(); }

  using trivially_relocatable = is_trivially_relocatable<string>::type;

  bool operator==(const AccountOwner &) const = default;

 private:
  string _enName;
  string _koName;
};

}  // namespace cct