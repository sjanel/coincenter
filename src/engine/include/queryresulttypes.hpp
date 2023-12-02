#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "balanceportfolio.hpp"
#include "cct_const.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_smallvector.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapitypes.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "traderesult.hpp"
#include "wallet.hpp"
#include "withdrawinfo.hpp"

namespace cct {
class Exchange;

template <class T>
using ExchangeWith = std::pair<const Exchange *, T>;

using MarketOrderBookConversionRate = std::tuple<std::string_view, MarketOrderBook, std::optional<MonetaryAmount>>;

using MarketOrderBookConversionRates = FixedCapacityVector<MarketOrderBookConversionRate, kNbSupportedExchanges>;

using MarketsPerExchange = FixedCapacityVector<ExchangeWith<MarketSet>, kNbSupportedExchanges>;

using MonetaryAmountPerExchange = FixedCapacityVector<ExchangeWith<MonetaryAmount>, kNbSupportedExchanges>;

using MonetaryAmountByCurrencySetPerExchange =
    FixedCapacityVector<ExchangeWith<MonetaryAmountByCurrencySet>, kNbSupportedExchanges>;

using LastTradesPerExchange = FixedCapacityVector<ExchangeWith<LastTradesVector>, kNbSupportedExchanges>;

using TradeResultPerExchange = SmallVector<ExchangeWith<TradeResult>, kTypicalNbPrivateAccounts>;

using TradedAmountsVectorWithFinalAmountPerExchange =
    SmallVector<ExchangeWith<TradedAmountsVectorWithFinalAmount>, kTypicalNbPrivateAccounts>;

using ExchangeHealthCheckStatus = FixedCapacityVector<ExchangeWith<bool>, kNbSupportedExchanges>;

using ExchangeTickerMaps = FixedCapacityVector<ExchangeWith<MarketOrderBookMap>, kNbSupportedExchanges>;

using CurrenciesPerExchange = FixedCapacityVector<ExchangeWith<CurrencyExchangeFlatSet>, kNbSupportedExchanges>;

using BalancePerExchange = SmallVector<std::pair<Exchange *, BalancePortfolio>, kTypicalNbPrivateAccounts>;

using WalletPerExchange = SmallVector<ExchangeWith<Wallet>, kTypicalNbPrivateAccounts>;

using OpenedOrdersPerExchange = SmallVector<ExchangeWith<OrdersSet>, kTypicalNbPrivateAccounts>;

using DepositsPerExchange = SmallVector<ExchangeWith<DepositsSet>, kTypicalNbPrivateAccounts>;

using WithdrawsPerExchange = SmallVector<ExchangeWith<WithdrawsSet>, kTypicalNbPrivateAccounts>;

using DeliveredWithdrawInfoWithExchanges = std::pair<std::array<const Exchange *, 2>, DeliveredWithdrawInfo>;

using NbCancelledOrdersPerExchange = SmallVector<ExchangeWith<int>, kTypicalNbPrivateAccounts>;

using ConversionPathPerExchange = FixedCapacityVector<ExchangeWith<MarketsPath>, kNbSupportedExchanges>;
}  // namespace cct