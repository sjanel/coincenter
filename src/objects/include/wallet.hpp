#pragma once

#include <string_view>

#include "accountowner.hpp"
#include "cct_config.hpp"
#include "cct_exception.hpp"
#include "cct_format.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"

namespace cct {
class WalletCheck {
 public:
  /// Deposit wallet will not be checked in file
  WalletCheck() noexcept = default;

  /// Deposit wallet will be checked in file
  WalletCheck(std::string_view dataDir, bool validateInDepositAddress)
      : _dataDir(validateInDepositAddress ? dataDir : std::string_view()) {}

  bool doCheck() const { return !_dataDir.empty(); }

  std::string_view dataDir() const { return _dataDir; }

 private:
  std::string_view _dataDir;
};

class Wallet {
 public:
  /// Empty wallet that should not be used, just for temporary purposes.
  Wallet() noexcept = default;

  /// Build a wallet with all information.
  Wallet(ExchangeName exchangeName, CurrencyCode currency, string address, std::string_view tag,
         WalletCheck walletCheck, const AccountOwner &accountOwner);

  const ExchangeName &exchangeName() const { return _exchangeName; }

  std::string_view address() const {
    check();
    return std::string_view(_addressAndTag.data(), startTag());
  }

  std::string_view tag() const {
    check();
    return std::string_view(startTag(), _addressAndTag.data() + _addressAndTag.size());
  }

  const AccountOwner &accountOwner() const { return _accountOwner; }

  CurrencyCode currencyCode() const { return _currency; }

  bool hasTag() const { return _tagPos != std::string_view::npos; }

  bool operator==(const Wallet &) const = default;

  static bool ValidateWallet(WalletCheck walletCheck, const ExchangeName &exchangeName, CurrencyCode currency,
                             std::string_view expectedAddress, std::string_view expectedTag);

  using trivially_relocatable =
      std::bool_constant<is_trivially_relocatable_v<ExchangeName> && is_trivially_relocatable_v<string> >::type;

 private:
  const char *startTag() const { return _addressAndTag.data() + (hasTag() ? _tagPos : _addressAndTag.size()); }

  void check() const {
    if (CCT_UNLIKELY(_addressAndTag.empty())) {
      throw exception("Cannot use an empty wallet address!");
    }
  }

  ExchangeName _exchangeName;
  string _addressAndTag;
  AccountOwner _accountOwner;
  std::size_t _tagPos = std::string_view::npos;
  CurrencyCode _currency;
};

}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::Wallet> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::Wallet &w, FormatContext &ctx) const -> decltype(ctx.out()) {
    bool hasTag = w.hasTag();
    return fmt::format_to(ctx.out(), "{:n} wallet of {} [{}{}{}]", w.exchangeName(), w.currencyCode(), w.address(),
                          hasTag ? "," : "", hasTag ? w.tag() : "");
  }
};
#endif