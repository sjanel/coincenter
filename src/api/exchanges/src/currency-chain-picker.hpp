#pragma once

#include <algorithm>
#include <functional>
#include <span>
#include <string_view>
#include <utility>

#include "currencycode.hpp"
#include "exchange-asset-config.hpp"

namespace cct::api {

template <class ChainT>
class CurrencyChainPicker {
 public:
  CurrencyChainPicker(const schema::ExchangeAssetConfig& assetConfig,
                      std::function<std::string_view(const ChainT&)> chainNameFromChain)
      : _preferredChains(assetConfig.preferredChains), _chainNameFromChain(std::move(chainNameFromChain)) {}

  bool shouldDiscardChain(std::span<const ChainT> allChains, CurrencyCode cur, const ChainT& chainDetail) const {
    std::string_view chainName = _chainNameFromChain(chainDetail);
    if (!_preferredChains.empty()) {
      // Display name is actually the chain name (for instance, ERC20).
      // chain is the name of the currency in this chain (for instance, shib).
      for (CurrencyCode preferredChain : _preferredChains) {
        const auto it = std::ranges::find_if(allChains, [this, preferredChain](const auto& chain) {
          return preferredChain.iequal(_chainNameFromChain(chain));
        });
        if (it != allChains.end()) {
          // return the unique chain that matches the first preferred one, discard all the other ones
          return chainName != _chainNameFromChain(*it);
        }
      }

      return true;
    }
    if (!cur.iequal(chainName)) {
      log::debug("Discarding chain '{}' as not supported by {}", chainName, cur);
      return true;
    }
    return false;
  }

 private:
  std::span<const CurrencyCode> _preferredChains;
  std::function<std::string_view(const ChainT&)> _chainNameFromChain;
};

}  // namespace cct::api