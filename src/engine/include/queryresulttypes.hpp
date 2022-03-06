#pragma once

#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "balanceportfolio.hpp"
#include "cct_const.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_smallvector.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapitypes.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "tradedamounts.hpp"
#include "wallet.hpp"

namespace cct {
class Exchange;

using MarketOrderBookConversionRate = std::tuple<std::string_view, MarketOrderBook, std::optional<MonetaryAmount>>;

using MarketOrderBookConversionRates = FixedCapacityVector<MarketOrderBookConversionRate, kNbSupportedExchanges>;

using MarketsPerExchange = FixedCapacityVector<std::pair<const Exchange *, MarketSet>, kNbSupportedExchanges>;

using MonetaryAmountPerExchange =
    FixedCapacityVector<std::pair<const Exchange *, MonetaryAmount>, kNbSupportedExchanges>;

using LastTradesPerExchange = FixedCapacityVector<std::pair<const Exchange *, LastTradesVector>, kNbSupportedExchanges>;

using TradedAmountsPerExchange = SmallVector<std::pair<const Exchange *, TradedAmounts>, kTypicalNbPrivateAccounts>;

using TradedAmountsVectorWithFinalAmountPerExchange =
    SmallVector<std::pair<const Exchange *, TradedAmountsVectorWithFinalAmount>, kTypicalNbPrivateAccounts>;

using ExchangeTickerMaps = FixedCapacityVector<std::pair<const Exchange *, MarketOrderBookMap>, kNbSupportedExchanges>;

using BalancePerExchange = SmallVector<std::pair<Exchange *, BalancePortfolio>, kTypicalNbPrivateAccounts>;

using WalletPerExchange = SmallVector<std::pair<const Exchange *, Wallet>, kTypicalNbPrivateAccounts>;

using OpenedOrdersPerExchange = SmallVector<std::pair<const Exchange *, OrdersSet>, kTypicalNbPrivateAccounts>;

using NbCancelledOrdersPerExchange = SmallVector<std::pair<const Exchange *, int>, kTypicalNbPrivateAccounts>;

using ConversionPathPerExchange = FixedCapacityVector<std::pair<const Exchange *, MarketsPath>, kNbSupportedExchanges>;
}  // namespace cct